#!/bin/sh
set -eu

buildroot_home="${1:-$(pwd)/buildroot}"
br2_external="${2:-$(pwd)/br2-external}"
linux_home="${3:-$(pwd)/debian-linux-kernel}"
opensbi_home="${4:-$(pwd)/opensbi}"

if [ ! -d "${buildroot_home}" ]; then
	echo "missing buildroot tree at ${buildroot_home}" >&2
	exit 1
fi

if [ ! -d "${br2_external}" ]; then
	echo "missing br2-external tree at ${br2_external}" >&2
	exit 1
fi

for tool in dtc make; do
	if ! command -v "${tool}" >/dev/null 2>&1; then
		echo "missing required tool: ${tool}" >&2
		exit 1
	fi
done

for tree in "${linux_home}" "${opensbi_home}"; do
	if [ ! -d "${tree}" ]; then
		echo "missing local source tree at ${tree}" >&2
		exit 1
	fi
done
