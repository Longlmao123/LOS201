# Lab 01 — Linux Kernel Build, Initramfs and QEMU Boot

> **Project:** Embedded Linux Systems (LOS201)<br>
> **Target:** ARMv7-A / Cortex-A9, QEMU `vexpress-a9`<br>
> **Verified:** QEMU boot, Linux 5.15.0, BusyBox init, serial shell, and essential virtual filesystems<br>
> **Practical focus:** Find the failing stage, check the right file or configuration, then apply a targeted fix.

## Table of contents

1. [Overview and scope](#1-overview-and-scope)
2. [What was verified](#2-what-was-verified)
3. [Development environment](#3-development-environment)
4. [Repository structure](#4-repository-structure)
5. [Boot architecture and initialization](#5-boot-architecture-and-initialization)
6. [Run the existing Lab 01 artifacts](#6-run-the-existing-lab-01-artifacts)
7. [Verify the running system](#7-verify-the-running-system)
8. [Rebuild guide and checkpoints](#8-rebuild-guide-and-checkpoints)
9. [Troubleshooting by symptom](#9-troubleshooting-by-symptom)
10. [First checks when something fails](#10-first-checks-when-something-fails)
11. [Publishing updates to GitHub](#11-publishing-updates-to-github)
12. [Reproducibility and limitations](#12-reproducibility-and-limitations)
13. [Conclusion](#13-conclusion)

---

## 1. Overview and scope

This lab sets up a minimal Embedded Linux environment for **ARM Cortex-A9**, using **Linux 5.15.0**, **BusyBox 1.35.0**, a **Device Tree Blob (DTB)**, and a compressed **cpio initramfs**. It was booted on **QEMU ARM Versatile Express (`vexpress-a9`)** and checked from its interactive serial console.

The lab illustrates how these components work together:

```text
Ubuntu development host
    |
    +-- ARM cross-compiler + Linux sources -> zImage + vexpress-v2p-ca9.dtb
    +-- BusyBox + startup scripts --------> rootfs/ -> initramfs.cpio.gz
    |
    +-- QEMU -kernel/-dtb/-initrd
            |
            v
       Linux kernel -> /sbin/init (PID 1) -> /etc/inittab
                                            -> /etc/init.d/rcS
                                            -> BusyBox shell (/ #)
```

**Important distinction:** The tested QEMU command uses **direct kernel boot**. Although `output/u-boot` and `configs/uboot.config` are retained, the verified sequence **does not execute U-Boot**. U-Boot-specific rebuilding and booting require separate verification.

This repository is a compact submission with **configs, rootfs, output artifacts, report, and diagram**, not a complete copy of the upstream Linux/U-Boot/BusyBox source trees or the entire 2.4 GB development workspace.

## 2. What was verified

The following items are based on terminal output recorded from the student's successful QEMU session on **20 October 2026**, not invented example results.

| Test | Observed result | Status |
| --- | --- | --- |
| Boot completes | Interactive `/ #` prompt | **PASS** |
| Kernel version | Linux `5.15.0`, architecture `armv7l` | **PASS** |
| CPU | ARMv7 CPU part `0xc09` (Cortex-A9); `processor: 0` and `processor: 1` listed | **PASS** |
| Board | `Hardware: ARM-Versatile Express` | **PASS** |
| Hostname | `embedded-lab` | **PASS** |
| Root filesystem | `rootfs on / type rootfs` | **PASS** |
| procfs | `/proc` is mounted as `proc` | **PASS** |
| sysfs | `/sys` is mounted as `sysfs` | **PASS** |
| tmpfs | `/tmp` is mounted as `tmpfs` | **PASS** |
| init process | PID 1 shown as `init` | **PASS** |

Recorded kernel identity:

```text
Linux embedded-lab 5.15.0 #1 SMP Mon Oct 5 16:09:28 +07 2026 armv7l GNU/Linux
```

Recorded mounts:

```text
rootfs on / type rootfs (rw,size=244912k,nr_inodes=61228)
none on /proc type proc (rw,relatime)
none on /sys type sysfs (rw,relatime)
none on /tmp type tmpfs (rw,relatime)
```

These checks demonstrate **basic system startup**. They do not prove that every driver, U-Boot path, storage controller, or network interface works.

## 3. Development environment

| Component | Lab 01 configuration |
| --- | --- |
| Development host | Ubuntu Linux |
| Target ISA | ARMv7-A (`armv7l`) |
| Emulated processor | Cortex-A9 |
| Emulated platform | ARM Versatile Express |
| QEMU machine option | `-M vexpress-a9` |
| Linux kernel | `5.15.0` |
| BusyBox | `1.35.0` |
| U-Boot artifact | `2022.04` (retained, not part of verified direct boot) |
| Cross-compiler prefix | `arm-linux-gnueabihf-` |
| RAM | `512M` |
| Emulated CPU count requested | `-smp 2` |
| Serial console | `ttyAMA0,115200` |
| Root filesystem format | `initramfs.cpio.gz` (cpio/newc + gzip) |

### 3.1 Settings confirmed in the checked-in configs

The **actual configuration files in this repository** include the following settings:

```text
# configs/kernel.config
CONFIG_ARCH_VEXPRESS=y
CONFIG_CPU_V7=y
CONFIG_AEABI=y
CONFIG_SMP=y
CONFIG_BLK_DEV_INITRD=y
CONFIG_RD_GZIP=y
CONFIG_SERIAL_AMBA_PL011=y
CONFIG_SERIAL_AMBA_PL011_CONSOLE=y

# configs/busybox.config
CONFIG_STATIC=y
CONFIG_INIT=y
CONFIG_ASH=y
CONFIG_CROSS_COMPILER_PREFIX="arm-linux-gnueabihf-"

# configs/uboot.config
CONFIG_TOOLS_FIT_SIGNATURE=y
```

`CONFIG_STATIC=y` means **the saved BusyBox configuration requests a static binary**. Always confirm the actual executable using `file` when diagnosing a changed build. The U-Boot setting also matters if the *host tools* fail to build, but it does not affect the already tested direct-kernel boot.

### 3.2 Check host tools before proceeding

Run these **on Ubuntu**, not inside the BusyBox `/ #` prompt:

```bash
command -v qemu-system-arm
qemu-system-arm --version
command -v arm-linux-gnueabihf-gcc
arm-linux-gnueabihf-gcc --version
command -v make
command -v cpio
command -v file
```

If a command is missing, install the appropriate package/toolchain before building. For example, on a compatible Ubuntu host:

```bash
sudo apt update
sudo apt install qemu-system-arm make bc bison flex libssl-dev libelf-dev \
  libncurses-dev cpio gzip file device-tree-compiler gcc-arm-linux-gnueabihf
```

> A distribution-supplied compiler may not be identical to the toolchain used to create the saved binaries. **Check the actual version and target** before rebuilding. These package-install instructions are a reference procedure; they were not part of the recorded boot verification.

### 3.3 Host shell versus guest shell

| Prompt | Where you are | What you can do |
| --- | --- | --- |
| `locvhm@...:~/LOS201$` | Ubuntu host | Build, run QEMU, use Git, inspect files |
| `/ #` | QEMU guest | Check `uname`, `mount`, `ps`, BusyBox and runtime status |

Do not run `git push`, `apt`, or host path commands (`~/LOS201`) inside the guest.

## 4. Repository structure

```text
embedded_lab1/
├── README.md
├── configs/
│   ├── kernel.config          # Saved Linux kernel configuration
│   ├── busybox.config         # Saved BusyBox configuration
│   └── uboot.config           # Saved U-Boot configuration
├── diagrams/
│   └── Embedded_Linux_Boot.pdf
├── output/
│   ├── zImage                 # Boot-tested ARM kernel
│   ├── vexpress-v2p-ca9.dtb   # Boot-tested Device Tree Blob
│   ├── initramfs.cpio.gz      # Boot-tested initial root filesystem
│   └── u-boot                 # Separate U-Boot build artifact
├── report/
│   └── Lab01_Report.pdf
└── rootfs/
    └── initramfs/
        ├── init
        ├── bin/               # BusyBox and applet symlinks
        ├── sbin/              # /sbin/init -> BusyBox
        ├── usr/
        ├── etc/
        │   ├── inittab
        │   ├── hostname
        │   └── init.d/rcS
        ├── dev/               # Git placeholders; device nodes not preserved
        ├── proc/
        ├── sys/
        └── tmp/
```

Related documentation: [Boot diagram](diagrams/Embedded_Linux_Boot.pdf) · [Lab report](report/Lab01_Report.pdf).

**Do not confuse these two things:**

- `rootfs/initramfs/` is a **source tree for inspection/editing**. Git supports regular files and symlinks, but **not character/block device nodes**.
- `output/initramfs.cpio.gz` is the **previously generated archive actually used in the successful boot**. It may contain metadata and device nodes absent from the Git tree.

Changing files in `rootfs/initramfs/` does **not automatically update** `output/initramfs.cpio.gz`.

## 5. Boot architecture and initialization

### 5.1 What the QEMU options mean

| Option | Purpose | If incorrect, check... |
| --- | --- | --- |
| `-M vexpress-a9` | Emulate the supported board | Machine list, correct DTB |
| `-cpu cortex-a9` | Target CPU model | ISA compatibility |
| `-m 512M` | Guest RAM allocation | Kernel memory parameters |
| `-smp 2` | Request two vCPUs | `/proc/cpuinfo`, kernel SMP support |
| `-nographic` | Use terminal/serial console | `console=ttyAMA0,115200` |
| `-kernel output/zImage` | Load the ARM Linux kernel | Kernel format and build output |
| `-dtb output/vexpress-v2p-ca9.dtb` | Describe emulated hardware | DTB matches vexpress-a9 |
| `-initrd output/initramfs.cpio.gz` | Supply initial userspace archive | gzip/cpio and `/sbin/init` |
| `-append '...'` | Pass kernel parameters | Console, `rdinit`, memory |

### 5.2 Startup stages

```text
[1] QEMU emulates the board
       |
[2] QEMU loads zImage + DTB + initramfs
       |
[3] Kernel configures CPU, memory, timer, drivers, scheduler
       |
[4] Kernel unpacks initramfs
       |
[5] Kernel executes /sbin/init  <-- rdinit=/sbin/init
       |
[6] BusyBox init reads /etc/inittab
       |
[7] ::sysinit: runs /etc/init.d/rcS
       |           mounts /proc, /sys, /tmp; sets hostname
[8] ::askfirst: starts /bin/sh -> / #
```

**Note:** `rdinit=/sbin/init` selects `/sbin/init` from initramfs. The separate top-level `/init` script is present, but is **not the entry point** of the tested QEMU command.

### 5.3 Actual startup files

`rootfs/initramfs/init`:

```sh
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
exec /sbin/init
```

`rootfs/initramfs/etc/inittab`:

```text
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/swapoff -a
::shutdown:/bin/umount -a -r
::restart:/sbin/init
```

`rootfs/initramfs/etc/init.d/rcS`:

```sh
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
echo 'Boot completed! Welcome to Embedded Linux Lab 1'
hostname embedded-lab
```

If the console opens but `/proc` is empty or the hostname is wrong, examine **`inittab` and `rcS` first**, rather than rebuilding the entire kernel.

## 6. Run the existing Lab 01 artifacts

### 6.1 Inspect before booting

From the Ubuntu host repository:

```bash
cd ~/LOS201/embedded_lab1
pwd
ls -lh output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz
file output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz
gzip -t output/initramfs.cpio.gz
```

Expected: the three files exist and `gzip -t` exits successfully (normally no output). Check `$?` immediately after a command if unsure:

```bash
echo $?
```

For the archive listing (inspection only):

```bash
gzip -dc output/initramfs.cpio.gz | cpio -t | grep -E '^\.?/?(init|sbin/init|bin/busybox|etc/inittab|etc/init.d/rcS)$'
```

If the listing command prints nothing, inspect the complete archive first, because member names may have a leading `./` or differ from expectations:

```bash
gzip -dc output/initramfs.cpio.gz | cpio -t | head -40
```

### 6.2 Boot command (successfully used)

```bash
cd ~/LOS201/embedded_lab1

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

Wait until you see the BusyBox prompt:

```text
/ #
```

**Exit QEMU:** Press **Ctrl+A**, release, then **X** (in `-nographic` mode). This is a QEMU escape sequence; do not use `Ctrl+C` expecting to exit the entire emulator reliably.

**Never replace `output/` with rebuilt artifacts until the new images are checked.** Copy experimental images to a separate directory first.

## 7. Verify the running system

Run **inside the QEMU guest** (`/ #`):

```sh
uname -a
cat /proc/cpuinfo
hostname
mount
ps
ls -l /bin/sh /sbin/init /etc/init.d/rcS
```

### 7.1 Interpreting the outputs

| Command | What to look for | What a mismatch suggests |
| --- | --- | --- |
| `uname -a` | `5.15.0`, `armv7l` | Wrong kernel binary or target |
| `cat /proc/cpuinfo` | ARM CPU part `0xc09`, two processor entries | CPU/machine/SMP mismatch |
| `hostname` | `embedded-lab` | `rcS` not run, failed, or modified |
| `mount` | `/`, `/proc`, `/sys`, `/tmp` mounted | Init script, mount support, missing directories |
| `ps` | PID 1 `init` and shell present | Init not running properly |
| `ls -l` | `/bin/sh` and `/sbin/init` resolve to working commands | Missing BusyBox applet links |

Known good `hostname`:

```text
embedded-lab
```

Known good process evidence: `ps` showed **PID 1 = init** and an interactive `-/bin/sh`. The full process list varies between boots and should not be compared line for line.

### 7.2 Optional deeper checks (not part of the recorded test)

```sh
cat /proc/cmdline
cat /proc/mounts
cat /proc/meminfo | head
ls -l /dev/console /dev/null
```

These checks help narrow down failures; the README does **not** claim recorded PASS results for them.

## 8. Rebuild guide and checkpoints

> **Scope note:** The prebuilt output files in this repository have been boot-tested. The commands in this section are a **recommended reproducibility procedure** using the matching original sources and toolchain. Their end-to-end rebuild was **not separately verified** from the supplied boot log.

### 8.1 Source directories and precautions

The original development workspace used:

```text
~/embedded_lab1/
├── kernel/linux-5.15/
├── uboot/u-boot-2022.04/
├── rootfs/busybox-1.35.0/
├── rootfs/initramfs/
└── output/
```

Upstream source trees (`linux-5.15`, `u-boot-2022.04`, `busybox-1.35.0`) are **not** tracked in this Git repository. Do not assume cloning `LOS201` alone provides them.

Before experimenting:

```bash
cd ~/LOS201/embedded_lab1
mkdir -p ~/embedded_lab1/rebuild-backup
cp -a output/. ~/embedded_lab1/rebuild-backup/
```

This preserves boot-tested output artifacts outside the repository.

### 8.2 Linux kernel: check configuration and build

On the Ubuntu **host**:

```bash
cd ~/embedded_lab1/kernel/linux-5.15
cp ~/LOS201/embedded_lab1/configs/kernel.config .config

# Keep generated configuration synchronized with the chosen source release.
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig

# Use -j2 when host RAM is limited; increase only if resources allow.
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage dtbs
```

**Checkpoint 1 — architecture and build results:**

```bash
file arch/arm/boot/zImage
find arch/arm/boot/dts -name 'vexpress-v2p-ca9.dtb' -ls
```

On Linux 5.15, the expected kernel image is `arch/arm/boot/zImage`. Find the generated DTB by its exact name rather than assuming every kernel version has the same DTS subdirectory layout.

**Checkpoint 2 — important kernel configuration options:**

```bash
grep -E '^(CONFIG_ARCH_VEXPRESS|CONFIG_AEABI|CONFIG_SMP|CONFIG_BLK_DEV_INITRD|CONFIG_RD_GZIP|CONFIG_SERIAL_AMBA_PL011|CONFIG_SERIAL_AMBA_PL011_CONSOLE)=' .config
```

For this design, check VExpress/ARM support, initramfs/gzip support, and PL011 serial console support. A missing option is a **candidate cause**, not automatic proof of a given boot failure.

**Checkpoint 3 — do not overwrite working artifacts prematurely:**

```bash
mkdir -p ~/embedded_lab1/test-output
cp arch/arm/boot/zImage ~/embedded_lab1/test-output/
cp "$(find arch/arm/boot/dts -name 'vexpress-v2p-ca9.dtb' -print -quit)" \
   ~/embedded_lab1/test-output/
```

If the DTB search finds no file, stop and fix the build before copying.

### 8.3 BusyBox: check the binary and applet links

```bash
cd ~/embedded_lab1/rootfs/busybox-1.35.0
cp ~/LOS201/embedded_lab1/configs/busybox.config .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2
file busybox
```

`file busybox` should identify an **ARM** executable. The checked-in BusyBox configuration requests **static linking** (`CONFIG_STATIC=y`). Confirm the resulting executable with `file`; a changed build may differ from the saved configuration.

To install into a **new staging tree** rather than overwriting the known-good rootfs:

```bash
mkdir -p ~/embedded_lab1/rootfs-stage
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
  CONFIG_PREFIX="$HOME/embedded_lab1/rootfs-stage" install
ls -l ~/embedded_lab1/rootfs-stage/bin/busybox
```

After staging, add the lab-specific `/etc/inittab`, `/etc/init.d/rcS`, top-level `/init`, directories, and any required runtime libraries. Installing BusyBox alone is **not** a finished Lab 01 rootfs.

For a dynamically linked BusyBox, inspect its interpreter and library requirements:

```bash
readelf -l ~/embedded_lab1/rootfs-stage/bin/busybox | grep -A1 INTERP || true
arm-linux-gnueabihf-readelf -d ~/embedded_lab1/rootfs-stage/bin/busybox | grep NEEDED || true
```

If the toolchain's `readelf` executable differs, use the matching `readelf` installed on the host. Missing `ld-linux-armhf.so.3` or shared libraries can make a file appear “not found” even when the BusyBox binary exists.

### 8.4 Initramfs: how to inspect and rebuild safely

**Inspect the boot-tested image without altering it:**

```bash
cd ~/LOS201/embedded_lab1
gzip -t output/initramfs.cpio.gz
gzip -dc output/initramfs.cpio.gz | cpio -t | head -50
```

**Critical detail:** Git does not preserve character device nodes. In the original rootfs, the following special files were observed:

```text
/dev/console
/dev/null
/dev/zero
/dev/random
```

If rebuilding from a Git checkout, first create a **separate staging rootfs** (do not make privileged changes inside your `~/LOS201` checkout). For example:

```bash
mkdir -p ~/embedded_lab1/initramfs-stage
cp -a ~/LOS201/embedded_lab1/rootfs/initramfs/. ~/embedded_lab1/initramfs-stage/
ls -l ~/embedded_lab1/initramfs-stage/bin/busybox
ls -l ~/embedded_lab1/initramfs-stage/sbin/init
```

The following are **example device-node commands for the staging directory** and may require root privileges. Run them only after confirming the staging path:

```bash
cd ~/embedded_lab1/initramfs-stage
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null    c 1 3
sudo mknod -m 666 dev/zero    c 1 5
sudo mknod -m 666 dev/random  c 1 8
```

If nodes already exist, inspect them with `ls -l dev/` rather than rerunning `mknod`. Verify the required BusyBox binary, symlinks, runtime libraries (if dynamically linked), executable scripts, and device-node major/minor values before packaging.

One possible packaging procedure from the **prepared stage** is:

```bash
cd ~/embedded_lab1/initramfs-stage
sudo find . -print0 | sudo cpio --null -o --format=newc | gzip -9 > ~/embedded_lab1/test-output/initramfs.cpio.gz
gzip -t ~/embedded_lab1/test-output/initramfs.cpio.gz
```

This is **an example rebuild procedure**, not evidence that the original archive was created with exactly these commands. It requires `~/embedded_lab1/test-output/` to exist. Avoid uploading a new archive until it has been tested with QEMU.

### 8.5 U-Boot (separate from tested QEMU flow)

The repository retains `configs/uboot.config` and `output/u-boot`. They are useful build artifacts, but **the QEMU command in Section 6 bypasses U-Boot**. Do not conclude that U-Boot can independently boot this target merely because `output/u-boot` exists.

If U-Boot compilation fails in crypto/host-tools code (for example, `rsa-sign.c`), inspect **the exact compiler diagnostic**, U-Boot release, host OpenSSL/development headers, and `TOOLS_FIT_SIGNATURE` configuration. The checked-in `configs/uboot.config` currently has **`CONFIG_TOOLS_FIT_SIGNATURE=y`**. In a lab environment where image signing is not required, disabling FIT-signature tooling **may** be a workaround; **do not disable signature verification in a secure-boot product**. Rebuild and test the bootloader separately after any such change.

## 9. Troubleshooting by symptom

This section organizes failures into **symptom → first place to check → candidate fix**. A proposed fix is not proof of the root cause until the related check confirms it.

### 9.1 Host and QEMU startup

#### A. `qemu-system-arm: command not found`

**Where:** Ubuntu host, before QEMU starts.

**Check:**

```bash
command -v qemu-system-arm
qemu-system-arm --version
```

**Fix:** Install `qemu-system-arm` through your distribution package manager. Do not try to install it from inside BusyBox.

#### B. `unsupported machine type`, `invalid machine`, or a rejected CPU

**Where:** QEMU command line.

**Check:**

```bash
qemu-system-arm -machine help | grep -i vexpress
qemu-system-arm -cpu help | grep -i cortex-a9
```

**Fix:** Use a supported machine/CPU pair; the validated command is `-M vexpress-a9 -cpu cortex-a9`. Do not substitute an unrelated machine without also checking the device tree and kernel configuration.

#### C. `Could not open ...`, `No such file or directory` for zImage, DTB, or initramfs

**Where:** Current directory and `output/`.

**Check:**

```bash
pwd
ls -lh output/
file output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz
```

**Fix:** Run QEMU from `~/LOS201/embedded_lab1/` or use absolute paths. Check case-sensitive filenames (`zImage`, not `zimage`).

#### D. QEMU starts but the terminal stays blank

**Where:** Console configuration / early boot.

**Check:**

```bash
grep -E '^CONFIG_SERIAL_AMBA_PL011(_CONSOLE)?=' configs/kernel.config
```

Check for `-nographic`, `console=ttyAMA0,115200`, the matching `vexpress-v2p-ca9.dtb`, and whether any kernel lines appear at all.

**Fix:** First restore the verified QEMU command, especially its serial arguments. If still blank, investigate DTB/console driver support or broken kernel image. **Do not assume a blank terminal always means a kernel panic.**

#### E. Kernel reports DTB/FDT errors or hardware initialization hangs

**Where:** Device Tree or QEMU machine pairing.

**Check:**

```bash
file output/vexpress-v2p-ca9.dtb
fdtdump output/vexpress-v2p-ca9.dtb | head -30
```

**Fix:** Supply a DTB built for the same `vexpress-a9` platform and compatible kernel tree. Do not mix DTBs from arbitrary ARM boards. `fdtdump` is provided by device-tree tools on some installations; `dtc -I dtb -O dts ...` is an alternative.

### 9.2 Kernel and initramfs boot

#### F. `Kernel panic - not syncing: No working init found` or failure to execute `/sbin/init`

**Where:** Content of the **compressed initramfs**, executable permissions, and BusyBox links.

**Check:**

```bash
gzip -t output/initramfs.cpio.gz
gzip -dc output/initramfs.cpio.gz | cpio -t | grep -E '(sbin/init|bin/busybox|etc/inittab|etc/init.d/rcS)'
ls -l rootfs/initramfs/sbin/init rootfs/initramfs/bin/busybox
```

**Fix:** Make sure `/sbin/init` resolves to an executable compatible with ARM, the initramfs archive contains it, and any dynamic loader/shared libraries exist. Rebuild the archive **after** fixing the staging filesystem. Fixing only the Git working tree will not change the already-packed `output/initramfs.cpio.gz`.

#### G. `Exec format error` when launching BusyBox or `/init`

**Where:** Binary architecture or script interpreter.

**Check:**

```bash
file rootfs/initramfs/bin/busybox
head -1 rootfs/initramfs/init
ls -l rootfs/initramfs/bin/sh
```

**Fix:** Cross-compile BusyBox for ARM (`arm-linux-gnueabihf-`), not x86-64. Scripts need a valid shebang (for example `#!/bin/sh`) and an existing executable interpreter.

#### H. BusyBox exists but starting it says `not found` / `No such file or directory`

**Where:** ELF dynamic loader or shared libraries.

**Check:**

```bash
file rootfs/initramfs/bin/busybox
readelf -l rootfs/initramfs/bin/busybox | grep -A1 INTERP || true
readelf -d rootfs/initramfs/bin/busybox | grep NEEDED || true
find rootfs/initramfs/lib -maxdepth 3 -type f | head
```

**Fix:** Provide matching ARM hard-float runtime loader/libraries in the initramfs, or rebuild BusyBox statically if that is the chosen design. Confirm the target ABI; installing **x86** libraries will not fix an **ARM** executable.

#### I. Kernel says it cannot unpack initramfs, complains about compression, or cannot locate the expected root filesystem

**Where:** gzip/cpio archive and kernel initrd configuration.

**Check:**

```bash
gzip -t output/initramfs.cpio.gz
gzip -dc output/initramfs.cpio.gz | cpio -t | head
# On the original kernel source tree:
grep -E '^(CONFIG_BLK_DEV_INITRD|CONFIG_RD_GZIP)=' \
  ~/embedded_lab1/kernel/linux-5.15/.config
```

**Fix:** Recreate a valid gzip-compressed `newc` cpio archive, ensure kernel support for initramfs/gzip, and use the tested `-initrd` parameter. For an `unknown-block(...)` panic, also inspect `root=`, initramfs loading, and the exact preceding kernel messages before guessing.

#### J. `Warning: unable to open an initial console` or broken `/dev/*`

**Where:** Device nodes in the packaged initramfs.

**Check:**

```bash
ls -l rootfs/initramfs/dev/
gzip -dc output/initramfs.cpio.gz | cpio -tv | grep -E 'dev/(console|null|zero|random)'
```

**Fix:** Recreate the special nodes with the correct device type/major/minor inside a **staging** filesystem, repack the archive, and verify. `.gitkeep` files are not substitutes for device nodes. Do **not** run privileged `mknod` blindly inside the Git checkout.

#### K. A modified `rcS` or `/init` is ignored after reboot

**Where:** Difference between the **Git rootfs directory** and the already-packed archive.

**Check:**

```bash
stat rootfs/initramfs/etc/init.d/rcS output/initramfs.cpio.gz
cat rootfs/initramfs/etc/init.d/rcS
```

**Fix:** Rebuild the archive from the updated staging filesystem, then point `-initrd` at the new archive during testing. Also remember that `rdinit=/sbin/init` means the top-level `/init` is **not** run on this tested path.

### 9.3 User space after boot

#### L. The prompt appears but `/proc` or `/sys` is missing/empty

**Where:** BusyBox init, `inittab`, and `rcS`.

**Check in guest:**

```sh
mount
ls /proc
ls /sys
ps
```

**Check on host:**

```bash
cat rootfs/initramfs/etc/inittab
cat rootfs/initramfs/etc/init.d/rcS
ls -l rootfs/initramfs/etc/init.d/rcS
```

**Fix:** Confirm `::sysinit:/etc/init.d/rcS` exists, script has execute permission, mount commands are correct, and `/proc` and `/sys` directories exist. If mounting still fails, review kernel procfs/sysfs config and actual guest error text.

#### M. Wrong hostname or missing `Boot completed!` message

**Where:** `rcS` execution and the **archive containing it**.

**Check in guest:**

```sh
hostname
cat /etc/init.d/rcS
```

**Fix:** Verify `inittab` invokes `rcS`, script runs without an interpreter/permission error, and the **repacked** initramfs includes the updated script. If only the hostname is wrong, focus first on the `hostname embedded-lab` command.

#### N. `Permission denied` running startup scripts

**Where:** Executable mode and shebang.

**Check on host:**

```bash
ls -l rootfs/initramfs/init rootfs/initramfs/etc/init.d/rcS
head -1 rootfs/initramfs/etc/init.d/rcS
```

**Fix:** Ensure scripts are executable in the **staging rootfs** (for example `chmod +x` there), begin with a valid shebang, and repack. Do not chmod the source tree indiscriminately.

### 9.4 Compile and build problems

#### O. ARM cross-compiler missing or compiling for x86 by mistake

**Check:**

```bash
command -v arm-linux-gnueabihf-gcc
arm-linux-gnueabihf-gcc -dumpmachine
file ~/embedded_lab1/rootfs/busybox-1.35.0/busybox
```

**Fix:** Install/select the correct ARM GNU/Linux cross-toolchain. Always provide the intended `CROSS_COMPILE` prefix when building kernel/BusyBox, and confirm final binary architecture with `file`.

#### P. Assembly error: `selected processor does not support ...` (for example `isb`)

**Where:** Compiler default `-march`/mode versus ARMv7-A source/assembler expectations.

**Check:**

```bash
arm-linux-gnueabihf-gcc -v 2>&1 | tail -20
grep '^CONFIG_CPU_V7=' ~/embedded_lab1/kernel/linux-5.15/.config
```

**Candidate lab-specific workaround:** If logs confirm an ARMv7 assembler target mismatch, an explicit set of flags used in related ARMv7 lab work is:

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
  KCFLAGS='-march=armv7-a -Wa,-march=armv7-a' -j2 zImage dtbs
```

Only apply after verifying the exact failing command. A forced architecture flag cannot repair every toolchain/build error; check GCC/binutils compatibility and avoid mixing ARM/Thumb assumptions indiscriminately.

#### Q. U-Boot compilation fails in `rsa-sign.c` or FIT signature host tools

**Where:** U-Boot host tools vs OpenSSL headers/APIs/configuration, **not** the direct Linux QEMU boot path.

**Check:**

```bash
cd ~/embedded_lab1/uboot/u-boot-2022.04
grep -E '^(CONFIG_TOOLS_FIT_SIGNATURE|CONFIG_FIT_SIGNATURE)=' .config
```

**Fix:** Capture the first compiler error, verify required host packages and supported versions. For a **non-secure educational build**, removing optional FIT-signature tooling may work when this is the actual failing component. Avoid this workaround where signed images or secure boot are required. Successful U-Boot compilation still requires **separate** bootloader execution testing.

### 9.5 GitHub submission problems

#### R. Git refuses to commit: `Author identity unknown`

**Check:**

```bash
git config user.name
git config user.email
```

**Fix:** Configure name/email for this repository (use your own identity):

```bash
git config --local user.name 'YOUR_GITHUB_NAME'
git config --local user.email 'YOUR_GITHUB_EMAIL_OR_NOREPLY'
```

#### S. Push fails: `Invalid username or token`

**Cause:** GitHub HTTPS Git operations do not use the normal account password.

**Fix:** Authenticate with GitHub CLI or a suitable credential method:

```bash
gh auth login
gh auth setup-git
gh auth status
```

Never paste authentication tokens, recovery codes, or passwords into the README or Git commits.

#### T. Push fails: `fetch first` / `non-fast-forward`

**Cause:** Teammates or the owner have pushed new commits to `main`.

**Check:**

```bash
git status
git fetch origin
git log --oneline --left-right HEAD...origin/main
```

**Fix (when work is committed and working tree is clean):**

```bash
git pull --rebase origin main
# Resolve conflicts if Git stops; do not force-push.
git push origin main
```

Do **not** use `git push --force` on a shared coursework repository.

#### U. Push fails: `403` / `permission denied`

**Cause:** The account used to authenticate Git has no write access, the invitation is pending, or the target branch is protected.

**Check:**

```bash
gh auth status
git remote -v
```

**Fix:** Confirm that your logged-in GitHub username is an accepted collaborator with push permissions. If `main` is protected, follow the repository's branch/pull-request workflow; changing Git commands alone cannot bypass server-side permissions.

#### V. GitHub rejects a file larger than 100 MB

**Where:** Accidentally staged upstream kernel source archives or giant build objects.

**Check staged objects before commit:**

```bash
git diff --cached --stat
find ~/embedded_lab1 -type f -size +50M -printf '%s %p\n' | sort -nr | head
```

**Fix:** Commit the needed configs, scripts, reports, and final reasonable-sized artifacts. Exclude unnecessary `vmlinux`, `vmlinux.o`, source tarballs, and build trees. If a very large file is essential, evaluate Git LFS and repository policy first. Do not stage the original multi-gigabyte workspace automatically.

## 10. First checks when something fails

Instead of rebuilding everything, identify the **first failing stage**:

| Failing stage | First command/check | Next likely location |
| --- | --- | --- |
| Before QEMU launches | `command -v qemu-system-arm` | Host package or command line |
| QEMU cannot open image | `pwd; ls -lh output/` | Wrong path/missing artifact |
| No serial output | QEMU `-nographic`, kernel `console=...` | PL011 config/DTB/kernel image |
| Kernel panics before shell | `gzip -t`; `cpio -t`; inspect panic log | Initramfs contents, `/sbin/init`, ABI |
| Shell works, mounts fail | `mount`; `cat /etc/inittab`; `cat /etc/init.d/rcS` | BusyBox init/startup scripts |
| Wrong binary format | `file bin/busybox` | Cross-compilation target |
| Config change has no effect | Compare rootfs files with packed `initramfs.cpio.gz` | Repack/retest |
| Git commit/push fails | `git status`; `gh auth status` | Local identity, authentication, shared branch |

### 10.1 Minimal evidence collection (Ubuntu host)

From `~/LOS201/embedded_lab1`:

```bash
pwd
ls -lh output/
file output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz
gzip -t output/initramfs.cpio.gz
git status --short
```

When asking for help, share the **exact first error line**, approximately **20–30 lines above it**, the command you ran, and whether the failure happened on **Ubuntu host** or inside **QEMU guest**. Do not hide the first error behind only the last line of a build log.

### 10.2 Kernel/BusyBox logs (optional, for rebuilding)

To capture a rebuild log without hiding it:

```bash
cd ~/embedded_lab1/kernel/linux-5.15
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage dtbs 2>&1 | tee ~/embedded_lab1/kernel_build.log
```

**Important:** When using `tee`, the shell may report `tee`'s exit status instead of `make`'s. In Bash, run `set -o pipefail` before the pipeline if you need the failure code to propagate correctly. The log only records a new build if you run the command; it is not part of the already verified results.

## 11. Publishing updates to GitHub

From Ubuntu:

```bash
cd ~/LOS201
# Update local README.md, configs, diagrams or report as needed.
git status
git diff -- embedded_lab1/README.md
git add embedded_lab1/README.md
git commit -m 'Expand Lab 01 README with troubleshooting guide'
git pull --rebase origin main
git push origin main
```

Use `git pull --rebase` only after committing your work and checking there are no unresolved local changes; if Git reports a conflict, **stop and resolve it** rather than running arbitrary reset/force commands.

**Shared-repo safety:** Always inspect `git status` and do not modify unrelated teammates' folders. The above example stages **only the Lab 01 README**.

## 12. Reproducibility and limitations

- **Verified**: direct QEMU boot with existing `zImage`, DTB, initramfs; Linux 5.15.0; ARM CPU identification; init PID 1; mounted `/proc`, `/sys`, `/tmp`; interactive shell.
- **Provided but not boot-validated in this record**: U-Boot binary, any U-Boot boot sequence, and a full rebuild from the source releases.
- **Not included in Git**: upstream Linux, U-Boot and BusyBox source trees; heavy intermediate compilation objects; raw special device nodes.
- **Required to rebuild**: corresponding source versions, matching cross-compiler, saved configuration files, proper BusyBox applet links, initialization scripts, and correctly packaged device nodes.
- **Not tested here**: networking, flash partitioning, storage persistence, performance, security hardening, and real ARM board bring-up.
- Many troubleshooting commands are **diagnostic suggestions**, not evidence of a specific error occurring during this Lab 01 boot.

## 13. Conclusion

This Lab 01 demonstrates the integration of an ARM Linux kernel, a board-specific DTB, and a BusyBox-based initramfs on QEMU's Versatile Express Cortex-A9 platform. The saved test confirms a working serial shell and core userspace initialization. The troubleshooting checklist provides a repeatable method to distinguish host-tool problems, kernel boot failures, rootfs/init issues, and Git submission errors without replacing known-good artifacts unnecessarily.
