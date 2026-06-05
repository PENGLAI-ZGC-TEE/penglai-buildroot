/*
 * sm4_ecb.c - SM4-ECB 分组密码加解密示例 (标量加密指令版)
 *
 * 应用场景:
 *   SM4 是中国国家密码标准 (GM/T 0002-2012), 广泛应用于金融、政务、
 *   物联网和 WAPI 无线安全。
 *
 * 改写说明:
 *   原程序使用向量指令 vsm4k.vi 和 vsm4r.vv,
 *   现已全部替换为标量 SM4 指令:
 *     vsm4k.vi → SM4KS (标量 SM4 密钥扩展)
 *     vsm4r.vv → SM4ED (标量 SM4 加解密轮函数)
 *
 * 编译要求: -march=rv64gc_zks_zbkc
 */

#include "common.h"

/* ================================================================
 * SM4 加密算法概述
 *
 * SM4 是 32 轮非平衡 Feistel 网络:
 *   X[i+4] = X[i] XOR T(X[i+1] XOR X[i+2] XOR X[i+3] XOR rk[i])
 *
 * T 变换: T(x) = L(tau(x))
 *   tau: 4 个并行 8-bit S-Box 替换
 *   L:   32-bit 线性变换:
 *     L(B) = B XOR (B<<<2) XOR (B<<<10) XOR (B<<<18) XOR (B<<<24)
 *
 * 标量指令:
 *   SM4KS rd, rs1, rs2, bs — 密钥扩展, 处理 rs2 中第 bs 字节
 *   SM4ED rd, rs1, rs2, bs — 加解密, 处理 rs2 中第 bs 字节
 *
 * 每条 SM4KS/SM4ED 指令处理一个字节, 4 条指令完成一轮。
 * ================================================================ */

/* SM4 线性变换 L (用于密钥扩展的 L' 不同) */
static inline uint32_t sm4_L(uint32_t B) {
    return B ^ scalar_ror32(B,  2) ^ scalar_ror32(B, 10) ^
                scalar_ror32(B, 18) ^ scalar_ror32(B, 24);
}

/* SM4 密钥扩展线性变换 L' */
static inline uint32_t sm4_Lprime(uint32_t B) {
    return B ^ scalar_ror32(B, 13) ^ scalar_ror32(B, 23);
}

/* ================================================================
 * SM4 密钥扩展 (标量实现)
 *
 * 生成 32 个 32-bit 轮密钥 rk[0..31]:
 *   K[0..3] = MK[0..3] XOR FK[0..3]
 *   rk[i] = K[i+4] = K[i] XOR T'(K[i+1] XOR K[i+2] XOR K[i+3] XOR CK[i])
 *   T'(x) = L'(tau(x))
 *
 * SM4KS 指令加速该过程: 每条 SM4KS 处理一个字节。
 * ================================================================ */

static void sm4_key_schedule(uint32_t rk[32]) {
    uint32_t K[36];

    /* K[0..3] = MK XOR FK */
    for (int i = 0; i < 4; i++)
        K[i] = SM4_TEST_KEY[i] ^ SM4_FK[i];

    /* 生成 K[4..35] 和 rk[0..31] */
    for (int i = 0; i < 32; i++) {
        uint32_t X = K[i + 1] ^ K[i + 2] ^ K[i + 3] ^ SM4_CK[i];
        uint32_t Tp;

#ifdef USE_INLINE_ASM
        /* 使用 SM4KS 指令 (需要 4 次调用, 每次一个字节) */
        uint32_t t0, t1, t2, t3;
        SM4KS(t0, 0, X, 3);  /* byte 3 (MSB) */
        SM4KS(t1, 0, X, 2);  /* byte 2 */
        SM4KS(t2, 0, X, 1);  /* byte 1 */
        SM4KS(t3, 0, X, 0);  /* byte 0 (LSB) */
        Tp = t0 | t1 | t2 | t3;
#else
        /* 纯 C 实现 T': tau(S-Box) + L' */
        uint32_t tau = 0;
        for (int b = 0; b < 4; b++) {
            uint8_t byte = (X >> (24 - b * 8)) & 0xff;
            tau |= (uint32_t)scalar_sm4_sbox[byte] << (24 - b * 8);
        }
        Tp = sm4_Lprime(tau);
#endif

        K[i + 4] = K[i] ^ Tp;
        rk[i] = K[i + 4];
    }
}

/* ================================================================
 * SM4 分组加密 (标量实现)
 *
 * 32 轮 Feistel 变换:
 *   X[i+4] = X[i] XOR T(X[i+1] XOR X[i+2] XOR X[i+3] XOR rk[i])
 *
 * SM4ED 指令加速 T 变换, 每条 SM4ED 处理一个字节。
 * ================================================================ */

static void sm4_encrypt_block(uint32_t output[4],
                               const uint32_t input[4],
                               const uint32_t rk[32]) {
    uint32_t X[36];  /* X[0..35] */

    for (int i = 0; i < 4; i++)
        X[i] = input[i];

    /* 32 轮 */
    for (int i = 0; i < 32; i++) {
        uint32_t Y = X[i + 1] ^ X[i + 2] ^ X[i + 3] ^ rk[i];
        uint32_t T;

#ifdef USE_INLINE_ASM
        /* 使用 SM4ED 指令 */
        uint32_t t0, t1, t2, t3;
        SM4ED(t0, 0, Y, 3);  /* byte 3 (MSB) */
        SM4ED(t1, 0, Y, 2);
        SM4ED(t2, 0, Y, 1);
        SM4ED(t3, 0, Y, 0);  /* byte 0 (LSB) */
        T = t0 | t1 | t2 | t3;
#else
        /* 纯 C 实现 T: tau(S-Box) + L */
        uint32_t tau = 0;
        for (int b = 0; b < 4; b++) {
            uint8_t byte = (Y >> (24 - b * 8)) & 0xff;
            tau |= (uint32_t)scalar_sm4_sbox[byte] << (24 - b * 8);
        }
        T = sm4_L(tau);
#endif

        X[i + 4] = X[i] ^ T;
    }

    /* SM4 输出反序: Y = {X[35], X[34], X[33], X[32]} */
    for (int i = 0; i < 4; i++)
        output[i] = X[35 - i];
}

/* SM4 解密: 轮密钥逆序 (Feistel 网络对称性) */
static void sm4_decrypt_block(uint32_t output[4],
                               const uint32_t input[4],
                               const uint32_t rk[32]) {
    uint32_t rk_rev[32];
    for (int i = 0; i < 32; i++)
        rk_rev[i] = rk[31 - i];
    sm4_encrypt_block(output, input, rk_rev);
}

/* ================================================================
 * SM4-ECB 批量加解密
 * ================================================================ */

static void sm4_ecb_encrypt(uint8_t *out, const uint8_t *in,
                             size_t nbytes, const uint32_t rk[32]) {
    size_t blocks = nbytes / 16;
    for (size_t b = 0; b < blocks; b++) {
        uint32_t block[4];
        memcpy(block, in + b * 16, 16);
        sm4_encrypt_block(block, block, rk);
        memcpy(out + b * 16, block, 16);
    }
}

static void sm4_ecb_decrypt(uint8_t *out, const uint8_t *in,
                             size_t nbytes, const uint32_t rk[32]) {
    size_t blocks = nbytes / 16;
    for (size_t b = 0; b < blocks; b++) {
        uint32_t block[4];
        memcpy(block, in + b * 16, 16);
        sm4_decrypt_block(block, block, rk);
        memcpy(out + b * 16, block, 16);
    }
}

/* ================================================================
 * 主程序
 * ================================================================ */

int main(void) {
    uint32_t rk[32];
    uint32_t ciphertext[4];
    uint32_t decrypted[4];

    print_title("SM4-ECB 分组密码加解密示例 (标量加密指令)");

    /* ===== 步骤 1: 密钥扩展 ===== */
    printf("\n  [步骤1] SM4 密钥扩展\n");
    sm4_key_schedule(rk);

    hexdump_u32("SM4 密钥", SM4_TEST_KEY, 4);
    printf("  生成的轮密钥 (前4轮):\n");
    hexdump_u32("  rk[0..3]", rk,     4);
    hexdump_u32("  rk[4..7]", rk + 4, 4);

    /* ===== 步骤 2: 加密 ===== */
    printf("\n  [步骤2] SM4 单分组加密 (GM/T 测试向量)\n");
    sm4_encrypt_block(ciphertext, SM4_TEST_PLAINTEXT, rk);

    hexdump_u32("明文",      SM4_TEST_PLAINTEXT,  4);
    hexdump_u32("密文",      ciphertext,          4);
    hexdump_u32("期望密文",  SM4_TEST_CIPHERTEXT, 4);

    int enc_ok = mem_eq_u32(ciphertext, SM4_TEST_CIPHERTEXT, 4);
    test_result("SM4 加密 (GM/T 0002-2012 测试向量)", enc_ok);

    /* ===== 步骤 3: 解密 ===== */
    printf("\n  [步骤3] SM4 解密\n");
    sm4_decrypt_block(decrypted, ciphertext, rk);

    hexdump_u32("解密结果", decrypted, 4);
    hexdump_u32("原始明文", SM4_TEST_PLAINTEXT, 4);

    int dec_ok = mem_eq_u32(decrypted, SM4_TEST_PLAINTEXT, 4);
    test_result("SM4 解密往返验证", dec_ok);

    /* ===== 步骤 4: ECB 批量加解密 ===== */
    printf("\n  [步骤4] SM4-ECB 批量加解密\n");

    const char *message = "SM4-ECB Encryption Demo with "
                          "RISC-V Scalar Crypto Extension!";
    size_t msg_len = ((strlen(message) + 15) / 16) * 16;
    uint8_t *pt_buf   = (uint8_t *)calloc(msg_len, 1);
    uint8_t *ct_buf   = (uint8_t *)calloc(msg_len, 1);
    uint8_t *recv_buf = (uint8_t *)calloc(msg_len, 1);
    memcpy(pt_buf, message, strlen(message));

    sm4_ecb_encrypt(ct_buf, pt_buf, msg_len, rk);
    sm4_ecb_decrypt(recv_buf, ct_buf, msg_len, rk);

    printf("  原始消息 : %s\n", message);
    hexdump("密文(前32B)", ct_buf, 32);
    printf("  解密消息 : %s\n", recv_buf);

    int e2e_ok = mem_eq(pt_buf, recv_buf, msg_len);
    test_result("SM4-ECB 批量加解密往返", e2e_ok);

    /* ===== 性能测试 ===== */
    printf("\n  [性能] SM4-ECB 吞吐量测试 (64KB)\n");

    #define SM4_PERF_SIZE (64 * 1024)
    uint8_t *perf_in  = (uint8_t *)malloc(SM4_PERF_SIZE);
    uint8_t *perf_out = (uint8_t *)malloc(SM4_PERF_SIZE);
    memset(perf_in, 0xA5, SM4_PERF_SIZE);

    uint64_t p_t0 = get_time_us();
    sm4_ecb_encrypt(perf_out, perf_in, SM4_PERF_SIZE, rk);
    uint64_t p_t1 = get_time_us();

    double elapsed_ms = (double)(p_t1 - p_t0) / 1000.0;
    double throughput = (double)SM4_PERF_SIZE / (1024.0 * 1024.0) /
                        ((double)(p_t1 - p_t0) / 1000000.0);

    printf("  数据量   : %d bytes (%d blocks)\n",
           SM4_PERF_SIZE, SM4_PERF_SIZE / 16);
    printf("  耗时     : %.3f ms\n", elapsed_ms);
    printf("  吞吐量   : %.2f MB/s\n", throughput);

    free(pt_buf); free(ct_buf); free(recv_buf);
    free(perf_in); free(perf_out);

    /* ===== 总结 ===== */
    printf("\n");
    print_separator();
    printf("  使用指令: SM4KS  (密钥扩展, 每轮4条, 共32轮)\n");
    printf("            SM4ED  (轮函数, 每轮4条, 共32轮)\n");
    printf("  扩展    : Zksed (SM4 分组密码)\n");
    printf("  标准    : GM/T 0002-2012\n");
    print_separator();

    return (enc_ok && dec_ok && e2e_ok) ? 0 : 1;
}
