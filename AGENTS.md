# AGENTS

This repository is the `ZGC_TEE` workspace. Current active targets are `nanhuv3a/nemu` and `nanhuv3a/fpga`.

Main source trees:

- `buildroot/`: Buildroot checkout
- `br2-external/`: ZGC_TEE Buildroot external tree
- `opensbi/`: local OpenSBI source tree
- `debian-linux-kernel/`: local Linux source tree
- `NEMU/`: simulator source tree
- `riscv-rootfs/`: migration source content only, not the long-term rootfs entry

## Build Entry

Use the top-level Makefile:

- `make PLATFORM=nemu defconfig`
- `make PLATFORM=nemu build`
- `make PLATFORM=nemu run`
- `make PLATFORM=fpga defconfig`
- `make PLATFORM=fpga build`

`PLATFORM` currently supports `nemu` and `fpga`.

## Source Modes

Default builds use reproducible git sources from the Buildroot defconfig:

- `make PLATFORM=nemu build`
- `LOCAL=0` is the default
- output directory: `output/<platform>/`

Local source override mode must be explicit:

- `make PLATFORM=nemu LOCAL=1 build`
- generates `output/<platform>-local/local.mk`
- overrides Buildroot Linux/OpenSBI sources with local `debian-linux-kernel/` and `opensbi/`

Keep git mode and local mode clearly separated. Do not make default builds consume local workspace trees.

## Rebuild Helper

Use the top-level `rebuild` target for explicit package rebuilds when source changes are not picked up by Buildroot stamps:

- `make PLATFORM=nemu rebuild`
- `make PLATFORM=fpga rebuild`
- `make PLATFORM=nemu LOCAL=1 rebuild`
- `make PLATFORM=fpga LOCAL=1 rebuild`
- `make PLATFORM=nemu rebuild PACKAGE=penglai-sdk`
- `make PLATFORM=nemu LOCAL=1 rebuild PACKAGE=linux,opensbi,penglai-sdk`

The helper is platform-generic. `PLATFORM` selects the Buildroot output tree and defconfig family; only `nemu` has run targets.

When `PACKAGE` is omitted, `rebuild` defaults to every directory under `br2-external/package/`. In `LOCAL=1` mode it also appends `linux opensbi` so local kernel/OpenSBI source edits are rebuilt.

When `PACKAGE` is set, only the specified comma-separated packages are rebuilt. Local-source packages listed in the top-level `LOCAL_SOURCE_PACKAGES` use `dirclean` before rebuild so Buildroot re-syncs their source tree. `rebuild` is not a full `build`; it does not guarantee final rootfs or payload regeneration.

## DTB Flow

Device trees are built by the Buildroot package:

- package: `br2-external/package/platform-dtb/`
- source DTS: `br2-external/board/nanhuv3a/<platform>/system.dts`
- generated DTB: `output/<platform>/images/platform.dtb`

OpenSBI consumes the generated DTB with:

- `FW_FDT_PATH=$(BINARIES_DIR)/platform.dtb`

The `buildroot/` submodule patches `boot/opensbi/opensbi.mk` so OpenSBI depends on `platform-dtb` when `BR2_PACKAGE_PLATFORM_DTB=y`. Do not reintroduce top-level `platform-dtb-rebuild opensbi-rebuild` sequencing for normal builds.

## Constraints

- Keep Buildroot integration in `br2-external/`, top-level orchestration, or intentional Buildroot package patches.
- Do not copy Linux/OpenSBI source into Buildroot or `br2-external/`.
- Do not change the NEMU device model to hide Buildroot/DTS/config migration issues.
- `BR2_TARGET_ROOTFS_INITRAMFS=y` is intentional for the current NEMU flow.
- Repeated `make PLATFORM=nemu build` may still regenerate rootfs images and relink the kernel Image because initramfs is enabled.
- The pinned OpenSBI revision includes the Buildroot toolchain, Xilinx UARTLite, NEMU SPMP, and PMA boot-print fixes; do not reintroduce duplicate external patches.

## Packages

Project packages are included through `br2-external/package/Config.in`:

- `br2-external/package/platform-dtb/`
- `br2-external/package/penglai-driver/`
- `br2-external/package/penglai-sdk/`
