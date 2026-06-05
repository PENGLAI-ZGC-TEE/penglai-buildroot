# penglai-buildroot

`penglai-buildroot` 是 ZGC_TEE Penglai 移植工作的 Buildroot 集成工程。当前主要维护目标是 `nanhuv3a/nemu` 和 `nanhuv3a/fpga`。

## 目录结构

- `buildroot/`：Buildroot 源码树
- `br2-external/`：ZGC_TEE 的 Buildroot external tree
- `opensbi/`：本地 OpenSBI 源码树
- `debian-linux-kernel/`：本地 Linux 源码树
- `NEMU/`：NEMU 模拟器源码树
- `riscv-rootfs/`：旧 rootfs 迁移来源，不作为长期 rootfs 构建入口

## 构建 NEMU

生成 Buildroot 配置：

```bash
make PLATFORM=nemu defconfig
```

构建 NEMU 使用的 OpenSBI payload：

```bash
make PLATFORM=nemu build
```

运行生成的 payload：

```bash
make PLATFORM=nemu run
```

## 构建 FPGA

```bash
make PLATFORM=fpga defconfig
make PLATFORM=fpga build
```

## 本地源码开发模式

默认构建使用 defconfig 中配置的 git 源码，适合可复现构建。

如果需要使用工作区里的本地 Linux/OpenSBI 源码，显式加 `LOCAL=1`：

```bash
make PLATFORM=nemu LOCAL=1 defconfig
make PLATFORM=nemu LOCAL=1 build
make PLATFORM=nemu LOCAL=1 run
```

本地模式会生成：

```text
output/<platform>-local/local.mk
```

并通过 Buildroot override 使用：

```text
debian-linux-kernel/
opensbi/
```

## 重新构建局部组件

Buildroot 依赖 stamp 判断 package 是否已经构建完成；修改 `br2-external/package/` 下的 local source package，或者在 `LOCAL=1` 模式下修改本地 Linux/OpenSBI 源码后，普通 `build` 不一定会自动重新编译对应组件。

顶层 Makefile 提供了 `rebuild` 入口：

```bash
make PLATFORM=nemu rebuild
make PLATFORM=fpga rebuild
```

`rebuild` 通过 `PLATFORM` 选择对应 Buildroot output，`nemu` 和 `fpga` 都使用同一套规则。不传 `PACKAGE` 时，默认重新构建 `br2-external/package/` 下的全部一级 package 目录；后续新增 package 目录也会自动包含进去。例如：

```text
penglai-driver platform-dtb penglai-sdk
```

`LOCAL=1` 时，默认额外重新构建本地 Linux/OpenSBI：

```bash
make PLATFORM=nemu LOCAL=1 rebuild
make PLATFORM=fpga LOCAL=1 rebuild
```

等价于重新构建这些项目包，并额外追加：

```text
linux opensbi
```

也可以显式指定 package，使用逗号分隔：

```bash
make PLATFORM=nemu rebuild PACKAGE=penglai-sdk
make PLATFORM=nemu LOCAL=1 rebuild PACKAGE=linux,opensbi,penglai-sdk
```

显式传入 `PACKAGE` 后，只会重新构建指定项，不会自动追加 `linux` 和 `opensbi`。`rebuild` 只负责重建指定组件，不等价于完整 `build`，不会自动完成最终 rootfs/payload 刷新。

## 设备树流程

设备树由 `platform-dtb` Buildroot package 生成：

```text
br2-external/board/nanhuv3a/<platform>/system.dts
  -> output/<platform>/images/platform.dtb
```

OpenSBI 通过下面的参数使用生成的 DTB：

```text
FW_FDT_PATH=$(BINARIES_DIR)/platform.dtb
```

`buildroot/` 子模块中的 OpenSBI package 已经依赖 `platform-dtb`，所以正常构建不需要手动执行 `platform-dtb-rebuild` 或 `opensbi-rebuild`。

## 注意事项

- 默认构建必须保持 git 源码模式；只有显式 `LOCAL=1` 时才使用本地源码树。
- 当前 NEMU 流程启用了 `BR2_TARGET_ROOTFS_INITRAMFS=y`。
- 重复执行 `make PLATFORM=nemu build` 时，Buildroot 仍可能重新生成 rootfs 并重新链接 kernel Image，这是 initramfs 流程导致的正常现象。
- 不要把 Linux/OpenSBI 源码复制进 Buildroot 或 `br2-external/`。
