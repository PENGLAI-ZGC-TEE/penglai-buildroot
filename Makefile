# below homes is submodules of this repo
export SBI_HOME=$(shell pwd)/opensbi
# export LINUX_HOME=$(shell pwd)/riscv-linux
export LINUX_HOME=$(shell pwd)/debian-linux-kernel
export RISCV_ROOTFS_HOME=$(shell pwd)/riscv-rootfs
export NEMU_HOME=$(shell pwd)/NEMU
export BUILDROOT_HOME ?= $(shell pwd)/buildroot
export BR2_EXTERNAL_ZGC_TEE_PATH ?= $(shell pwd)/br2-external

# sub-directories of the submodules
DTS_HOME=$(SBI_HOME)/dts
DTS_NAME=system.dts
FW_FDT_PATH=$(DTS_HOME)/xiangshan.dtb
IMG=$(SBI_HOME)/build/platform/generic/firmware/fw_payload.bin
PLATFORM=generic
FW_PAYLOAD_PATH=$(LINUX_HOME)/vmlinux.bin
# FW_PAYLOAD_PATH=

# LINUX_CONFIG
# LINUX_CONFIG=fpga_defconfig
LINUX_CONFIG=nanhu_fpga_defconfig
LINUX_INIT_CONFIG=init_defconfig

# arch and cross compile infomation
export ARCH=riscv
export ISA=riscv64
export CROSS_COMPILE=riscv64-unknown-linux-gnu-
export CROSS_COMPILE_OBJCOPY=$(CROSS_COMPILE)objcopy
export RISCV=/opt/riscv/
export OPENSBI_PLATFORM_RISCV_ISA ?= rv64imafdc_zicsr_zifencei
export OPENSBI_NEMU_SKIP_SPMP_ENABLE ?= 1

# NEMU settings
NEMU_BINARY=$(NEMU_HOME)/build/riscv64-nemu-interpreter
BUILDROOT_OUTPUT ?= $(shell pwd)/output/nemu
BUILDROOT_OVERRIDE_FILE ?= $(BUILDROOT_OUTPUT)/local.mk
BUILDROOT_MAKE = $(MAKE) -C $(BUILDROOT_HOME) BR2_EXTERNAL=$(BR2_EXTERNAL_ZGC_TEE_PATH) O=$(BUILDROOT_OUTPUT)
BUILDROOT_LOCAL_MAKE = $(MAKE) -C $(BUILDROOT_HOME) BR2_EXTERNAL=$(BR2_EXTERNAL_ZGC_TEE_PATH) BR2_PACKAGE_OVERRIDE_FILE=$(BUILDROOT_OVERRIDE_FILE) O=$(BUILDROOT_OUTPUT)
BUILDROOT_NEMU_DEFCONFIG ?= zgc_tee_nanhuv3a_nemu_defconfig
BUILDROOT_NEMU_DTS ?= $(BR2_EXTERNAL_ZGC_TEE_PATH)/board/nanhuv3a/nemu/system.dtb
BUILDROOT_NEMU_DTS_SRC ?= $(BR2_EXTERNAL_ZGC_TEE_PATH)/board/nanhuv3a/nemu/system.dts
BUILDROOT_NEMU_IMG ?= $(BUILDROOT_OUTPUT)/images/fw_payload.bin

.PHONY: all buildroot-check buildroot-local-overrides buildroot-nemu buildroot-nemu-local buildroot-nemu-defconfig buildroot-nemu-defconfig-local buildroot-nemu-dts clean dts help init linux nemu nemu-menu nemu-pmptable opensbi penglai-sdk run run-buildroot run-buildroot-local

all: opensbi
	@echo "make linux with Penglai-TEE success"

help:
	@echo "Legacy targets:"
	@echo "  make init            # initialize submodules and legacy rootfs flow"
	@echo "  make opensbi         # build OpenSBI + Linux with legacy flow"
	@echo "  make run             # run legacy fw_payload on NEMU"
	@echo "Buildroot/NEMU targets:"
	@echo "  make buildroot-nemu-defconfig       # reproducible git-based config"
	@echo "  make buildroot-nemu                 # reproducible git-based build"
	@echo "  make run-buildroot                  # run reproducible git-based build"
	@echo "  make buildroot-nemu-defconfig-local # local Linux/OpenSBI sources"
	@echo "  make buildroot-nemu-local           # local Linux/OpenSBI sources"
	@echo "  make run-buildroot-local            # run local-source build"

opensbi: linux dts
	$(MAKE) -C $(SBI_HOME) PLATFORM=$(PLATFORM) CROSS_COMPILE=$(CROSS_COMPILE) FW_FDT_PATH=$(FW_FDT_PATH) FW_PAYLOAD_PATH=$(FW_PAYLOAD_PATH) PLATFORM_RISCV_ISA=$(OPENSBI_PLATFORM_RISCV_ISA) NEMU_SKIP_SPMP_ENABLE=$(OPENSBI_NEMU_SKIP_SPMP_ENABLE)

linux:
	$(MAKE) -C $(LINUX_HOME) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $(LINUX_CONFIG)
	RISCV_ROOTFS_HOME=$(RISCV_ROOTFS_HOME) $(MAKE) -C $(LINUX_HOME) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) vmlinux
	cd $(LINUX_HOME); $(CROSS_COMPILE_OBJCOPY) -O binary vmlinux vmlinux.bin

dts:
	cd $(SBI_HOME)/dts; dtc -O dtb -o xiangshan.dtb $(DTS_NAME)

init:
	git submodule update --init --recursive
	cd NEMU; make riscv64-tee_defconfig; make -j8
	$(MAKE) -C $(RISCV_ROOTFS_HOME)/apps/busybox
	$(MAKE) -C $(LINUX_HOME) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) ${LINUX_INIT_CONFIG}
	RISCV_ROOTFS_HOME=$(RISCV_ROOTFS_HOME) $(MAKE) -C $(LINUX_HOME) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) vmlinux
	$(MAKE) -C $(RISCV_ROOTFS_HOME)/apps/penglai-sdk
	$(MAKE) -C $(RISCV_ROOTFS_HOME)/apps/penglai-driver
	$(MAKE) -C $(LINUX_HOME) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) ${LINUX_CONFIG}
	$(MAKE) -C $(LINUX_HOME) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) vmlinux
	@echo "initialization success"

penglai-sdk:
	$(MAKE) -C $(RISCV_ROOTFS_HOME)/apps/penglai-sdk
	$(MAKE) -C $(RISCV_ROOTFS_HOME)/apps/penglai-driver

run:
	$(NEMU_BINARY) $(IMG)

nemu:
	$(MAKE) -C $(NEMU_HOME) -j32

nemu-pmptable:
	$(MAKE) -C $(NEMU_HOME) riscv64-tee-pmptable_defconfig
	$(MAKE) -C $(NEMU_HOME) -j32

nemu-menu:
	$(MAKE) -C $(NEMU_HOME) menuconfig

buildroot-check:
	@./scripts/check-buildroot-env.sh "$(BUILDROOT_HOME)" "$(BR2_EXTERNAL_ZGC_TEE_PATH)" "$(LINUX_HOME)" "$(SBI_HOME)"

buildroot-local-overrides: buildroot-check
	@mkdir -p $(BUILDROOT_OUTPUT)
	@printf "LINUX_OVERRIDE_SRCDIR=%s\nOPENSBI_OVERRIDE_SRCDIR=%s\n" "$(LINUX_HOME)" "$(SBI_HOME)" > $(BUILDROOT_OVERRIDE_FILE)

buildroot-nemu-dts:
	@mkdir -p $(dir $(BUILDROOT_NEMU_DTS))
	dtc -O dtb -o $(BUILDROOT_NEMU_DTS) $(BUILDROOT_NEMU_DTS_SRC)

buildroot-nemu-defconfig: buildroot-check
	$(BUILDROOT_MAKE) $(BUILDROOT_NEMU_DEFCONFIG)

buildroot-nemu-defconfig-local: buildroot-local-overrides
	$(BUILDROOT_LOCAL_MAKE) $(BUILDROOT_NEMU_DEFCONFIG)

buildroot-nemu: buildroot-check buildroot-nemu-dts
	$(BUILDROOT_MAKE)

buildroot-nemu-local: buildroot-local-overrides buildroot-nemu-dts
	$(BUILDROOT_LOCAL_MAKE)

run-buildroot: buildroot-nemu nemu
	@test -f "$(BUILDROOT_NEMU_IMG)" || { echo "missing Buildroot OpenSBI payload at $(BUILDROOT_NEMU_IMG)"; exit 1; }
	$(NEMU_BINARY) $(BUILDROOT_NEMU_IMG)

run-buildroot-local: buildroot-nemu-local nemu
	@test -f "$(BUILDROOT_NEMU_IMG)" || { echo "missing Buildroot OpenSBI payload at $(BUILDROOT_NEMU_IMG)"; exit 1; }
	$(NEMU_BINARY) $(BUILDROOT_NEMU_IMG)

clean:
	$(MAKE) -C $(SBI_HOME) clean
	$(MAKE) -C $(LINUX_HOME) clean
