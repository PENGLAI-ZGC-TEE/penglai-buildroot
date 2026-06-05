################################################################################
#
# crypto_demos_scalar
#
################################################################################

CRYPTO_DEMOS_SCALAR_VERSION = 1.0
CRYPTO_DEMOS_SCALAR_SITE = $(BR2_EXTERNAL_ZGC_TEE_PATH)/package/crypto_demos_scalar
CRYPTO_DEMOS_SCALAR_SITE_METHOD = local
CRYPTO_DEMOS_SCALAR_LICENSE = Proprietary

CRYPTO_DEMOS_SCALAR_TARGETS = \
	aes_ctr_inline \
	sha256_inline \
	sm4_ecb_inline \
	sm3_inline \
	aes_gcm_inline

define CRYPTO_DEMOS_SCALAR_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		INLINE=1
endef

define CRYPTO_DEMOS_SCALAR_INSTALL_TARGET_CMDS
	$(foreach target,$(CRYPTO_DEMOS_SCALAR_TARGETS), \
		$(INSTALL) -D -m 0755 $(@D)/$(target) $(TARGET_DIR)/root/crypto_demos_scalar/$(target)
	)
endef

$(eval $(generic-package))
