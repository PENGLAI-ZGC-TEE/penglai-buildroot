# AGENTS

This repository is a workspace-style project built around several source trees:

- `opensbi/`: local OpenSBI source tree
- `debian-linux-kernel/`: local Linux source tree
- `NEMU/`: simulator source tree
- `riscv-rootfs/`: legacy rootfs content, currently being migrated
- `buildroot/`: Buildroot checkout
- `br2-external/`: ZGC_TEE external tree for project integration

## Project Shape

This repository represents the larger `ZGC_TEE` project.

- `nanhuv3a` is one project line under `ZGC_TEE`
- supported target families are expected to include `nemu`, `qemu`, `fpga`, and `board`
- current active integration target is `nanhuv3a/nemu`

Relevant Buildroot board layout:

- `br2-external/board/nanhuv3a/common/`
- `br2-external/board/nanhuv3a/nemu/`
- `br2-external/board/nanhuv3a/qemu/`
- `br2-external/board/nanhuv3a/fpga/`
- `br2-external/board/nanhuv3a/board/`

## Build Modes

There are two Buildroot source modes:

1. Reproducible git mode
   - `make buildroot-nemu-defconfig`
   - `make buildroot-nemu`
   - `make run-buildroot`
   - uses the git-based Linux/OpenSBI source settings from `br2-external/configs/zgc_tee_nanhuv3a_nemu_defconfig`

2. Local source override mode
   - `make buildroot-nemu-defconfig-local`
   - `make buildroot-nemu-local`
   - `make run-buildroot-local`
   - generates `output/nemu/local.mk`
   - overrides Buildroot sources with local `debian-linux-kernel/` and `opensbi/`

Default `make buildroot-nemu` must stay on git mode.
Local workspace trees must only be used through the `*-local` targets.

## Current DTS Path

For the current NEMU flow:

- source DTS: `opensbi/dts/system.dts`
- generated DTB: `br2-external/board/nanhuv3a/nemu/system.dtb`

Buildroot passes that generated DTB to OpenSBI.

## Important Constraints

- Do not change the NEMU device model just to make Buildroot boot.
  Legacy `make run` already shows OpenSBI/Linux output, so boot regressions should be treated as migration/config mismatches first.
- Keep `buildroot-nemu` and `buildroot-nemu-local` behavior clearly separated.
- Keep Buildroot integration changes in `br2-external/` or top-level orchestration where possible.
- Avoid copying Linux/OpenSBI source into Buildroot or `br2-external/`.
- `riscv-rootfs/` should be treated as migration source content, not the long-term rootfs build entry.

## Known Buildroot Notes

- `br2-external/configs/zgc_tee_nanhuv3a_nemu_defconfig` is the current active defconfig.
- `BR2_TARGET_ROOTFS_INITRAMFS=y` is intentionally enabled to stay closer to the legacy NEMU boot flow.
- OpenSBI needs the external patch under `br2-external/patches/opensbi/` to disable stack protector in the bare-metal build under the current Buildroot toolchain.

## Penglai Packages

Penglai-related packages are grouped under:

- `br2-external/package/penglai/hello/`
- `br2-external/package/penglai/driver/`
- `br2-external/package/penglai/sdk/`

They are included through `br2-external/package/Config.in`.
