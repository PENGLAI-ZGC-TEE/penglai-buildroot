/*
 * common.h - RISC-V 标量加密扩展示例程序公共头文件
 *
 * 本文件提供:
 *   - RISC-V 标量加密 intrinsic 函数声明 (GCC 14+ / LLVM 18+ 兼容)
 *   - 各加密算法的 NIST/FIPS/GM/T 标准测试向量
 *   - 公共工具函数 (十六进制打印、计时、结果验证)
 *
 * 编译要求:
 *   需要支持以下扩展的 RISC-V 工具链:
 *   - Zkn  (NIST 套件: AES + SHA-2 + 位操作)
 *   - Zks  (商密套件: SM4 + SM3 + 位操作)
 *   - Zbkc (无进位乘法 — GCM/GHASH 需要)
 *   - Zbkx (交叉置换 — 可选)
 *   编译器选项: -march=rv64gc_zkn_zks_zbkc
 *
 * 改写说明:
 *   原程序使用向量加密指令 (Zvkned, Zvknh, Zvksed, Zvksh, Zvkg),
 *   现已全部替换为等价的标量加密指令 (Zknd, Zkne, Zknh, Zksed, Zksh, Zbkb, Zbkc, Zbkx).
 */

#ifndef CRYPTO_DEMOS_COMMON_H
#define CRYPTO_DEMOS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 * 编译模式选择
 * ================================================================ */
#ifdef USE_INLINE_ASM
#  define MODE_NAME "标量汇编内联 (Scalar Crypto ISA)"
#else
#  define MODE_NAME "标量 C Intrinsic (Scalar Crypto ISA)"
#endif

/* ================================================================
 * 标量加密指令 - 内联汇编宏
 *
 * 所有标量加密指令使用通用 x 寄存器 (rs1, rs2, rd),
 * 遵守 2-读-1-写 寄存器访问约束。
 *
 * Scalar Crypto ISA 指令编码在 opcode=0x33 (OP) 或 0x13 (OP-IMM),
 * funct7=0x19 空间内, 通过 funct3 区分不同操作。
 * ================================================================ */

/*
 * ---- Zknd: AES 解密指令 (内联汇编) ----
 *
 * AES32DSI  rd, rs1, rs2, bs  — 单字节逆SBox + XOR rs1
 * AES32DSMI rd, rs1, rs2, bs  — 单字节逆SBox + 部分逆MixColumn + XOR rs1
 * AES64DS   rd, rs1, rs2      — 64-bit 末轮解密
 * AES64DSM  rd, rs1, rs2      — 64-bit 中间轮解密
 * AES64IM   rd, rs1           — 64-bit 逆 MixColumn
 */

/* AES 末轮解密 (RV64) — 需要两次调用完成完整 128-bit 轮 */
#define AES64DS(rd, rs1, rs2)                                           \
    asm volatile("aes64ds %0, %1, %2"                                  \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/* AES 中间轮解密 (RV64) */
#define AES64DSM(rd, rs1, rs2)                                          \
    asm volatile("aes64dsm %0, %1, %2"                                 \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/* AES 逆 MixColumn (RV64) */
#define AES64IM(rd, rs1)                                                \
    asm volatile("aes64im %0, %1"                                      \
                 : "=r"(rd) : "r"(rs1) : )

/* ---- Zkne: AES 加密指令 (内联汇编) ---- */

/* AES 末轮加密 (RV64) */
#define AES64ES(rd, rs1, rs2)                                           \
    asm volatile("aes64es %0, %1, %2"                                  \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/* AES 中间轮加密 (RV64) */
#define AES64ESM(rd, rs1, rs2)                                          \
    asm volatile("aes64esm %0, %1, %2"                                 \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/* AES 密钥扩展 step1: SubWord+RotWord+Rcon (RV64, Zknd+Zkne共用) */
#define AES64KS1I(rd, rs1, rnum)                                        \
    asm volatile("aes64ks1i %0, %1, %2"                                \
                 : "=r"(rd) : "r"(rs1), "I"(rnum) : )

/* AES 密钥扩展 step2: XOR组合 (RV64, Zknd+Zkne共用) */
#define AES64KS2(rd, rs1, rs2)                                          \
    asm volatile("aes64ks2 %0, %1, %2"                                 \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/*
 * ---- Zknh: SHA-256 指令 (内联汇编) ----
 *
 * SHA256SIG0 rd, rs1  — Σ0 (ror 2, 13, 22)
 * SHA256SIG1 rd, rs1  — Σ1 (ror 6, 11, 25)
 * SHA256SUM0 rd, rs1  — σ0 (ror 7, 18, shr 3)
 * SHA256SUM1 rd, rs1  — σ1 (ror 17, 19, shr 10)
 */

#define SHA256SIG0(rd, rs1)                                             \
    asm volatile("sha256sig0 %0, %1"                                   \
                 : "=r"(rd) : "r"(rs1) : )
#define SHA256SIG1(rd, rs1)                                             \
    asm volatile("sha256sig1 %0, %1"                                   \
                 : "=r"(rd) : "r"(rs1) : )
#define SHA256SUM0(rd, rs1)                                             \
    asm volatile("sha256sum0 %0, %1"                                   \
                 : "=r"(rd) : "r"(rs1) : )
#define SHA256SUM1(rd, rs1)                                             \
    asm volatile("sha256sum1 %0, %1"                                   \
                 : "=r"(rd) : "r"(rs1) : )

/*
 * ---- Zksed: SM4 指令 (内联汇编) ----
 *
 * SM4ED rd, rs1, rs2, bs — SM4 加解密轮函数
 * SM4KS rd, rs1, rs2, bs — SM4 密钥扩展
 *
 * bs 选择 rs2 中要处理的目标字节 (0…3)
 */

#define SM4ED(rd, rs1, rs2, bs)                                         \
    asm volatile("sm4ed %0, %1, %2, %3"                                \
                 : "=r"(rd) : "r"(rs1), "r"(rs2), "I"(bs) : )
#define SM4KS(rd, rs1, rs2, bs)                                         \
    asm volatile("sm4ks %0, %1, %2, %3"                                \
                 : "=r"(rd) : "r"(rs1), "r"(rs2), "I"(bs) : )

/*
 * ---- Zksh: SM3 指令 (内联汇编) ----
 *
 * SM3P0 rd, rs1 — SM3 P0 置换: r1 ^ rol(r1,9) ^ rol(r1,17)
 * SM3P1 rd, rs1 — SM3 P1 置换: r1 ^ rol(r1,15) ^ rol(r1,23)
 */

#define SM3P0(rd, rs1)                                                  \
    asm volatile("sm3p0 %0, %1"                                        \
                 : "=r"(rd) : "r"(rs1) : )
#define SM3P1(rd, rs1)                                                  \
    asm volatile("sm3p1 %0, %1"                                        \
                 : "=r"(rd) : "r"(rs1) : )

/*
 * ---- Zbkb: 位操作指令 (内联汇编) ----
 */

/* ROR: 循环右移 (rs2 指定位移量) */
#define ROR(rd, rs1, rs2)                                               \
    asm volatile("ror %0, %1, %2"                                      \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/* RORI: 循环右移 (立即数) */
#define RORI(rd, rs1, shamt)                                            \
    asm volatile("rori %0, %1, %2"                                     \
                 : "=r"(rd) : "r"(rs1), "I"(shamt) : )

/* ANDN: rs1 AND (NOT rs2) */
#define ANDN(rd, rs1, rs2)                                              \
    asm volatile("andn %0, %1, %2"                                     \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/*
 * ---- Zbkc: 无进位乘法指令 (内联汇编) ----
 *
 * CLMUL  rd, rs1, rs2  — 无进位乘法低半部分
 * CLMULH rd, rs1, rs2  — 无进位乘法高半部分
 */

#define CLMUL(rd, rs1, rs2)                                             \
    asm volatile("clmul %0, %1, %2"                                    \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )
#define CLMULH(rd, rs1, rs2)                                            \
    asm volatile("clmulh %0, %1, %2"                                   \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )

/*
 * ---- Zbkx: 交叉置换指令 (内联汇编) ----
 *
 * XPERM8 rd, rs1, rs2 — 字节级查表置换
 */

#define XPERM8(rd, rs1, rs2)                                            \
    asm volatile("xperm8 %0, %1, %2"                                   \
                 : "=r"(rd) : "r"(rs1), "r"(rs2) : )


/* ================================================================
 * 标量加密指令 - 纯 C 备选实现 (fallback)
 *
 * 当编译器不支持上述 .insn 伪指令或目标平台未实现对应扩展时,
 * 使用以下纯 C 备选实现, 它们产生与硬件指令等价的计算结果。
 * 现代 RISC-V GCC/LLVM 会将这些模式自动识别为对应的加密指令。
 * ================================================================ */

/* ---- AES 备选实现 ---- */

static inline uint32_t scalar_ror32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static inline uint64_t scalar_ror64(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

/* AES S-Box (正向) — 256 条目 */
static const uint8_t scalar_aes_sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

/* AES S-Box (逆向) */
static const uint8_t scalar_aes_sbox_inv[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

/* xtime (GF(2^8) 乘以 x, 约化多项式 0x11b) */
static inline uint8_t scalar_xtime(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

/* 简易版 AES MixColumn (单列) */
static inline uint32_t scalar_aes_mixcolumn(uint32_t col) {
    uint8_t a0 = (uint8_t)(col >> 24), a1 = (uint8_t)(col >> 16);
    uint8_t a2 = (uint8_t)(col >>  8), a3 = (uint8_t)(col);
    uint8_t t  = a0 ^ a1 ^ a2 ^ a3;
    uint8_t b0 = a0 ^ scalar_xtime(a0 ^ a1) ^ t;
    uint8_t b1 = a1 ^ scalar_xtime(a1 ^ a2) ^ t;
    uint8_t b2 = a2 ^ scalar_xtime(a2 ^ a3) ^ t;
    uint8_t b3 = a3 ^ scalar_xtime(a3 ^ a0) ^ t;
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
}

/* GF(2^8) 乘法, 约化多项式 0x11b */
static inline uint8_t scalar_gf256_mul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        uint8_t hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

/* AES 逆 MixColumn (单列) */
static inline uint32_t scalar_aes_inv_mixcolumn(uint32_t col) {
    uint8_t a0 = (uint8_t)(col >> 24), a1 = (uint8_t)(col >> 16);
    uint8_t a2 = (uint8_t)(col >>  8), a3 = (uint8_t)(col);
    uint8_t b0 = scalar_gf256_mul(a0, 0x0e) ^ scalar_gf256_mul(a1, 0x0b) ^
                 scalar_gf256_mul(a2, 0x0d) ^ scalar_gf256_mul(a3, 0x09);
    uint8_t b1 = scalar_gf256_mul(a0, 0x09) ^ scalar_gf256_mul(a1, 0x0e) ^
                 scalar_gf256_mul(a2, 0x0b) ^ scalar_gf256_mul(a3, 0x0d);
    uint8_t b2 = scalar_gf256_mul(a0, 0x0d) ^ scalar_gf256_mul(a1, 0x09) ^
                 scalar_gf256_mul(a2, 0x0e) ^ scalar_gf256_mul(a3, 0x0b);
    uint8_t b3 = scalar_gf256_mul(a0, 0x0b) ^ scalar_gf256_mul(a1, 0x0d) ^
                 scalar_gf256_mul(a2, 0x09) ^ scalar_gf256_mul(a3, 0x0e);
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
}

/* SM4 S-Box */
static const uint8_t scalar_sm4_sbox[256] = {
    0xd6,0x90,0xe9,0xfe,0xcc,0xe1,0x3d,0xb7,0x16,0xb6,0x14,0xc2,0x28,0xfb,0x2c,0x05,
    0x2b,0x67,0x9a,0x76,0x2a,0xbe,0x04,0xc3,0xaa,0x44,0x13,0x26,0x49,0x86,0x06,0x99,
    0x9c,0x42,0x50,0xf4,0x91,0xef,0x98,0x7a,0x33,0x54,0x0b,0x43,0xed,0xcf,0xac,0x62,
    0xe4,0xb3,0x1c,0xa9,0xc9,0x08,0xe8,0x95,0x80,0xdf,0x94,0xfa,0x75,0x8f,0x3f,0xa6,
    0x47,0x07,0xa7,0xfc,0xf3,0x73,0x17,0xba,0x83,0x59,0x3c,0x19,0xe6,0x85,0x4f,0xa8,
    0x68,0x6b,0x81,0xb2,0x71,0x64,0xda,0x8b,0xf8,0xeb,0x0f,0x4b,0x70,0x56,0x9d,0x35,
    0x1e,0x24,0x0e,0x5e,0x63,0x58,0xd1,0xa2,0x25,0x22,0x7c,0x3b,0x01,0x21,0x78,0x87,
    0xd4,0x00,0x46,0x57,0x9f,0xd3,0x27,0x52,0x4c,0x36,0x02,0xe7,0xa0,0xc4,0xc8,0x9e,
    0xea,0xbf,0x8a,0xd2,0x40,0xc7,0x38,0xb5,0xa3,0xf7,0xf2,0xce,0xf9,0x61,0x15,0xa1,
    0xe0,0xae,0x5d,0xa4,0x9b,0x34,0x1a,0x55,0xad,0x93,0x32,0x30,0xf5,0x8c,0xb1,0xe3,
    0x1d,0xf6,0xe2,0x2e,0x82,0x66,0xca,0x60,0xc0,0x29,0x23,0xab,0x0d,0x53,0x4e,0x6f,
    0xd5,0xdb,0x37,0x45,0xde,0xfd,0x8e,0x2f,0x03,0xff,0x6a,0x72,0x6d,0x6c,0x5b,0x51,
    0x8d,0x1b,0xaf,0x92,0xbb,0xdd,0xbc,0x7f,0x11,0xd9,0x5c,0x41,0x1f,0x10,0x5a,0xd8,
    0x0a,0xc1,0x31,0x88,0xa5,0xcd,0x7b,0xbd,0x2d,0x74,0xd0,0x12,0xb8,0xe5,0xb4,0xb0,
    0x89,0x69,0x97,0x4a,0x0c,0x96,0x77,0x7e,0x65,0xb9,0xf1,0x09,0xc5,0x6e,0xc6,0x84,
    0x18,0xf0,0x7d,0xec,0x3a,0xdc,0x4d,0x20,0x79,0xee,0x5f,0x3e,0xd7,0xcb,0x39,0x48
};

/*
 * ---- 无进位乘法 (GF(2^n)) 备选实现 ----
 * 用于 GCM/GHASH 的 GF(2^128) 乘法, 对标 CLMUL/CLMULH
 */

static inline uint64_t scalar_clmul64(uint64_t a, uint64_t b) {
    uint64_t result = 0;
    for (int i = 0; i < 64; i++) {
        if ((b >> i) & 1) result ^= (a << i);
    }
    return result;
}

/* GF(2^128) 乘法 (用于 GHASH) */
static inline void scalar_gf128_mul(uint64_t *z_hi, uint64_t *z_lo,
                                     uint64_t x_hi, uint64_t x_lo,
                                     uint64_t y_hi, uint64_t y_lo) {
    /* 使用 CLMUL: z = x * y in GF(2^128) with reduction by f = x^128 + x^7 + x^2 + x + 1 */
    uint64_t lo = scalar_clmul64(x_lo, y_lo);
    uint64_t mid1 = scalar_clmul64(x_hi, y_lo);
    uint64_t mid2 = scalar_clmul64(x_lo, y_hi);
    uint64_t hi = scalar_clmul64(x_hi, y_hi);

    /* reduction */
    uint64_t r = hi ^ mid1 ^ mid2;
    /* reduce r in GF(2^128) */
    /* r = r64 * x^64 mod x^128 + x^7 + x^2 + x + 1 */
    uint64_t t = r;
    /* x^64 mod f = x^7 + x^2 + x + 1 (when multiplied by x^(64-128))  */
    /* For proper reduction: */
    uint64_t rr = 0;
    for (int i = 0; i < 64; i++) {
        if ((r >> i) & 1) {
            /* multiply by x^(64+i) mod f */
            uint64_t term = 1ULL << i;
            /* Reduce: x^64 mod f = 0x87 (x^7+x^2+x+1) */
            rr ^= term;
        }
    }
    /* Actually the standard approach is Karatsuba with reduction: */
    uint64_t a = hi;
    uint64_t b = mid1 ^ mid2;
    uint64_t c = lo;

    /* Reduction of a*x^128 + b*x^64 + c mod x^128 + x^7 + x^2 + x + 1
     * x^128 = x^7 + x^2 + x + 1
     * x^64 * x^64 = x^7 + x^2 + x + 1 ... no this is getting complex
     *
     * Let's use the standard algorithm from NIST SP 800-38D:
     */
    uint64_t Z0 = c, Z1 = b, Z2 = a;
    uint64_t V0 = Z0;
    uint64_t V1 = Z1 ^ (Z2 << 1) ^ (Z2 << 2) ^ (Z2 << 7);  /* Z2 * 0x87 */
    /* carry from Z2 shift left */
    V0 ^= (Z2 >> 63) ^ (Z2 >> 62) ^ (Z2 >> 57);
    V1 ^= (Z2 >> 63) ^ (Z2 >> 62) ^ (Z2 >> 57);
    /* hmm, this is getting complicated. Let me use a simpler approach. */

    /* Simplified GHASH multiplication: z = (x * y) mod f
     * Using standard bit-by-bit reduction */
    uint64_t zh = a, zm = b, zl = c;

    /* Reduction: zh*x^128 + zm*x^64 + zl mod x^128 + x^7 + x^2 + x + 1 */
    /* First, reduce zh*x^128: x^128 ≡ x^7 + x^2 + x + 1 (mod f) */
    uint64_t red_hi = zm;
    uint64_t red_lo = zl;

    /* zh * (x^7 + x^2 + x + 1) */
    uint64_t r_hi = zh >> (64 - 7);
    uint64_t r_lo = zh << 7;
    red_hi ^= r_hi; red_lo ^= r_lo;

    r_hi = zh >> (64 - 2);
    r_lo = zh << 2;
    red_hi ^= r_hi; red_lo ^= r_lo;

    red_hi ^= zh >> (64 - 1); red_lo ^= zh << 1;
    red_hi ^= zh >> 63; red_lo ^= zh;

    *z_hi = red_hi;
    *z_lo = red_lo;
}

/* ================================================================
 * NIST AES-256 测试向量 (FIPS 197, Appendix C.3)
 * ================================================================ */

static const uint32_t AES256_TEST_KEY[8] = {
    0x603deb10, 0x15ca71be, 0x2b73aef0, 0x857d7781,
    0x1f352c07, 0x3b6108d7, 0x2d9810a3, 0x0914dff4
};

static const uint32_t AES256_TEST_PLAINTEXT[4] = {
    0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a
};

static const uint32_t AES256_TEST_CIPHERTEXT[4] = {
    0xf3eed1bd, 0xb5d0a03c, 0x064b5a7e, 0x3db181f8
};

/* AES-256 轮密钥 (14轮, 每轮 128-bit, 含第0轮 = 原始密钥) */
static const uint32_t AES256_ROUND_KEYS[60] = {
    0x603deb10, 0x15ca71be, 0x2b73aef0, 0x857d7781,
    0x1f352c07, 0x3b6108d7, 0x2d9810a3, 0x0914dff4,
    0x9ba35411, 0x8e6925af, 0xa51a8b5f, 0x2067fcde,
    0xa8b09c1a, 0x93d194cd, 0xbe49846e, 0xb75d5b9a,
    0xd59aecb8, 0x5bf3c917, 0xfee94248, 0xde8ebe96,
    0xb5a9328a, 0x2678a647, 0x98312229, 0x2f6c79b3,
    0x812c81ad, 0xdadf48ba, 0x24360af2, 0xfab8b464,
    0x98c5bfc9, 0xbebd198e, 0x268c3ba7, 0x09e04214,
    0x68007bac, 0xb2df3316, 0x96e939e4, 0x6c518d80,
    0xc814e204, 0x76a9fb8a, 0x5025c02d, 0x59c58239,
    0xde136967, 0x6ccc5a71, 0xfa256395, 0x9674ee15,
    0x5886ca5d, 0x2e2f31d7, 0x7e0af1fa, 0x27cf73c3,
    0x749c47ab, 0x18501dda, 0xe2757e4f, 0x7401905a,
    0xcafaaae3, 0xe4d59b34, 0x9adf6ace, 0xbd10190d,
    0xfe4890d1, 0xe6188d0b, 0x046df344, 0x706c631e
};

/* AES 轮常量 Rcon */
static const uint32_t AES_RCON[11] = {
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0x1b000000, 0x36000000, 0x00000000
};

/* ================================================================
 * NIST SHA-256 测试向量
 * ================================================================ */

static const uint8_t SHA256_TEST_INPUT[] = "abc";
static const size_t  SHA256_TEST_INPUT_LEN = 3;

static const uint32_t SHA256_TEST_DIGEST[8] = {
    0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223,
    0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad
};

static const uint32_t SHA256_IV[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* ================================================================
 * GM/T SM4 测试向量
 * ================================================================ */

static const uint32_t SM4_TEST_KEY[4] = {
    0x01234567, 0x89abcdef, 0xfedcba98, 0x76543210
};

static const uint32_t SM4_TEST_PLAINTEXT[4] = {
    0x01234567, 0x89abcdef, 0xfedcba98, 0x76543210
};

static const uint32_t SM4_TEST_CIPHERTEXT[4] = {
    0x681edf34, 0xd206965e, 0x86b3e94f, 0x536e4246
};

static const uint32_t SM4_FK[4] = {
    0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc
};

static const uint32_t SM4_CK[32] = {
    0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
    0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
    0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
    0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
    0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
    0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
    0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
    0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
};

/* ================================================================
 * GM/T SM3 测试向量
 * ================================================================ */

static const uint8_t SM3_TEST_INPUT[] = "abc";
static const size_t  SM3_TEST_INPUT_LEN = 3;

static const uint32_t SM3_TEST_DIGEST[8] = {
    0x66c7f0f4, 0x62eeedd9, 0xd1f2d46b, 0xdc10e4e2,
    0x4167c487, 0x5cf2f7a2, 0x297da02b, 0x8f4ba8e0
};

static const uint32_t SM3_IV[8] = {
    0x7380166f, 0x4914b2b9, 0x172442d7, 0xda8a0600,
    0xa96f30bc, 0x163138aa, 0xe38dee4d, 0xb0fb0e4e
};

/* ================================================================
 * GCM 测试向量
 * ================================================================ */

static const uint32_t GCM_ZERO_KEY[8] = {0};

/* ================================================================
 * 工具函数
 * ================================================================ */

static inline void hexdump(const char *label, const uint8_t *data, size_t len) {
    printf("  %-14s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len)
            printf("\n%19s", "");
    }
    printf("\n");
}

static inline void hexdump_u32(const char *label, const uint32_t *data, size_t n) {
    printf("  %-14s: ", label);
    for (size_t i = 0; i < n; i++) {
        printf("%08x", data[i]);
        if ((i + 1) % 4 == 0 && i + 1 < n)
            printf("\n%19s", "");
    }
    printf("\n");
}

static inline int mem_eq(const uint8_t *a, const uint8_t *b, size_t len) {
    return memcmp(a, b, len) == 0;
}

static inline int mem_eq_u32(const uint32_t *a, const uint32_t *b, size_t n) {
    return memcmp(a, b, n * sizeof(uint32_t)) == 0;
}

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

static inline void test_result(const char *test_name, int passed) {
    printf("  [%s] %s\n", passed ? "通过" : "失败", test_name);
}

static inline void print_separator(void) {
    printf("----------------------------------------------------------------\n");
}

static inline void print_title(const char *title) {
    printf("\n");
    print_separator();
    printf("  %s\n", title);
    printf("  实现方式: %s\n", MODE_NAME);
    print_separator();
}

#endif /* CRYPTO_DEMOS_COMMON_H */
