#!/bin/sh
set -eu

chmod 0755 "${TARGET_DIR}/etc/init.d/rcS"
chmod 0755 "${TARGET_DIR}/etc/init.d/S50dsid"

mkdir -p "${TARGET_DIR}/root/scripts"

if [ -d "${BR2_EXTERNAL_ZGC_TEE_PATH}/../riscv-rootfs/rootfsimg/scripts" ]; then
	cp -a "${BR2_EXTERNAL_ZGC_TEE_PATH}/../riscv-rootfs/rootfsimg/scripts/." "${TARGET_DIR}/root/scripts/"
	find "${TARGET_DIR}/root/scripts" -type f -name "*.sh" -exec chmod 0755 {} +
fi
