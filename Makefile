PROJECT_ROOT := $(CURDIR)

export SBI_HOME := $(PROJECT_ROOT)/opensbi
export LINUX_HOME := $(PROJECT_ROOT)/debian-linux-kernel
export NEMU_HOME := $(PROJECT_ROOT)/NEMU
export BUILDROOT_HOME ?= $(PROJECT_ROOT)/buildroot
export BR2_EXTERNAL_ZGC_TEE_PATH ?= $(PROJECT_ROOT)/br2-external

SUPPORTED_PLATFORMS := nemu fpga
PLATFORM ?= nemu
LOCAL ?= 0

ifeq ($(filter $(PLATFORM),$(SUPPORTED_PLATFORMS)),)
$(error unsupported PLATFORM=$(PLATFORM), expected one of: $(SUPPORTED_PLATFORMS))
endif

LOCAL_ENABLED := $(filter 1 y yes true,$(LOCAL))
OUTPUT_SUFFIX := $(if $(LOCAL_ENABLED),-local)
BUILDROOT_OUTPUT_DIR ?= $(PROJECT_ROOT)/output/$(PLATFORM)$(OUTPUT_SUFFIX)
BUILDROOT_OVERRIDE_FILE ?= $(BUILDROOT_OUTPUT_DIR)/local.mk
BUILDROOT_DEFCONFIG ?= zgc_tee_nanhuv3a_$(PLATFORM)_defconfig
BUILDROOT_JOBS ?= $(shell nproc 2>/dev/null || echo 8)
BUILDROOT_ENV := env -u LD_LIBRARY_PATH
BUILDROOT_OVERRIDE_ARG := $(if $(LOCAL_ENABLED),BR2_PACKAGE_OVERRIDE_FILE=$(BUILDROOT_OVERRIDE_FILE))
BUILDROOT_MAKE = $(BUILDROOT_ENV) $(MAKE) -j$(BUILDROOT_JOBS) -C $(BUILDROOT_HOME) BR2_EXTERNAL=$(BR2_EXTERNAL_ZGC_TEE_PATH) $(BUILDROOT_OVERRIDE_ARG) O=$(BUILDROOT_OUTPUT_DIR)

NEMU_BINARY := $(NEMU_HOME)/build/riscv64-nemu-interpreter
PAYLOAD_BIN := $(BUILDROOT_OUTPUT_DIR)/images/fw_payload.bin

.PHONY: help buildroot-check prepare-output defconfig build run nemu menuconfig clean-output

help:
	@echo "Buildroot flow:"
	@echo "  make PLATFORM=nemu defconfig"
	@echo "  make PLATFORM=nemu build"
	@echo "  make PLATFORM=nemu run"
	@echo "  make PLATFORM=fpga defconfig"
	@echo "  make PLATFORM=fpga build"
	@echo ""
	@echo "Local source override:"
	@echo "  make PLATFORM=nemu LOCAL=1 defconfig"
	@echo "  make PLATFORM=nemu LOCAL=1 build"
	@echo ""
	@echo "Variables:"
	@echo "  PLATFORM=$(SUPPORTED_PLATFORMS)"
	@echo "  LOCAL=1 uses local Linux/OpenSBI sources in a separate output tree"
	@echo "  BUILDROOT_OUTPUT_DIR=$(BUILDROOT_OUTPUT_DIR)"

buildroot-check:
	@./scripts/check-buildroot-env.sh "$(BUILDROOT_HOME)" "$(BR2_EXTERNAL_ZGC_TEE_PATH)" "$(LINUX_HOME)" "$(SBI_HOME)"

prepare-output: buildroot-check
	@mkdir -p $(BUILDROOT_OUTPUT_DIR)
ifneq ($(LOCAL_ENABLED),)
	@printf "LINUX_OVERRIDE_SRCDIR=%s\nOPENSBI_OVERRIDE_SRCDIR=%s\n" "$(LINUX_HOME)" "$(SBI_HOME)" > $(BUILDROOT_OVERRIDE_FILE)
else
	@$(RM) $(BUILDROOT_OVERRIDE_FILE)
endif

defconfig: prepare-output
	+$(BUILDROOT_MAKE) $(BUILDROOT_DEFCONFIG)

build: prepare-output
	+$(BUILDROOT_MAKE)

ifeq ($(PLATFORM),nemu)
run: build nemu
	@test -f "$(PAYLOAD_BIN)" || { echo "missing Buildroot OpenSBI payload at $(PAYLOAD_BIN)"; exit 1; }
	$(NEMU_BINARY) -b $(PAYLOAD_BIN)
else
run: build
	@echo "FPGA payload: $(PAYLOAD_BIN)"
endif

nemu:
	+$(MAKE) -C $(NEMU_HOME) -j$(BUILDROOT_JOBS)

menuconfig: prepare-output
	+$(BUILDROOT_MAKE) menuconfig

clean-output:
	+$(BUILDROOT_MAKE) clean
