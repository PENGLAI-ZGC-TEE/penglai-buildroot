# PENGLAI ZGC TEE SPMP

Workspace for the ZGC_TEE Penglai port, currently focused on the
`nanhuv3a/nemu` Buildroot flow.

## Current Buildroot NEMU Flow

Generate the Buildroot configuration:

```bash
make buildroot-nemu-defconfig
```

Build the NEMU image:

```bash
make buildroot-nemu
```

Run the generated OpenSBI payload on NEMU:

```bash
make run-buildroot
```

## Local Source Development

Use the local-source targets when developing against the workspace Linux or
OpenSBI trees:

```bash
make buildroot-nemu-defconfig-local
make buildroot-nemu-local
make run-buildroot-local
```

The legacy Makefile flow is still present for comparison, but the active
migration target is the Buildroot NEMU flow.
