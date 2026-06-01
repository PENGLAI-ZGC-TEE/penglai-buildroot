################################################################################
#
# penglai-sdk
#
################################################################################

PENGLAI_SDK_VERSION = 1.0
PENGLAI_SDK_SITE = $(BR2_EXTERNAL_ZGC_TEE_PATH)/../riscv-rootfs/apps/penglai-sdk/repo
PENGLAI_SDK_SITE_METHOD = local
PENGLAI_SDK_LICENSE = Proprietary

define PENGLAI_SDK_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/musl CROSS_COMPILE=$(TARGET_CROSS)
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/lib CROSS_COMPILE=$(TARGET_CROSS)
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/demo/host \
		PENGLAI_SDK=$(@D) \
		CC="$(TARGET_CC)"
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/demo/prime \
		PENGLAI_SDK=$(@D) \
		CC="$(TARGET_CC)" \
		LINK="$(TARGET_LD)" \
		AS="$(TARGET_AS)"
endef

define PENGLAI_SDK_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/demo/host/host $(TARGET_DIR)/root/host
	$(INSTALL) -D -m 0755 $(@D)/demo/prime/prime $(TARGET_DIR)/root/prime
endef

$(eval $(generic-package))
