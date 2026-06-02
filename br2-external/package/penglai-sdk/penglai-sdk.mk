################################################################################
#
# penglai-sdk
#
################################################################################

PENGLAI_SDK_VERSION = 1.0
PENGLAI_SDK_SITE = $(BR2_EXTERNAL_ZGC_TEE_PATH)/package/penglai-sdk/src
PENGLAI_SDK_SITE_METHOD = local
PENGLAI_SDK_LICENSE = Proprietary
PENGLAI_SDK_INSTALL_STAGING = YES

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
		AS="$(TARGET_AS)" \
		GCC_LIB="$(shell $(TARGET_CC) -print-libgcc-file-name)"
endef

define PENGLAI_SDK_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/demo/host/host $(TARGET_DIR)/root/host
	$(INSTALL) -D -m 0755 $(@D)/demo/prime/prime $(TARGET_DIR)/root/prime
endef

define PENGLAI_SDK_INSTALL_STAGING_CMDS
	mkdir -p $(STAGING_DIR)/usr/include/penglai/app
	mkdir -p $(STAGING_DIR)/usr/include/penglai/gm
	mkdir -p $(STAGING_DIR)/usr/include/penglai/host
	cp -a $(@D)/lib/app/include/. $(STAGING_DIR)/usr/include/penglai/app/
	cp -a $(@D)/lib/gm/include/. $(STAGING_DIR)/usr/include/penglai/gm/
	cp -a $(@D)/lib/host/include/. $(STAGING_DIR)/usr/include/penglai/host/
	$(INSTALL) -D -m 0644 $(@D)/lib/libpenglai-enclave-eapp.a $(STAGING_DIR)/usr/lib/libpenglai-enclave-eapp.a
	$(INSTALL) -D -m 0644 $(@D)/lib/libpenglai-enclave-host.a $(STAGING_DIR)/usr/lib/libpenglai-enclave-host.a
endef

$(eval $(generic-package))
