#!/bin/sh
set -eu

mkdir -p "${BINARIES_DIR}/nemu"

for file in fw_payload.bin fw_payload.elf Image rootfs.cpio rootfs.cpio.gz vmlinux; do
	if [ -f "${BINARIES_DIR}/${file}" ]; then
		cp -f "${BINARIES_DIR}/${file}" "${BINARIES_DIR}/nemu/${file}"
	fi
done
