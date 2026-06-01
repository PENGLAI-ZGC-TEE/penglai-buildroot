################################################################################
#
# hello
#
################################################################################

PENGLAI_HELLO_VERSION = 1.0
PENGLAI_HELLO_SITE = $(BR2_EXTERNAL_ZGC_TEE_PATH)/../riscv-rootfs/apps/hello
PENGLAI_HELLO_SITE_METHOD = local
PENGLAI_HELLO_LICENSE = Proprietary

define PENGLAI_HELLO_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $(@D)/hello $(@D)/hello.c
endef

define PENGLAI_HELLO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/hello $(TARGET_DIR)/usr/bin/hello
endef

$(eval $(generic-package))
