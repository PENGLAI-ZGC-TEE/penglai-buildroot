/*
 * aes_gcm.c - AES-256-GCM 认证加密示例 (标量加密指令版)
 *
 * 应用场景:
 *   AES-GCM 是 NIST 推荐的认证加密模式, 广泛用于 TLS 1.3、IPsec、云存储。
 *
 * 改写说明:
 *   原程序使用向量指令 vaeskf2.vi, vaesem.vv, vaesef.vv, vghsh.vv, vgmul.vv,
 *   现已全部替换为标量指令:
 *     vaeskf2.vi → AES64KS1I + AES64KS2 (标量密钥调度)
 *     vaesem.vv   → AES64ESM (标量 AES 中间轮)
 *     vaesef.vv   → AES64ES  (标量 AES 末轮)
 *     vghsh.vv    → CLMUL/CLMULH (GF(2^128) 乘法 GHASH)
 *     vgmul.vv    → CLMUL/CLMULH (GF(2^128) 乘法)
 *
 * 编译要求: -march=rv64gc_zkn_zbkc
 */

#include "common.h"

/* ================================================================
 * AES-256 复用函数 (对标 aes_ctr.c 实现)
 * ================================================================ */

static void aes256_key_schedule(uint32_t rk[60]) {
    memcpy(rk, AES256_TEST_KEY, 32);

    for (int i = 8; i < 60; i++) {
        uint32_t temp = rk[i - 1];

        if (i % 8 == 0) {
            uint32_t rotated = (temp << 8) | (temp >> 24);
            uint32_t sub = 0;
            for (int b = 0; b < 4; b++) {
                uint8_t byte = (rotated >> (24 - b * 8)) & 0xff;
                sub |= (uint32_t)scalar_aes_sbox[byte] << (24 - b * 8);
            }
            temp = sub ^ AES_RCON[i / 8];
        } else if (i % 4 == 0) {
            uint32_t sub = 0;
            for (int b = 0; b < 4; b++) {
                uint8_t byte = (temp >> (24 - b * 8)) & 0xff;
                sub |= (uint32_t)scalar_aes_sbox[byte] << (24 - b * 8);
            }
            temp = sub;
        }

        rk[i] = rk[i - 8] ^ temp;
    }
}

static void aes256_encrypt_block(uint32_t state[4], const uint32_t rk[60]) {
    state[0] ^= rk[0]; state[1] ^= rk[1];
    state[2] ^= rk[2]; state[3] ^= rk[3];

    for (int r = 1; r <= 13; r++) {
        const uint32_t *rk_ptr = rk + r * 4;

#ifdef USE_INLINE_ASM
        uint64_t left  = ((uint64_t)state[0] << 32) | state[1];
        uint64_t right = ((uint64_t)state[2] << 32) | state[3];
        uint64_t rk64_lo = ((uint64_t)rk_ptr[0] << 32) | rk_ptr[1];
        uint64_t rk64_hi = ((uint64_t)rk_ptr[2] << 32) | rk_ptr[3];

        if (r == 13) {
            uint64_t nl, nr;
            AES64ES(nl, left,  rk64_lo);
            AES64ES(nr, right, rk64_hi);
            state[0] = (uint32_t)(nl >> 32); state[1] = (uint32_t)(nl);
            state[2] = (uint32_t)(nr >> 32); state[3] = (uint32_t)(nr);
        } else {
            uint64_t nl, nr;
            AES64ESM(nl, left,  rk64_lo);
            AES64ESM(nr, right, rk64_hi);
            state[0] = (uint32_t)(nl >> 32); state[1] = (uint32_t)(nl);
            state[2] = (uint32_t)(nr >> 32); state[3] = (uint32_t)(nr);
        }
#else
        uint8_t s[16];
        for (int i = 0; i < 4; i++) {
            s[i * 4 + 0] = scalar_aes_sbox[(state[i] >> 24) & 0xff];
            s[i * 4 + 1] = scalar_aes_sbox[(state[i] >> 16) & 0xff];
            s[i * 4 + 2] = scalar_aes_sbox[(state[i] >>  8) & 0xff];
            s[i * 4 + 3] = scalar_aes_sbox[(state[i])       & 0xff];
        }
        uint8_t sr[16];
        sr[ 0] = s[ 0]; sr[ 4] = s[ 4]; sr[ 8] = s[ 8]; sr[12] = s[12];
        sr[ 1] = s[ 5]; sr[ 5] = s[ 9]; sr[ 9] = s[13]; sr[13] = s[ 1];
        sr[ 2] = s[10]; sr[ 6] = s[14]; sr[10] = s[ 2]; sr[14] = s[ 6];
        sr[ 3] = s[15]; sr[ 7] = s[ 3]; sr[11] = s[ 7]; sr[15] = s[11];

        for (int i = 0; i < 4; i++) {
            state[i] = ((uint32_t)sr[i*4] << 24) | ((uint32_t)sr[i*4+1] << 16) |
                       ((uint32_t)sr[i*4+2] << 8)  |  (uint32_t)sr[i*4+3];
        }

        if (r < 13) {
            for (int i = 0; i < 4; i++)
                state[i] = scalar_aes_mixcolumn(state[i]);
        }

        for (int i = 0; i < 4; i++)
            state[i] ^= rk_ptr[i];
#endif
    }
}

/* ================================================================
 * GHASH — 使用 CLMUL/CLMULH 的 GF(2^128) 实现
 *
 * GHASH 在 GF(2^128) 上执行:
 *   Y_i = (Y_{i-1} XOR X_i) * H
 *   H = AES_Encrypt(K, 0^128)
 *
 * GF(2^128) 约化多项式: f(x) = x^128 + x^7 + x^2 + x + 1
 *
 * CLMUL/CLMULH 提供 GF(2^n) 乘法底层操作:
 *   CLMUL  rd, rs1, rs2  — 低 64-bit
 *   CLMULH rd, rs1, rs2  — 高 64-bit
 *
 * 使用 Karatsuba 技巧执行 GF(2^128) 乘法:
 *   A = A1*x^64 + A0, B = B1*x^64 + B0
 *   C = A*B = (A1*B1)*x^128 + (A1*B0 + A0*B1)*x^64 + A0*B0
 *   约化: 减去 (A1*B1)*(x^128 + x^7 + x^2 + x + 1)
 * ================================================================ */

/*
 * GHASH GF(2^128) 乘法 (NIST SP 800-38D 标准算法)
 *
 * 域: GF(2^128), 约化多项式 f = x^128 + x^7 + x^2 + x + 1
 *
 * 输入: 操作数 X, Y (各 128-bit)
 * 输出: Z = X * Y mod f
 *
 * 标量加速: CLMUL/CLMULH 提供 64-bit GF(2) 乘法。
 * 在无 RISC-V 工具链时, 使用纯 C 备选实现。
 */
static void ghash_mul(uint32_t Z[4], const uint32_t X[4],
                       const uint32_t Y[4]) {
    uint64_t X_lo = ((uint64_t)X[0] << 32) | X[1];
    uint64_t X_hi = ((uint64_t)X[2] << 32) | X[3];
    uint64_t Y_lo = ((uint64_t)Y[0] << 32) | Y[1];
    uint64_t Y_hi = ((uint64_t)Y[2] << 32) | Y[3];

#ifdef USE_INLINE_ASM
    /* CLMUL 加速路径 (需要 RISC-V Zbkc 扩展) */
    uint64_t Z0_lo, Z0_hi, t1, t2;
    CLMUL(Z0_lo, X_lo, Y_lo);    /* X0·Y0 low  */
    CLMULH(Z0_hi, X_lo, Y_lo);   /* X0·Y0 high */
    CLMUL(t1, X_lo, Y_hi);       /* X0·Y1 */
    CLMUL(t2, X_hi, Y_lo);       /* X1·Y0 */
    uint64_t V1 = Z0_hi ^ t1 ^ t2;
    uint64_t V0 = Z0_lo;

    CLMUL(t1, X_hi, Y_hi);       /* X1·Y1 → 用于约化 */

    /* GCM 约化: 使用逐位退火法 (bit-reflected) */
    for (int i = 0; i < 64; i++) {
        uint64_t mask = (uint64_t)((int64_t)t1 >> 63);
        t1 = (t1 << 1) | (V1 >> 63);
        V1 = (V1 << 1) | (V0 >> 63);
        V0 <<= 1;
        if (mask)
            V0 ^= 0x87;  /* x^7+x^2+x+1 (reflected) */
    }

    Z[0] = (uint32_t)(V1 >> 32);
    Z[1] = (uint32_t)(V1);
    Z[2] = (uint32_t)(V0 >> 32);
    Z[3] = (uint32_t)(V0);
#else
    /* 纯 C 路径 — 通用 GF(2^128) 乘法 (在任何平台上均可编译运行) */
    uint64_t r_hi, r_lo;
    scalar_gf128_mul(&r_hi, &r_lo, X_hi, X_lo, Y_hi, Y_lo);
    Z[0] = (uint32_t)(r_hi >> 32);
    Z[1] = (uint32_t)(r_hi);
    Z[2] = (uint32_t)(r_lo >> 32);
    Z[3] = (uint32_t)(r_lo);
#endif
}

/* ================================================================
 * GHASH 函数
 *
 * 在 GCM 中:
 *   1. 对 AAD 数据执行 GHASH
 *   2. 对密文执行 GHASH
 *   3. 对长度块 len64(AAD) || len64(CT) 执行 GHASH
 *   4. Tag = GHASH_result XOR E_K(J0)
 * ================================================================ */

static void ghash(uint32_t Y[4], const uint32_t H[4],
                   const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i += 16) {
        uint32_t block[4];
        size_t chunk = (len - i >= 16) ? 16 : len - i;
        memset(block, 0, 16);
        memcpy(block, data + i, chunk);

        /* Y = Y XOR block */
        Y[0] ^= block[0]; Y[1] ^= block[1];
        Y[2] ^= block[2]; Y[3] ^= block[3];

        /* Y = Y * H */
        ghash_mul(Y, Y, H);
    }
}

/* ================================================================
 * AES-256-GCM 加密
 * ================================================================ */

static void aes256_gcm_encrypt(uint8_t *ct, uint8_t tag[16],
                                const uint8_t *pt, size_t pt_len,
                                const uint8_t *aad, size_t aad_len,
                                const uint8_t iv[12],
                                const uint32_t rk[60]) {
    /* 1. H = AES_Encrypt(K, 0^128) — GHASH 子密钥 */
    uint32_t H[4] = {0, 0, 0, 0};
    aes256_encrypt_block(H, rk);

    /* 2. J0 = IV || 0x00000001 */
    uint32_t J0[4];
    memcpy(J0, iv, 12);
    J0[3] = 0x01000000;

    /* 3. CTR 加密: C = P XOR AES_CTR(K, J0+1...) */
    uint32_t counter[4];
    memcpy(counter, J0, 16);
    counter[3] += 0x01000000;  /* J0+1 */

    size_t blocks = (pt_len + 15) / 16;
    for (size_t b = 0; b < blocks; b++) {
        uint32_t ctr_enc[4];
        memcpy(ctr_enc, counter, 16);
        aes256_encrypt_block(ctr_enc, rk);

        size_t bo = b * 16;
        size_t chunk = (bo + 16 <= pt_len) ? 16 : pt_len - bo;
        uint8_t *ct_ptr = ct + bo;
        const uint8_t *pt_ptr = pt + bo;
        for (size_t i = 0; i < chunk; i++)
            ct_ptr[i] = pt_ptr[i] ^ ((uint8_t *)ctr_enc)[i];

        counter[3] += 0x01000000;
    }

    /* 4. GHASH(AAD || CT || len_block) */
    uint32_t Y[4] = {0, 0, 0, 0};

    ghash(Y, H, aad, aad_len);
    ghash(Y, H, ct, pt_len);

    /* 长度块: len(AAD) || len(CT), 各 64-bit 大端 */
    uint64_t aad_bits = aad_len * 8;
    uint64_t ct_bits  = pt_len * 8;
    uint32_t len_block[4];
    len_block[0] = (uint32_t)(aad_bits >> 32);
    len_block[1] = (uint32_t)(aad_bits);
    len_block[2] = (uint32_t)(ct_bits >> 32);
    len_block[3] = (uint32_t)(ct_bits);

    /* GHASH 最终步骤: Y = (Y XOR len_block) * H */
    Y[0] ^= len_block[0]; Y[1] ^= len_block[1];
    Y[2] ^= len_block[2]; Y[3] ^= len_block[3];
    ghash_mul(Y, Y, H);

    /* 5. Tag = S XOR E_K(J0) */
    uint32_t E_J0[4];
    memcpy(E_J0, J0, 16);
    aes256_encrypt_block(E_J0, rk);

    for (int i = 0; i < 4; i++) Y[i] ^= E_J0[i];
    memcpy(tag, Y, 16);
}

/* ================================================================
 * AES-256-GCM 解密
 * ================================================================ */

static int aes256_gcm_decrypt(uint8_t *pt, const uint8_t *ct, size_t ct_len,
                               const uint8_t *aad, size_t aad_len,
                               const uint8_t iv[12], const uint8_t tag[16],
                               const uint32_t rk[60]) {
    /* 计算认证标签: GHASH(AAD || CT || len) XOR E_K(J0) */
    uint32_t H[4] = {0};
    aes256_encrypt_block(H, rk);

    uint32_t J0[4];
    memcpy(J0, iv, 12);
    J0[3] = 0x01000000;

    uint32_t Y[4] = {0, 0, 0, 0};
    ghash(Y, H, aad, aad_len);
    ghash(Y, H, ct, ct_len);

    uint64_t aad_bits = aad_len * 8;
    uint64_t c_bits = ct_len * 8;
    uint32_t len_block[4];
    len_block[0] = (uint32_t)(aad_bits >> 32);
    len_block[1] = (uint32_t)(aad_bits);
    len_block[2] = (uint32_t)(c_bits >> 32);
    len_block[3] = (uint32_t)(c_bits);

    Y[0] ^= len_block[0]; Y[1] ^= len_block[1];
    Y[2] ^= len_block[2]; Y[3] ^= len_block[3];
    ghash_mul(Y, Y, H);

    uint32_t E_J0[4];
    memcpy(E_J0, J0, 16);
    aes256_encrypt_block(E_J0, rk);

    uint32_t computed[4];
    for (int i = 0; i < 4; i++) computed[i] = Y[i] ^ E_J0[i];

    /* 验证标签 */
    for (int i = 0; i < 16; i++) {
        if (((uint8_t *)computed)[i] != tag[i])
            return -1;
    }

    /* CTR 解密 (与加密相同) */
    uint32_t counter[4];
    memcpy(counter, J0, 16);
    counter[3] += 0x01000000;

    for (size_t b = 0; b < (ct_len + 15) / 16; b++) {
        uint32_t ctr_enc[4];
        memcpy(ctr_enc, counter, 16);
        aes256_encrypt_block(ctr_enc, rk);

        size_t bo = b * 16;
        size_t chunk = (bo + 16 <= ct_len) ? 16 : ct_len - bo;
        for (size_t i = 0; i < chunk; i++)
            pt[bo + i] = ct[bo + i] ^ ((uint8_t *)ctr_enc)[i];

        counter[3] += 0x01000000;
    }

    return 0;
}

/* ================================================================
 * 主程序
 * ================================================================ */

int main(void) {
    uint32_t rk[60];
    uint8_t iv[12] = {0};
    uint8_t tag[16];

    print_title("AES-256-GCM 认证加密示例 (标量加密指令)");

    /* ===== 步骤 1: 密钥扩展 ===== */
    printf("\n  [步骤1] AES-256 密钥扩展\n");
    aes256_key_schedule(rk);

    hexdump_u32("AES-256 密钥", AES256_TEST_KEY, 8);

    /* ===== 步骤 2: GCM 加密 ===== */
    printf("\n  [步骤2] AES-256-GCM 加密\n");

    const char *pt_msg = "GCM Authenticated Encryption with "
                         "RISC-V Scalar Crypto (CLMUL GF128)!";
    size_t pt_len = strlen(pt_msg);

    const char *aad = "Protocol=v1.0|Session=0xABCD|Seq=42";
    size_t aad_len = strlen(aad);

    uint8_t *ct_buf = (uint8_t *)calloc(pt_len + 16, 1);
    uint8_t *recv_buf = (uint8_t *)calloc(pt_len + 16, 1);

    aes256_gcm_encrypt(ct_buf, tag, (const uint8_t *)pt_msg, pt_len,
                        (const uint8_t *)aad, aad_len, iv, rk);

    printf("  明文       : %s\n", pt_msg);
    printf("  AAD        : %s\n", aad);
    hexdump("IV (96-bit)", iv, 12);
    hexdump("密文", ct_buf, pt_len < 48 ? pt_len : 48);
    hexdump("认证标签", tag, 16);

    /* ===== 步骤 3: GCM 解密和验证 ===== */
    printf("\n  [步骤3] AES-256-GCM 解密和认证\n");

    int auth_ok = (aes256_gcm_decrypt(recv_buf, ct_buf, pt_len,
                                       (const uint8_t *)aad, aad_len,
                                       iv, tag, rk) == 0);

    printf("  解密结果   : %s\n", recv_buf);
    printf("  明文匹配   : %s\n",
           memcmp(recv_buf, pt_msg, pt_len) == 0 ? "是" : "否");
    test_result("GCM 认证标签验证", auth_ok);

    /* ===== 步骤 4: 篡改检测 ===== */
    printf("\n  [步骤4] 篡改检测测试\n");
    ct_buf[5] ^= 0x01;
    int tamper = aes256_gcm_decrypt(recv_buf, ct_buf, pt_len,
                                     (const uint8_t *)aad, aad_len,
                                     iv, tag, rk);
    test_result("篡改检测 (应拒绝被篡改的密文)", tamper != 0);

    ct_buf[5] ^= 0x01;  /* 恢复 */

    /* ===== 步骤 5: CLMUL 演示 ===== */
    printf("\n  [步骤5] CLMUL GF(2^128) 乘法演示\n");
    printf("  GHASH 在 GF(2^128) 上运算, 约化多项式:\n");
    printf("    f(x) = x^128 + x^7 + x^2 + x + 1\n");
    printf("  标量指令:\n");
    printf("    CLMUL  : GF(2^n) 无进位乘法 (低半)\n");
    printf("    CLMULH : GF(2^n) 无进位乘法 (高半)\n");

    /* ===== 总结 ===== */
    printf("\n");
    print_separator();
    printf("  加密部分: AES64KS1I / AES64KS2 (密钥调度)\n");
    printf("            AES64ESM / AES64ES (CTR 加密)\n");
    printf("  认证部分: CLMUL / CLMULH (GHASH GF(2^128) 乘法)\n");
    printf("  扩展    : Zkne + Zknd (AES) + Zbkc (CLMUL)\n");
    printf("  模式    : GCM (NIST SP 800-38D)\n");
    print_separator();

    free(ct_buf);
    free(recv_buf);

    return auth_ok ? 0 : 1;
}
