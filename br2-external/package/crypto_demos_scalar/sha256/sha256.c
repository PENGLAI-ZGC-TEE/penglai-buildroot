/*
 * sha256.c - SHA-256 安全哈希示例 (标量加密指令版)
 *
 * 应用场景:
 *   SHA-256 是应用最广泛的安全哈希算法, 用于数字签名、区块链、证书验证等。
 *
 * 改写说明:
 *   原程序使用向量指令 vsha2ms.vv, vsha2ch.vv, vsha2cl.vv,
 *   现已替换为标量 SHA-256 指令:
 *     vsha2ms.vv   → SHA256SIG0 + SHA256SIG1 + SHA256SUM0 + SHA256SUM1
 *     vsha2ch.vv   → 标准 SHA-256 压缩轮函数 (标量实现)
 *     vsha2cl.vv   → 标准 SHA-256 压缩轮函数 (标量实现)
 *
 * 编译要求: -march=rv64gc_zkn_zbkc
 */

#include "common.h"

/* ================================================================
 * SHA-256 消息扩展 (标量实现)
 *
 * 从 W[0..15] (来自输入块) 生成 W[16..63]:
 *   W[t] = σ1(W[t-2]) + W[t-7] + σ0(W[t-15]) + W[t-16]
 *
 * 标量指令加速:
 *   SHA256SIG0: Σ0(x) = ROR(x,2) ^ ROR(x,13) ^ ROR(x,22) — 压缩函数用
 *   SHA256SIG1: Σ1(x) = ROR(x,6) ^ ROR(x,11) ^ ROR(x,25) — 压缩函数用
 *   SHA256SUM0: σ0(x) = ROR(x,7) ^ ROR(x,18) ^ SHR(x,3)  — 消息扩展用
 *   SHA256SUM1: σ1(x) = ROR(x,17) ^ ROR(x,19) ^ SHR(x,10) — 消息扩展用
 * ================================================================ */

static void sha256_msg_schedule(uint32_t W[64]) {
    for (int t = 16; t < 64; t++) {
        uint32_t s0, s1;

#ifdef USE_INLINE_ASM
        SHA256SUM0(s0, W[t - 15]);    /* σ0(W[t-15]) */
        SHA256SUM1(s1, W[t - 2]);     /* σ1(W[t-2])  */
#else
        /* 纯 C 实现 */
        uint32_t x;
        x = W[t - 15];
        s0 = scalar_ror32(x, 7) ^ scalar_ror32(x, 18) ^ (x >> 3);
        x = W[t - 2];
        s1 = scalar_ror32(x, 17) ^ scalar_ror32(x, 19) ^ (x >> 10);
#endif

        W[t] = s1 + W[t - 7] + s0 + W[t - 16];
    }
}

/* ================================================================
 * SHA-256 压缩函数 (标量实现)
 *
 * 64 轮压缩, 使用 8 个工作变量 (a,b,c,d,e,f,g,h)。
 * 每轮消耗一个消息字 W[t] 和轮常量 K[t]。
 *
 * 标量指令加速:
 *   SHA256SIG0: Σ0 — 用于计算循环中的 T1 (Maj 部分)
 *   SHA256SIG1: Σ1 — 用于计算循环中的 T1 (参与)
 * ================================================================ */

static void sha256_compress(uint32_t H[8], const uint32_t W[64]) {
    uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
    uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

    for (int t = 0; t < 64; t++) {
        uint32_t S1, S0, ch, maj, temp1, temp2;

#ifdef USE_INLINE_ASM
        SHA256SIG1(S1, e);   /* Σ1(e) */
        SHA256SIG0(S0, a);   /* Σ0(a) */
#else
        S1 = scalar_ror32(e, 6) ^ scalar_ror32(e, 11) ^ scalar_ror32(e, 25);
        S0 = scalar_ror32(a, 2) ^ scalar_ror32(a, 13) ^ scalar_ror32(a, 22);
#endif

        /* Ch(e, f, g) = (e & f) ^ (~e & g) */
        ch  = (e & f) ^ ((~e) & g);
        /* Maj(a, b, c) = (a & b) ^ (a & c) ^ (b & c) */
        maj = (a & b) ^ (a & c) ^ (b & c);

        temp1 = h + S1 + ch + SHA256_K[t] + W[t];
        temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    /* Davies-Meyer: H = H + 压缩结果 */
    H[0] += a;  H[1] += b;  H[2] += c;  H[3] += d;
    H[4] += e;  H[5] += f;  H[6] += g;  H[7] += h;
}

/* ================================================================
 * SHA-256 完整哈希函数
 * ================================================================ */

static void sha256_hash(const uint8_t *msg, size_t len, uint32_t digest[8]) {
    /* 初始化哈希值 */
    memcpy(digest, SHA256_IV, 32);

    /* 填充 */
    uint64_t bit_len = (uint64_t)len * 8;
    size_t padded_len = ((len + 8 + 64) / 64) * 64;
    uint8_t *padded = (uint8_t *)calloc(padded_len, 1);
    memcpy(padded, msg, len);
    padded[len] = 0x80;

    /* 大端序写入长度 */
    for (int i = 0; i < 8; i++)
        padded[padded_len - 8 + i] = (uint8_t)(bit_len >> (56 - i * 8));

    /* 逐块处理 */
    for (size_t off = 0; off < padded_len; off += 64) {
        uint32_t W[64] = {0};

        /* 大端序加载 W[0..15] */
        for (int i = 0; i < 16; i++) {
            W[i] = ((uint32_t)padded[off + i * 4 + 0] << 24) |
                   ((uint32_t)padded[off + i * 4 + 1] << 16) |
                   ((uint32_t)padded[off + i * 4 + 2] << 8)  |
                   ((uint32_t)padded[off + i * 4 + 3]);
        }

        sha256_msg_schedule(W);
        sha256_compress(digest, W);
    }

    free(padded);
}

/* ================================================================
 * 主程序
 * ================================================================ */

int main(void) {
    uint32_t digest[8];

    print_title("SHA-256 安全哈希示例 (标量加密指令)");

    /* ===== 测试 1: "abc" ===== */
    printf("\n  [测试1] NIST SHA-256 测试向量: \"abc\"\n");

    sha256_hash(SHA256_TEST_INPUT, SHA256_TEST_INPUT_LEN, digest);

    hexdump("输入", SHA256_TEST_INPUT, SHA256_TEST_INPUT_LEN);
    hexdump_u32("计算摘要", digest, 8);
    hexdump_u32("期望摘要", SHA256_TEST_DIGEST, 8);

    int test1_ok = mem_eq_u32(digest, SHA256_TEST_DIGEST, 8);
    test_result("SHA-256(\"abc\") NIST 测试向量", test1_ok);

    /* ===== 测试 2: 空消息 ===== */
    printf("\n  [测试2] SHA-256 空消息\n");
    sha256_hash((const uint8_t *)"", 0, digest);
    hexdump_u32("SHA-256(\"\")", digest, 8);
    printf("  (标准值: e3b0c442 98fc1c14 9afbf4c8 996fb924 "
           "27ae41e4 649b934c a495991b 7852b855)\n");

    /* ===== 测试 3: 长消息 ===== */
    printf("\n  [测试3] SHA-256 长消息 (448 bits)\n");
    const char *long_msg =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    size_t long_len = strlen(long_msg);
    sha256_hash((const uint8_t *)long_msg, long_len, digest);
    hexdump_u32("SHA-256(long)", digest, 8);

    /* ===== 性能测试 ===== */
    printf("\n  [性能] SHA-256 吞吐量测试 (1MB 数据)\n");

    #define SHA_PERF_SIZE (1024 * 1024)
    uint8_t *perf_buf = (uint8_t *)malloc(SHA_PERF_SIZE);
    memset(perf_buf, 0x55, SHA_PERF_SIZE);

    uint64_t p_t0 = get_time_us();
    sha256_hash(perf_buf, SHA_PERF_SIZE, digest);
    uint64_t p_t1 = get_time_us();

    double elapsed_ms = (double)(p_t1 - p_t0) / 1000.0;
    double throughput = (double)SHA_PERF_SIZE / (1024.0 * 1024.0) /
                        ((double)(p_t1 - p_t0) / 1000000.0);

    printf("  数据量   : %d bytes\n", SHA_PERF_SIZE);
    printf("  耗时     : %.3f ms\n", elapsed_ms);
    printf("  吞吐量   : %.2f MB/s\n", throughput);

    free(perf_buf);

    /* ===== 总结 ===== */
    printf("\n");
    print_separator();
    printf("  使用指令: SHA256SUM0 / SHA256SUM1 (消息扩展)\n");
    printf("            SHA256SIG0 / SHA256SIG1 (压缩函数)\n");
    printf("            ROR / RORI (循环移位, Zbkb)\n");
    printf("  扩展    : Zknh (SHA-2 哈希) + Zbkb (位操作)\n");
    printf("  算法    : SHA-256 (NIST FIPS 180-4)\n");
    print_separator();

    return test1_ok ? 0 : 1;
}
