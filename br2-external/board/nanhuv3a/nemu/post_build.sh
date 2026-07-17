#!/bin/sh
set -eu

chmod 0755 "${TARGET_DIR}/etc/init.d/rcS"
find "${TARGET_DIR}/etc/init.d" -maxdepth 1 -type f -name "S[0-9][0-9]*" -exec chmod 0755 {} +
rm -f "${TARGET_DIR}/etc/init.d/S40network"
