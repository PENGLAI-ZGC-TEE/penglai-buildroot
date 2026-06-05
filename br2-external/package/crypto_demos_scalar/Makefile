# ============================================================================
# RISC-V 标量加密扩展示例程序 - Makefile
#
# 编译要求:
#   - RISC-V GCC 14+ 或 LLVM/Clang 18+ 工具链
#   - 目标架构需支持: Zk (标量加密) + Zbkc (无进位乘法)
#
# 用法:
#   make                    # 编译所有示例
#   make INLINE=1           # 编译汇编内联版本 (使用 .insn 伪指令)
#   make run                # 编译并运行 (需 QEMU/RISC-V 硬件)
#   make clean              # 清理
#
# 改写说明:
#   已将原来的向量加密扩展 (-march=rv64gcv_zvkn_zvks) 
#   替换为标量加密扩展 (-march=rv64gc_zkn_zks_zbkc)
# ============================================================================

# ---- 工具链配置 ----
CROSS_COMPILE ?= riscv64-unknown-linux-gnu-
CC           = $(CROSS_COMPILE)gcc
CFLAGS_BASE  = -march=rv64gc_zkn_zks_zbkc -mabi=lp64d -O2 -static
CFLAGS       = $(CFLAGS_BASE) -Wall -Wextra -I.

# ---- QEMU ----
QEMU         = qemu-riscv64
QEMUFLAGS    = -cpu rv64,zkn=true,zks=true,zbkc=true

# ---- 编译模式 ----
ifeq ($(INLINE),1)
  CFLAGS      += -DUSE_INLINE_ASM
  BUILD_SUFFIX = _inline
else
  BUILD_SUFFIX = _scalar
endif

# ---- 源文件列表 ----
AES_CTR_SRC  = aes_ctr/aes_ctr.c
SHA256_SRC   = sha256/sha256.c
SM4_SRC      = sm4/sm4_ecb.c
SM3_SRC      = sm3/sm3.c
GCM_SRC      = gcm/aes_gcm.c

# ---- 目标文件 ----
TARGETS = aes_ctr$(BUILD_SUFFIX) sha256$(BUILD_SUFFIX) \
          sm4_ecb$(BUILD_SUFFIX) sm3$(BUILD_SUFFIX) \
          aes_gcm$(BUILD_SUFFIX)

# ---- 默认目标 ----
.PHONY: all
all: $(TARGETS)

# ---- 编译规则 ----
aes_ctr$(BUILD_SUFFIX): $(AES_CTR_SRC) common.h
	$(CC) $(CFLAGS) -o $@ $(AES_CTR_SRC)

sha256$(BUILD_SUFFIX): $(SHA256_SRC) common.h
	$(CC) $(CFLAGS) -o $@ $(SHA256_SRC)

sm4_ecb$(BUILD_SUFFIX): $(SM4_SRC) common.h
	$(CC) $(CFLAGS) -o $@ $(SM4_SRC)

sm3$(BUILD_SUFFIX): $(SM3_SRC) common.h
	$(CC) $(CFLAGS) -o $@ $(SM3_SRC)

aes_gcm$(BUILD_SUFFIX): $(GCM_SRC) common.h
	$(CC) $(CFLAGS) -o $@ $(GCM_SRC)

# ---- 运行目标 ----
.PHONY: run run-aes run-sha256 run-sm4 run-sm3 run-gcm
run: $(TARGETS)
	@echo "========================================="
	@echo "  运行所有标量加密示例"
	@echo "========================================="
	@for t in $(TARGETS); do \
		echo ""; \
		echo "--- $$t ---"; \
		$(QEMU) $(QEMUFLAGS) ./$$t || true; \
	done

run-aes: aes_ctr$(BUILD_SUFFIX)
	$(QEMU) $(QEMUFLAGS) ./aes_ctr$(BUILD_SUFFIX)

run-sha256: sha256$(BUILD_SUFFIX)
	$(QEMU) $(QEMUFLAGS) ./sha256$(BUILD_SUFFIX)

run-sm4: sm4_ecb$(BUILD_SUFFIX)
	$(QEMU) $(QEMUFLAGS) ./sm4_ecb$(BUILD_SUFFIX)

run-sm3: sm3$(BUILD_SUFFIX)
	$(QEMU) $(QEMUFLAGS) ./sm3$(BUILD_SUFFIX)

run-gcm: aes_gcm$(BUILD_SUFFIX)
	$(QEMU) $(QEMUFLAGS) ./aes_gcm$(BUILD_SUFFIX)

# ---- 清理 ----
.PHONY: clean
clean:
	rm -f aes_ctr_scalar aes_ctr_inline \
	      sha256_scalar sha256_inline \
	      sm4_ecb_scalar sm4_ecb_inline \
	      sm3_scalar sm3_inline \
	      aes_gcm_scalar aes_gcm_inline

# ---- 帮助 ----
.PHONY: help
help:
	@echo "RISC-V 标量加密扩展示例程序"
	@echo ""
	@echo "目标:"
	@echo "  make             编译标量版本 (纯C备选)"
	@echo "  make INLINE=1    编译标量版本 (汇编内联, 使用 .insn)"
	@echo "  make run         编译并运行"
	@echo "  make run-aes     仅编译并运行 AES-CTR"
	@echo "  make clean       清理编译产物"
	@echo ""
	@echo "示例:"
	@echo "  make INLINE=1 run-aes   编译标量汇编版 AES 并运行"
	@echo "  make run                编译标量纯C版全部并运行"
