################################################################################
#
# platform-dtb
#
################################################################################

PLATFORM_DTB_VERSION = 1.0
PLATFORM_DTB_SOURCE =
PLATFORM_DTB_LICENSE = Proprietary
PLATFORM_DTB_DEPENDENCIES = host-dtc

PLATFORM_DTB_PLATFORM = $(call qstrip,$(BR2_PACKAGE_PLATFORM_DTB_PLATFORM))
PLATFORM_DTB_BOARD_DIR = $(BR2_EXTERNAL_ZGC_TEE_PATH)/board/nanhuv3a
PLATFORM_DTB_DTS = $(PLATFORM_DTB_BOARD_DIR)/$(PLATFORM_DTB_PLATFORM)/system.dts
PLATFORM_DTB_OUT = $(BINARIES_DIR)/platform.dtb

define PLATFORM_DTB_BUILD_CMDS
	$(HOST_DIR)/bin/dtc \
		-i $(PLATFORM_DTB_BOARD_DIR)/common \
		-i $(PLATFORM_DTB_BOARD_DIR)/$(PLATFORM_DTB_PLATFORM) \
		-I dts -O dtb \
		-o $(PLATFORM_DTB_OUT) \
		$(PLATFORM_DTB_DTS)
endef

define PLATFORM_DTB_INSTALL_IMAGES_CMDS
	@test -f $(PLATFORM_DTB_OUT)
endef

$(eval $(generic-package))
