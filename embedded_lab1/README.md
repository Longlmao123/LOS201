# Lab 01: Linux Kernel Build and Boot

## 1. Introduction

This laboratory builds and validates a minimal Embedded Linux system for an ARM Cortex-A9 target using QEMU's Versatile Express (`vexpress-a9`) board model. The system runs a Linux 5.15.0 kernel and a BusyBox 1.35.0 userspace from a compressed initial RAM filesystem (`initramfs`).

The repository retains the kernel, U-Boot, and BusyBox configuration files, the generated boot artifacts, and the userspace filesystem. The large upstream source trees and intermediate compilation outputs are intentionally not included.

**Verification status:** The QEMU system booted successfully and its runtime behavior was confirmed using terminal outputs supplied on **20 October 2026**. This repository documents that observed result; it does not claim that the build was independently reproduced from scratch during documentation.

## 2. Objectives

1. Configure a Linux kernel for an ARMv7-A / Cortex-A9 target.
2. Cross-compile and preserve the kernel image and Device Tree Blob (DTB).
3. Configure a compact BusyBox userspace.
4. Package userspace into a compressed `initramfs`.
5. Boot and inspect the system using QEMU.
6. Verify the running kernel, processor, init process, and mounted filesystems.

## 3. Development and Target Environment

| Item | Configuration |
| --- | --- |
| Target architecture | ARMv7-A / `armv7l` |
| CPU | ARM Cortex-A9 (`CPU part: 0xc09`) |
| Emulated board | ARM Versatile Express |
| QEMU machine | `vexpress-a9` |
| Kernel | Linux 5.15.0 |
| BusyBox | 1.35.0 |
| U-Boot build | 2022.04 (artifact retained, **not executed** in the demonstrated boot path) |
| Cross-compiler prefix | `arm-linux-gnueabihf-` |
| QEMU RAM / vCPUs | 512 MiB / 2 (`-m 512M -smp 2`) |
| Root filesystem | gzip-compressed cpio initramfs |
| Console | `ttyAMA0,115200` |
| Guest hostname | `embedded-lab` |

> This run uses **QEMU direct kernel boot** (`-kernel`, `-dtb`, `-initrd`). Although an `output/u-boot` binary is available, it is **not part of the tested boot sequence**.

## 4. Repository Organization

```text
embedded_lab1/
├── README.md
├── configs/
│   ├── busybox.config
│   ├── kernel.config
│   └── uboot.config
├── diagrams/
│   └── Embedded_Linux_Boot.pdf
├── output/
│   ├── initramfs.cpio.gz
│   ├── u-boot
│   ├── vexpress-v2p-ca9.dtb
│   └── zImage
├── report/
│   └── Lab01_Report.pdf
└── rootfs/
    └── initramfs/
        ├── init
        ├── bin/           # BusyBox and applet symlinks
        ├── sbin/          # includes /sbin/init link
        ├── etc/           # inittab, hostname, init.d/rcS
        ├── usr/
        ├── dev/           # placeholders; special nodes not stored by Git
        ├── proc/
        ├── sys/
        └── tmp/
```

The [boot diagram](diagrams/Embedded_Linux_Boot.pdf) illustrates the **tested** QEMU path; the [lab report](report/Lab01_Report.pdf) records the environment and verification evidence.

## 5. System Components

### 5.1 Linux Kernel

`output/zImage` is the compressed ARM Linux kernel. Its configuration is preserved as `configs/kernel.config`.

### 5.2 Device Tree

`output/vexpress-v2p-ca9.dtb` describes the hardware exposed by QEMU's ARM Versatile Express machine. QEMU supplies this DTB when starting the kernel.

### 5.3 BusyBox and RootFS

`rootfs/initramfs/bin/busybox` supplies a minimal shell and system utilities. Configuration is preserved as `configs/busybox.config`. Standard commands are exposed by BusyBox applet symlinks.

The repository stores regular files and symlinks from the rootfs. Git does **not** preserve special device nodes such as `/dev/console`, `/dev/null`, `/dev/zero`, or `/dev/random`. The original `output/initramfs.cpio.gz` is the boot-tested archive and should be used for the documented QEMU run. Rebuilding an equivalent archive requires recreating the necessary device nodes with correct metadata and permissions.

### 5.4 U-Boot

`output/u-boot` and `configs/uboot.config` preserve the U-Boot 2022.04 build result. This is **supplementary evidence** of the Lab 1 build environment, not a bootloader stage in the demonstrated `-kernel` QEMU command.

## 6. Userspace Initialization

The tested kernel command line contains:

```text
console=ttyAMA0,115200 rdinit=/sbin/init mem=512M
```

Therefore the kernel executes BusyBox `/sbin/init` as PID 1. BusyBox reads `/etc/inittab` and invokes `/etc/init.d/rcS`. The rootfs also includes a top-level `/init` script, but **`rdinit=/sbin/init` selects `/sbin/init` directly** in this configuration.

**`rootfs/initramfs/init`**

```sh
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
exec /sbin/init
```

**`rootfs/initramfs/etc/inittab`**

```text
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/swapoff -a
::shutdown:/bin/umount -a -r
::restart:/sbin/init
```

**`rootfs/initramfs/etc/init.d/rcS`**

```sh
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
echo 'Boot completed! Welcome to Embedded Linux Lab 1'
hostname embedded-lab
```

The `rcS` script prepares the runtime pseudo-filesystems and sets the guest hostname.

## 7. Running Lab 01 in QEMU

From `embedded_lab1/`, run:

```bash
qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel output/zImage \
  -dtb output/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

When the BusyBox prompt (`/ #`) appears, run:

```sh
uname -a
cat /proc/cpuinfo
hostname
mount
ps
```

In QEMU's `-nographic` serial console, use **Ctrl+A**, then **X** to exit the emulator.

## 8. System Verification - Observed Results

The following results were copied from the QEMU shell supplied by the student; they are **observations**, not synthetic sample output.

| Check | Observed evidence | Result |
| --- | --- | --- |
| Kernel/architecture | `Linux embedded-lab 5.15.0 ... armv7l GNU/Linux` | PASS |
| Processor | `ARMv7 Processor rev 0`, part `0xc09` | PASS |
| CPU listing | `/proc/cpuinfo` contains `processor: 0` and `processor: 1` | PASS |
| Emulated board | `Hardware: ARM-Versatile Express` | PASS |
| Hostname | `embedded-lab` | PASS |
| Root filesystem | `rootfs on / type rootfs (rw,...)` | PASS |
| procfs | `none on /proc type proc` | PASS |
| sysfs | `none on /sys type sysfs` | PASS |
| tmpfs | `none on /tmp type tmpfs` | PASS |
| Init process | `PID 1 ... init` in `ps` | PASS |
| Interactive shell | `/ #` prompt and successful commands | PASS |

**Actual `uname -a`:**

```text
Linux embedded-lab 5.15.0 #1 SMP Mon Oct 5 16:09:28 +07 2026 armv7l GNU/Linux
```

**Actual mount output:**

```text
rootfs on / type rootfs (rw,size=244912k,nr_inodes=61228)
none on /proc type proc (rw,relatime)
none on /sys type sysfs (rw,relatime)
none on /tmp type tmpfs (rw,relatime)
```

**Process evidence:** `ps` shows PID 1 as `init`, an interactive `-/bin/sh`, and kernel threads. This confirms the initial userspace reached an interactive prompt.

## 9. Reproducibility and Limitations

- This upload excludes upstream Linux, BusyBox, and U-Boot source trees and multi-gigabyte intermediate build objects.
- The original toolchain, source releases, and saved `.config` files are needed to rebuild equivalent artifacts.
- Git cannot directly store device nodes. Recreate these when assembling a fresh rootfs and `initramfs.cpio.gz`.
- The demonstrated QEMU run does not exercise U-Boot, TFTP, NAND/NOR boot, or real hardware; no claims are made about those paths.
- The terminal outputs confirm system startup and fundamental operations, not exhaustive hardware-driver or performance testing.
- The report uses textual verification evidence; no terminal screenshots were supplied for inclusion.

## 10. Conclusion

Lab 01 successfully booted a minimal ARMv7 Embedded Linux system on QEMU's Versatile Express platform. The system reported Linux 5.15.0, two Cortex-A9 CPUs in `/proc/cpuinfo`, the expected hostname, mounted procfs/sysfs/tmpfs, and BusyBox init as PID 1. The repository preserves the tested boot artifacts and relevant configurations in a compact, reviewable layout.
