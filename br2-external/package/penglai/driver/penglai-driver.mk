################################################################################
#
# penglai-driver
#
################################################################################

PENGLAI_DRIVER_VERSION = 1.0
PENGLAI_DRIVER_SITE = $(BR2_EXTERNAL_ZGC_TEE_PATH)/package/penglai/driver/src
PENGLAI_DRIVER_SITE_METHOD = local
PENGLAI_DRIVER_LICENSE = Proprietary
PENGLAI_DRIVER_DEPENDENCIES = linux

define PENGLAI_DRIVER_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(LINUX_DIR) \
		ARCH=$(KERNEL_ARCH) \
		CROSS_COMPILE=$(TARGET_CROSS) \
		M=$(@D) \
		modules
endef

define PENGLAI_DRIVER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/penglai.ko $(TARGET_DIR)/root/penglai/penglai.ko
endef

$(eval $(generic-package))
