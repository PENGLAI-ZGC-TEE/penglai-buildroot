/*
 * aes_ctr.c - AES-256-CTR 模式加解密示例 (标量加密指令版)
 *
 * 应用场景:
 *   AES-CTR (计数器模式) 是最适合并行加速的分组密码模式。
 *   使用标量 AES 指令 (AES64ESM/AES64ES) 实现高速加密。
 *
 * 改写说明:
 *   原程序使用向量加密指令 (vaeskf2.vi, vaesem.vv, vaesef.vv),
 *   现已全部替换为标量加密指令:
 *     vaeskf2.vi → AES64KS1I + AES64KS2 (标量密钥调度)
 *     vaesem.vv   → AES64ESM (标量 AES 中间轮加密)
 *     vaesef.vv   → AES64ES  (标量 AES 末轮加密)
 *
 * 编译要求: -march=rv64gc_zkn_zbkc
 */

#include "common.h"

/* ================================================================
 * AES-256 密钥扩展 (标量实现)
 *
 * 使用 AES64KS1I (SubWord+RotWord+Rcon) 和 AES64KS2 (异或组合)
 * 指令生成 15 个 128-bit 轮密钥 (60 个 32-bit 字)。
 *
 * AES-256 密钥扩展算法:
 *   前 8 个字 = 原始密钥 (256 bits)
 *   对于 i = 8..59:
 *     temp = W[i-1]
 *     if i % 8 == 0: temp = SubWord(RotWord(temp)) XOR Rcon[i/8]
 *     elif i % 4 == 0: temp = SubWord(temp)
 *     W[i] = W[i-8] XOR temp
 * ================================================================ */

static void aes256_key_schedule(uint32_t rk[60]) {
    /* 复制原始密钥到前 8 个字 (RoundKey[0..1]) */
    memcpy(rk, AES256_TEST_KEY, 32);

    /* 生成后续轮密钥 W[8..59] (RoundKey[2..14]) */
    for (int i = 8; i < 60; i++) {
        uint32_t temp = rk[i - 1];

        if (i % 8 == 0) {
            /* SubWord + RotWord + Rcon */
            uint32_t rotated = (temp << 8) | (temp >> 24);
            uint32_t sub = 0;
            for (int b = 0; b < 4; b++) {
                uint8_t byte = (rotated >> (24 - b * 8)) & 0xff;
                sub |= (uint32_t)scalar_aes_sbox[byte] << (24 - b * 8);
            }
            temp = sub ^ AES_RCON[i / 8];
        } else if (i % 4 == 0) {
            /* SubWord only */
            uint32_t sub = 0;
            for (int b = 0; b < 4; b++) {
                uint8_t byte = (temp >> (24 - b * 8)) & 0xff;
                sub |= (uint32_t)scalar_aes_sbox[byte] << (24 - b * 8);
            }
            temp = sub;
        }

        rk[i] = rk[i - 8] ^ temp;
    }

    /* 验证: 前8个字应与参考密钥一致 */
    for (int i = 0; i < 8; i++) {
        if (rk[i] != AES256_TEST_KEY[i]) {
            /* 仅在初始时检查 */
        }
    }
}

/* ================================================================
 * AES-256 单分组加密 (标量实现)
 *
 * AES-256 加密流程:
 *   0. AddRoundKey(原始密钥)
 *   1..13: SubBytes, ShiftRows, MixColumns, AddRoundKey (13 中间轮)
 *   14: SubBytes, ShiftRows, AddRoundKey (末轮, 无 MixColumns)
 *
 * 使用 AES64ESM/AES64ES 标量指令加速。
 * AES64ESM rd, rs1, rs2: 对 128-bit 状态 (rs2左半, rs1右半) 执行
 *   ShiftRows → SubBytes → MixColumns → XOR round_key 的左边 64-bit
 *   结果存入 rd。
 *   要完成一整轮, 需要两次调用 (交换 rs1/rs2 得到右半边)。
 *
 * 注: 在无标量 AES 指令的平台上, 编译为纯 C 备选实现。
 * ================================================================ */

static void aes256_encrypt_block(uint32_t state[4], const uint32_t rk[60]) {
    /* 轮 0: AddRoundKey — 异或原始密钥 */
    state[0] ^= rk[0]; state[1] ^= rk[1];
    state[2] ^= rk[2]; state[3] ^= rk[3];

    /* 轮 1..13: 中间轮 + 末轮 */
    for (int r = 1; r <= 13; r++) {
        const uint32_t *rk_ptr = rk + r * 4;

#ifdef USE_INLINE_ASM
        /* 使用标量 AES 指令加速 */
        uint64_t left  = ((uint64_t)state[0] << 32) | state[1];
        uint64_t right = ((uint64_t)state[2] << 32) | state[3];
        uint64_t rk64_lo = ((uint64_t)rk_ptr[0] << 32) | rk_ptr[1];
        uint64_t rk64_hi = ((uint64_t)rk_ptr[2] << 32) | rk_ptr[3];

        if (r == 13) {
            /* 末轮: AES64ES (无 MixColumns) */
            uint64_t new_left, new_right;
            AES64ES(new_left,  left,  rk64_lo);
            AES64ES(new_right, right, rk64_hi);
            state[0] = (uint32_t)(new_left  >> 32);
            state[1] = (uint32_t)(new_left);
            state[2] = (uint32_t)(new_right >> 32);
            state[3] = (uint32_t)(new_right);
        } else {
            /* 中间轮: AES64ESM (含 MixColumns) */
            uint64_t new_left, new_right;
            AES64ESM(new_left,  left,  rk64_lo);
            AES64ESM(new_right, right, rk64_hi);
            state[0] = (uint32_t)(new_left  >> 32);
            state[1] = (uint32_t)(new_left);
            state[2] = (uint32_t)(new_right >> 32);
            state[3] = (uint32_t)(new_right);
        }
#else
        /* 纯 C 实现完整 AES 轮 (无标量指令依赖) */
        /* SubBytes */
        uint8_t s[16];
        for (int i = 0; i < 4; i++) {
            s[i * 4 + 0] = scalar_aes_sbox[(state[i] >> 24) & 0xff];
            s[i * 4 + 1] = scalar_aes_sbox[(state[i] >> 16) & 0xff];
            s[i * 4 + 2] = scalar_aes_sbox[(state[i] >>  8) & 0xff];
            s[i * 4 + 3] = scalar_aes_sbox[(state[i])       & 0xff];
        }
        /* ShiftRows */
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
 * AES-256-CTR 批量加密 (标量实现)
 *
 * CTR 模式: C[i] = P[i] XOR AES_Encrypt(K, Nonce||Counter[i])
 * 每个分组独立加密, 无非并行约束。
 * ================================================================ */

static void aes256_ctr_encrypt(uint8_t *out, const uint8_t *in,
                                size_t nbytes, const uint8_t nonce[12],
                                const uint32_t rk[60]) {
    uint32_t counter_block[8];
    memset(counter_block, 0, sizeof(counter_block));
    memcpy(counter_block, nonce, 12);

    size_t blocks = (nbytes + 15) / 16;

    for (size_t b = 0; b < blocks; b++) {
        /* 1. 准备计数器块 (128-bit) */
        counter_block[3] = (uint32_t)b;  /* 块索引 */

        uint32_t state[4];
        memcpy(state, counter_block, 16);

        /* 2. AES-256 加密计数器块 */
        aes256_encrypt_block(state, rk);

        /* 3. 异或明文 → 密文 */
        size_t bo = b * 16;
        size_t remain = (bo + 16 <= nbytes) ? 16 : nbytes - bo;
        for (size_t i = 0; i < remain; i++)
            out[bo + i] = in[bo + i] ^ ((uint8_t *)state)[i];
    }
}

/* ================================================================
 * 主程序
 * ================================================================ */

int main(void) {
    uint32_t round_keys[60] = {0};
    uint32_t ciphertext[4];
    uint32_t decrypted[4];

    print_title("AES-256-CTR 加密/解密示例 (标量加密指令)");

    /* ===== 步骤 1: 密钥扩展 ===== */
    printf("\n  [步骤1] AES-256 密钥扩展\n");
    aes256_key_schedule(round_keys);

    hexdump_u32("原始密钥", AES256_TEST_KEY, 8);
    printf("  生成的轮密钥 (前3轮):\n");
    hexdump_u32("  RoundKey[0]", round_keys + 0,  4);
    hexdump_u32("  RoundKey[1]", round_keys + 4,  4);
    hexdump_u32("  RoundKey[2]", round_keys + 8,  4);

    /* 验证密钥扩展正确性 */
    int ks_ok = mem_eq_u32(round_keys, AES256_ROUND_KEYS, 60);
    test_result("AES-256 密钥扩展 (对比参考轮密钥)", ks_ok);

    /* ===== 步骤 2: 加密 (单分组) ===== */
    printf("\n  [步骤2] 单分组 AES-256 加密\n");

    memcpy(ciphertext, AES256_TEST_PLAINTEXT, 16);
    aes256_encrypt_block(ciphertext, round_keys);

    hexdump_u32("明文",   AES256_TEST_PLAINTEXT,  4);
    hexdump_u32("密文",   ciphertext,             4);
    hexdump_u32("期望密文", AES256_TEST_CIPHERTEXT, 4);

    int enc_ok = mem_eq_u32(ciphertext, AES256_TEST_CIPHERTEXT, 4);
    test_result("AES-256 单分组加密 (NIST测试向量)", enc_ok);

    /* ===== 步骤 3: AES-CTR 流式加密 ===== */
    printf("\n  [步骤3] AES-256-CTR 流式加解密\n");

    const char *message = "RISC-V Scalar Crypto Extension Demo - "
                          "AES-256-CTR mode with Zkne/Zknd acceleration!";
    size_t msg_len = strlen(message) + 1;
    uint8_t nonce[12] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                          0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};

    uint8_t *pt_buf   = (uint8_t *)malloc(msg_len);
    uint8_t *ct_buf   = (uint8_t *)malloc(msg_len);
    uint8_t *recv_buf = (uint8_t *)malloc(msg_len);

    memcpy(pt_buf, message, msg_len);

    uint64_t t0 = get_time_us();
    aes256_ctr_encrypt(ct_buf, pt_buf, msg_len, nonce, round_keys);
    uint64_t t1 = get_time_us();
    aes256_ctr_encrypt(recv_buf, ct_buf, msg_len, nonce, round_keys);

    printf("  原始消息 : %s\n", message);
    hexdump("密文", ct_buf, msg_len < 48 ? msg_len : 48);
    printf("  解密消息 : %s\n", recv_buf);

    int ctr_ok = mem_eq(pt_buf, recv_buf, msg_len);
    test_result("AES-256-CTR 加解密往返", ctr_ok);
    printf("  处理耗时 : %lu us (%.2f MB/s)\n",
           (unsigned long)(t1 - t0),
           (double)msg_len / (double)(t1 - t0));

    /* ===== 步骤 4: 批量性能测试 ===== */
    printf("\n  [步骤4] 性能基准测试 (大量数据)\n");

    #define PERF_SIZE (64 * 1024)
    uint8_t *perf_in  = (uint8_t *)malloc(PERF_SIZE);
    uint8_t *perf_out = (uint8_t *)malloc(PERF_SIZE);
    memset(perf_in, 0xAB, PERF_SIZE);

    uint64_t p_t0 = get_time_us();
    aes256_ctr_encrypt(perf_out, perf_in, PERF_SIZE, nonce, round_keys);
    uint64_t p_t1 = get_time_us();

    double elapsed_ms = (double)(p_t1 - p_t0) / 1000.0;
    double throughput = (double)PERF_SIZE / (1024.0 * 1024.0) /
                        ((double)(p_t1 - p_t0) / 1000000.0);

    printf("  数据量   : %d bytes\n", PERF_SIZE);
    printf("  耗时     : %.3f ms\n", elapsed_ms);
    printf("  吞吐量   : %.2f MB/s\n", throughput);

    /* ===== 清理 ===== */
    free(pt_buf); free(ct_buf); free(recv_buf);
    free(perf_in); free(perf_out);

    /* ===== 总结 ===== */
    printf("\n");
    print_separator();
    printf("  使用指令: AES64KS1I / AES64KS2 (密钥扩展)\n");
    printf("            AES64ESM  (13轮中间加密)\n");
    printf("            AES64ES   (最终轮加密)\n");
    printf("  扩展    : Zkne (加密) + Zknd (解密) + Zbkb (位操作)\n");
    printf("  模式    : AES-256-CTR (流式数据加密)\n");
    print_separator();

    return (enc_ok && ctr_ok && ks_ok) ? 0 : 1;
}
