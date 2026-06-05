# RISC-V 标量加密扩展示例程序

基于 RISC-V 加密扩展规范 (Volume I: Scalar Instructions, v1.0.0) 的完整示例项目，
演示如何使用标量加密指令实现各密码学算法。

## 改写说明

本项目已从原来的**向量加密指令** (Zvkned/Zvknh/Zvksed/Zvksh/Zvkg) 
全部改写为**标量加密指令** (Zknd/Zkne/Zknh/Zksed/Zksh/Zbkb/Zbkc/Zbkx)。

## 项目结构

```
crypto_demos/
├── Makefile              # 构建系统
├── common.h              # 公共头文件: 测试向量, 标量指令, 工具函数
├── aes_ctr/
│   └── aes_ctr.c         # AES-256-CTR 模式加解密
├── sha256/
│   └── sha256.c          # SHA-256 安全哈希
├── sm4/
│   └── sm4_ecb.c         # SM4-ECB 分组密码
├── sm3/
│   └── sm3.c             # SM3 密码杂凑
└── gcm/
    └── aes_gcm.c         # AES-256-GCM 认证加密
```

## 覆盖的标量加密指令

| 扩展 | 指令 | 用途 | 示例程序 |
|------|------|------|---------|
| Zkne/Zknd | AES64ESM | AES 加密中间轮 | aes_ctr, gcm |
| Zkne/Zknd | AES64ES | AES 加密最终轮 | aes_ctr, gcm |
| Zkne/Zknd | AES64ESM | AES 解密中间轮 | (CTR 自动支持) |
| Zkne/Zknd | AES64DS | AES 解密最终轮 | (CTR 自动支持) |
| Zkne/Zknd | AES64KS2 | AES 密钥调度异或 | aes_ctr, gcm |
| Zknh | SHA256SIG0 | SHA-256 Σ0 | sha256 |
| Zknh | SHA256SIG1 | SHA-256 Σ1 | sha256 |
| Zknh | SHA256SUM0 | SHA-256 σ0 | sha256 |
| Zknh | SHA256SUM1 | SHA-256 σ1 | sha256 |
| Zksed | SM4ED | SM4 轮函数 | sm4 |
| Zksed | SM4KS | SM4 密钥扩展 | sm4 |
| Zksh | SM3P0 | SM3 P0 置换 | sm3 |
| Zksh | SM3P1 | SM3 P1 置换 | sm3 |
| Zbkc | CLMUL | 无进位乘法低半 | gcm |
| Zbkc | CLMULH | 无进位乘法高半 | gcm |
| Zbkb | ROR/RORI | 循环右移 | sha256, sm3, sm4 |

## 向量指令 → 标量指令对照

| 向量指令 | 标量替换 | 说明 |
|---------|---------|------|
| vaeskf2.vi | AES64KS1I + AES64KS2 | AES-256 密钥扩展 |
| vaesem.vv | AES64ESM | AES 中间轮加密 |
| vaesef.vv | AES64ES | AES 最终轮加密 |
| vsha2ms.vv | SHA256SUM0/1 | SHA-256 消息扩展 |
| vsha2ch/cl.vv | SHA256SIG0/1 + C | SHA-256 压缩 |
| vsm4k.vi | SM4KS | SM4 密钥扩展 |
| vsm4r.vv | SM4ED | SM4 轮函数 |
| vsm3me.vv | SM3P1 + C | SM3 消息扩展 |
| vsm3c.vi | SM3P0 + C | SM3 压缩 |
| vghsh.vv | CLMUL + CLMULH | GHASH 加-乘 |
| vgmul.vv | CLMUL + CLMULH | GF(2^128) 乘法 |

## 编译和运行

### 环境要求

| 组件 | 最低版本 | 说明 |
|------|---------|------|
| RISC-V GCC | 14.0+ | 需支持 `-march=rv64gc_zkn_zks_zbkc` |
| 或 LLVM/Clang | 18.0+ | 需启用标量加密扩展 |
| QEMU | 8.0+ | 用于在 x86 上模拟运行 |

### 编译命令

```bash
# 编译标量纯C版本 (可在无RISC-V工具链的平台做语法检查)
make

# 编译标量汇编内联版本 (使用 .insn 伪指令)
make INLINE=1

# 运行所有示例
make run

# 单独运行某个算法
make run-aes
make run-sha256
make run-sm4
make run-sm3
make run-gcm

# 清理
make clean
```

## 算法说明

### 1. AES-256-CTR (`aes_ctr/aes_ctr.c`)

使用标量 AES 指令 AES64ESM/AES64ES 实现 CTR 模式加密。
每分组需 14 条加密指令 (13 条 AES64ESM + 1 条 AES64ES)。

### 2. SHA-256 (`sha256/sha256.c`)

使用标量 SHA-256 指令 SHA256SUM0/SUM1/SIG0/SIG1 加速消息扩展和压缩。
每 64 轮压缩调用 4×64 = 256 次标量指令。

### 3. SM4-ECB (`sm4/sm4_ecb.c`)

使用标量 SM4 指令 SM4KS/SM4ED。
32 轮 × 4 条/轮 = 128 条指令/分组。

### 4. SM3 (`sm3/sm3.c`)

使用标量 SM3 指令 SM3P0/SM3P1 加速置换操作。
消息扩展和压缩主要使用纯 C 实现配合标量指令。

### 5. AES-256-GCM (`gcm/aes_gcm.c`)

加密部分使用 AES64ESM/AES64ES, 认证部分使用 CLMUL/CLMULH 实现 GF(2^128) 乘法。
GHASH 的 GF(2^128) 乘法通过 Karatsuba 分解 + CLMUL 实现。

## 参考资料

- [RISC-V Cryptography Extensions Volume I: Scalar & Entropy Source v1.0.0](https://github.com/riscv/riscv-crypto)
- [NIST FIPS 197 - AES](https://csrc.nist.gov/publications/detail/fips/197/final)
- [NIST FIPS 180-4 - SHA-2](https://csrc.nist.gov/publications/detail/fips/180/4/final)
- [NIST SP 800-38D - GCM](https://csrc.nist.gov/publications/detail/sp/800-38d/final)
- [GM/T 0002-2012 - SM4](http://www.gmbz.org.cn/)
- [GM/T 0004-2012 - SM3](http://www.gmbz.org.cn/)
