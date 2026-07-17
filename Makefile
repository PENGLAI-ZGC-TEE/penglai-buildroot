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
EXTERNAL_PACKAGES := $(shell find $(BR2_EXTERNAL_ZGC_TEE_PATH)/package -mindepth 1 -maxdepth 1 -type d -printf '%f ')
LOCAL_SOURCE_PACKAGES := penglai-driver penglai-sdk
comma := ,
empty :=
space := $(empty) $(empty)
REBUILD_PACKAGES := $(or $(strip $(subst $(comma),$(space),$(PACKAGE))),packages)
REBUILD_PACKAGE_LIST := $(strip $(foreach pkg,$(REBUILD_PACKAGES),$(if $(filter packages,$(pkg)),$(EXTERNAL_PACKAGES),$(pkg))))
DEFAULT_LOCAL_REBUILD_PACKAGES := $(if $(LOCAL_ENABLED),linux opensbi)
EFFECTIVE_REBUILD_PACKAGE_LIST := $(strip $(REBUILD_PACKAGE_LIST) $(if $(strip $(PACKAGE)),,$(DEFAULT_LOCAL_REBUILD_PACKAGES)))

NEMU_BINARY := $(NEMU_HOME)/build/riscv64-nemu-interpreter
NEMU_DEFCONFIG ?= riscv64-nanhuv3a_defconfig
PAYLOAD_BIN := $(BUILDROOT_OUTPUT_DIR)/images/fw_payload.bin

.PHONY: help buildroot-check prepare-output defconfig build rebuild nemu-init nemu-build menuconfig clean-output

help:
	@echo "Buildroot flow:"
	@echo "  make PLATFORM=nemu defconfig"
	@echo "  make PLATFORM=nemu build"
	@echo "  make PLATFORM=nemu rebuild"
	@echo "  make PLATFORM=nemu LOCAL=1 rebuild"
	@echo "  make PLATFORM=nemu rebuild PACKAGE=linux,opensbi,penglai-sdk"
	@echo "  make PLATFORM=nemu rebuild PACKAGE=packages"
	@echo "  make nemu-init"
	@echo "  make nemu-build"
	@echo "  make PLATFORM=nemu run"
	@echo "  make PLATFORM=nemu run-direct"
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
	+$(BUILDROOT_MAKE) opensbi-rebuild

rebuild: prepare-output
	@set -e; \
	for pkg in $(EFFECTIVE_REBUILD_PACKAGE_LIST); do \
		if echo " $(LOCAL_SOURCE_PACKAGES) " | grep -q " $$pkg "; then \
			echo "Rebuilding $$pkg from local source"; \
			$(BUILDROOT_MAKE) "$$pkg-dirclean" "$$pkg"; \
		else \
			echo "Rebuilding $$pkg"; \
			$(BUILDROOT_MAKE) "$$pkg-rebuild"; \
		fi; \
	done

ifeq ($(PLATFORM),nemu)
.PHONY: run run-direct

run: build $(NEMU_BINARY)
	@test -f "$(PAYLOAD_BIN)" || { echo "missing Buildroot OpenSBI payload at $(PAYLOAD_BIN)"; exit 1; }
	$(NEMU_BINARY) -b $(PAYLOAD_BIN)

run-direct: $(NEMU_BINARY)
	@test -f "$(PAYLOAD_BIN)" || { echo "missing Buildroot OpenSBI payload at $(PAYLOAD_BIN)"; exit 1; }
	$(NEMU_BINARY) -b $(PAYLOAD_BIN)
endif

nemu-init:
	+$(MAKE) -C $(NEMU_HOME) $(NEMU_DEFCONFIG)

nemu-build:
	+$(MAKE) -C $(NEMU_HOME) -j$(BUILDROOT_JOBS)

$(NEMU_BINARY):
	+$(MAKE) nemu-build

menuconfig: prepare-output
	+$(BUILDROOT_MAKE) menuconfig

clean-output:
	+$(BUILDROOT_MAKE) clean
