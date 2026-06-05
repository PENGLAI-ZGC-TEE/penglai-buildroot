/*
 * sm3.c - SM3 密码杂凑算法示例 (标量加密指令版)
 *
 * 应用场景:
 *   SM3 是中国国家密码标准 (GM/T 0004-2012), 广泛应用于数字签名、
 *   消息认证和区块链。
 *
 * 改写说明:
 *   原程序使用向量指令 vsm3me.vv 和 vsm3c.vi,
 *   现已替换为标量 SM3 指令和纯 C 实现:
 *     vsm3me.vv → SM3P0/SM3P1 + 标量消息扩展算法
 *     vsm3c.vi  → 标量 SM3 压缩函数
 *
 * 编译要求: -march=rv64gc_zks_zbkc
 */

#include "common.h"

/* ================================================================
 * SM3 算法概述
 *
 * 与 SHA-256 类似, SM3 处理 512-bit 数据块, 输出 256-bit 摘要。
 * 但消息扩展更复杂 (132 个字 vs 64 个字)。
 *
 * 消息扩展:
 *   W[0..15] 来自输入块, 大端序
 *   W[16..67]: W[j] = P1(W[j-16] ^ W[j-9] ^ ROL(W[j-3], 15)) ^ ROL(W[j-13], 7) ^ W[j-6]
 *   W'[0..63]: W'[j] = W[j] ^ W[j+4]
 *
 * 标量指令:
 *   SM3P0: P0(x) = x ^ ROL(x, 9) ^ ROL(x, 17) — 压缩函数用
 *   SM3P1: P1(x) = x ^ ROL(x, 15) ^ ROL(x, 23) — 消息扩展用
 * ================================================================ */

/* SM3 消息扩展 — P1 置换 (对标 SM3P1 指令) */
static inline uint32_t sm3_P1(uint32_t x) {
#ifdef USE_INLINE_ASM
    uint32_t ret;
    SM3P1(ret, x);
    return ret;
#else
    return x ^ scalar_ror32(x, 15) ^ scalar_ror32(x, 23);
#endif
}

/* SM3 压缩 — P0 置换 (对标 SM3P0 指令) */
static inline uint32_t sm3_P0(uint32_t x) {
#ifdef USE_INLINE_ASM
    uint32_t ret;
    SM3P0(ret, x);
    return ret;
#else
    return x ^ scalar_ror32(x, 9) ^ scalar_ror32(x, 17);
#endif
}

/* ================================================================
 * SM3 压缩函数常量 T[j]
 *   T[j] = 0x79cc4519  (0 <= j <= 15)
 *   T[j] = 0x7a879d8a  (16 <= j <= 63)
 * ================================================================ */

static inline uint32_t sm3_T(int j) {
    return (j < 16) ? 0x79cc4519u : 0x7a879d8au;
}

/* SM3 布尔函数 */
static inline uint32_t sm3_FF(int j, uint32_t X, uint32_t Y, uint32_t Z) {
    if (j < 16)
        return X ^ Y ^ Z;
    else
        return (X & Y) | (X & Z) | (Y & Z);
}

static inline uint32_t sm3_GG(int j, uint32_t X, uint32_t Y, uint32_t Z) {
    if (j < 16)
        return X ^ Y ^ Z;
    else
        return (X & Y) | ((~X) & Z);
}

/* ================================================================
 * SM3 消息扩展 (标量实现)
 *
 * W[0..15] 直接从输入块转换 (大端序)
 * W[16..67]: W[j] = P1(W[j-16] ^ W[j-9] ^ ROL(W[j-3], 15)) ^ ROL(W[j-13], 7) ^ W[j-6]
 * W'[0..63]: W'[j] = W[j] ^ W[j+4]
 * ================================================================ */

static void sm3_msg_expand(uint32_t W[68], uint32_t W_prime[64],
                            const uint8_t *block) {
    /* W[0..15] — 大端序加载 */
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i * 4 + 0] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8)  |
               ((uint32_t)block[i * 4 + 3]);
    }

    /* W[16..67] — 扩展 */
    for (int j = 16; j < 68; j++) {
        uint32_t t = W[j - 16] ^ W[j - 9] ^ scalar_ror32(W[j - 3], 15);
        W[j] = sm3_P1(t) ^ scalar_ror32(W[j - 13], 7) ^ W[j - 6];
    }

    /* W'[0..63] */
    for (int j = 0; j < 64; j++)
        W_prime[j] = W[j] ^ W[j + 4];
}

/* ================================================================
 * SM3 压缩函数 (标量实现)
 *
 * 64 轮压缩, 使用 8 个工作变量:
 *   A,B,C,D,E,F,G,H (初始 = V[0..7])
 *
 * 轮函数:
 *   SS1 = ROL(ROL(A,12) + E + ROL(T[j], j), 7)
 *   SS2 = SS1 ^ ROL(A, 12)
 *   TT1 = FF(A,B,C) + D + SS2 + W'[j]
 *   TT2 = GG(E,F,G) + H + SS1 + W[j]
 *   D=C, C=ROL(B,9), B=A, A=TT1
 *   H=G, G=ROL(F,19), F=E, E=P0(TT2)
 * ================================================================ */

static void sm3_compress(uint32_t V[8], const uint32_t W[68],
                          const uint32_t W_prime[64]) {
    uint32_t A = V[0], B = V[1], C = V[2], D = V[3];
    uint32_t E = V[4], F = V[5], G = V[6], H = V[7];

    for (int j = 0; j < 64; j++) {
        uint32_t SS1 = scalar_ror32(scalar_ror32(A, 12) + E +
                                     scalar_ror32(sm3_T(j), j), 7);
        uint32_t SS2 = SS1 ^ scalar_ror32(A, 12);
        uint32_t TT1 = sm3_FF(j, A, B, C) + D + SS2 + W_prime[j];
        uint32_t TT2 = sm3_GG(j, E, F, G) + H + SS1 + W[j];

        D = C;
        C = scalar_ror32(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = scalar_ror32(F, 19);
        F = E;
        E = sm3_P0(TT2);
    }

    /* SM3 使用 XOR (而非 ADD) 更新哈希值 */
    V[0] ^= A;  V[1] ^= B;  V[2] ^= C;  V[3] ^= D;
    V[4] ^= E;  V[5] ^= F;  V[6] ^= G;  V[7] ^= H;
}

/* ================================================================
 * SM3 完整哈希函数
 * ================================================================ */

static void sm3_hash(const uint8_t *msg, size_t len, uint32_t digest[8]) {
    memcpy(digest, SM3_IV, 32);

    /* 填充 (与 SHA-256 模式相同) */
    uint64_t bit_len = (uint64_t)len * 8;
    size_t padded_len = ((len + 8 + 64) / 64) * 64;
    uint8_t *padded = (uint8_t *)calloc(padded_len, 1);
    memcpy(padded, msg, len);
    padded[len] = 0x80;

    for (int i = 0; i < 8; i++)
        padded[padded_len - 8 + i] = (uint8_t)(bit_len >> (56 - i * 8));

    /* 逐块处理 */
    for (size_t off = 0; off < padded_len; off += 64) {
        uint32_t W[68]      = {0};
        uint32_t W_prime[64] = {0};

        sm3_msg_expand(W, W_prime, padded + off);
        sm3_compress(digest, W, W_prime);
    }

    free(padded);
}

/* ================================================================
 * 主程序
 * ================================================================ */

int main(void) {
    uint32_t digest[8];

    print_title("SM3 密码杂凑算法示例 (标量加密指令)");

    /* ===== 测试 1: "abc" ===== */
    printf("\n  [测试1] SM3 标准测试向量: \"abc\"\n");

    sm3_hash(SM3_TEST_INPUT, SM3_TEST_INPUT_LEN, digest);

    hexdump("输入", SM3_TEST_INPUT, SM3_TEST_INPUT_LEN);
    hexdump_u32("SM3(\"abc\")", digest, 8);
    hexdump_u32("期望摘要", SM3_TEST_DIGEST, 8);

    int test1_ok = mem_eq_u32(digest, SM3_TEST_DIGEST, 8);
    test_result("SM3(\"abc\") GM/T 0004-2012 测试向量", test1_ok);

    /* ===== 测试 2: 空消息 ===== */
    printf("\n  [测试2] SM3 空消息\n");
    sm3_hash((const uint8_t *)"", 0, digest);
    hexdump_u32("SM3(\"\")", digest, 8);

    /* ===== 测试 3: 长消息 ===== */
    printf("\n  [测试3] SM3 长消息 (512 bits)\n");
    const char *long_msg =
        "abcdabcdabcdabcdabcdabcdabcdabcd"
        "abcdabcdabcdabcdabcdabcdabcdabcd";
    sm3_hash((const uint8_t *)long_msg, 64, digest);
    hexdump_u32("SM3(64-byte)", digest, 8);

    /* ===== 性能测试 ===== */
    printf("\n  [性能] SM3 吞吐量测试 (1MB)\n");

    #define SM3_PERF_SIZE (1024 * 1024)
    uint8_t *perf_buf = (uint8_t *)malloc(SM3_PERF_SIZE);
    memset(perf_buf, 0x33, SM3_PERF_SIZE);

    uint64_t p_t0 = get_time_us();
    sm3_hash(perf_buf, SM3_PERF_SIZE, digest);
    uint64_t p_t1 = get_time_us();

    double elapsed_ms = (double)(p_t1 - p_t0) / 1000.0;
    double throughput = (double)SM3_PERF_SIZE / (1024.0 * 1024.0) /
                        ((double)(p_t1 - p_t0) / 1000000.0);

    printf("  数据量   : %d bytes\n", SM3_PERF_SIZE);
    printf("  耗时     : %.3f ms\n", elapsed_ms);
    printf("  吞吐量   : %.2f MB/s\n", throughput);

    free(perf_buf);

    /* ===== 总结 ===== */
    printf("\n");
    print_separator();
    printf("  使用指令: SM3P0 (压缩函数 P0 置换)\n");
    printf("            SM3P1 (消息扩展 P1 置换)\n");
    printf("            ROR/RORI (循环移位, Zbkb)\n");
    printf("  扩展    : Zksh (SM3 密码杂凑) + Zbkb (位操作)\n");
    printf("  标准    : GM/T 0004-2012\n");
    print_separator();

    return test1_ok ? 0 : 1;
}
