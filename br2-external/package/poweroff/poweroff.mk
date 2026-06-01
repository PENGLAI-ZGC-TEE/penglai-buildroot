################################################################################
#
# poweroff
#
################################################################################

POWEROFF_VERSION = 1.0
POWEROFF_SITE = $(BR2_EXTERNAL_ZGC_TEE_PATH)/../riscv-rootfs/apps/poweroff
POWEROFF_SITE_METHOD = local
POWEROFF_LICENSE = Proprietary

define POWEROFF_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $(@D)/poweroff-demo $(@D)/poweroff.c
endef

define POWEROFF_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/poweroff-demo $(TARGET_DIR)/usr/bin/poweroff-demo
endef

$(eval $(generic-package))
