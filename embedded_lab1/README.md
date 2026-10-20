# Lab 01 — Build and Boot Embedded Linux on QEMU (ARM Cortex-A9)

> **Course:** Embedded Linux Systems (LOS201)<br>
> **Target:** ARMv7-A, ARM Versatile Express (`vexpress-a9`)<br>
> **Components:** Linux 5.15.0 · BusyBox 1.35.0 · initramfs · QEMU<br>
> **Audience:** Beginners following the lab for the first time<br>
> **Verified:** The supplied images successfully booted in QEMU on 20 October 2026.

This guide explains **how to complete Lab 01 from a fresh Ubuntu environment**, how to check each stage, and how to identify and fix common errors. You can use it either to **run the existing, verified images** or to **rebuild everything yourself**.

> **Scope and honesty:** The QEMU boot and runtime checks in [Section 9](#9-verified-results) were observed in the student's terminal. The complete fresh-source rebuild procedure is a carefully documented **reproduction guide**, not a claim that every command was independently run with the current Ubuntu packages. Compiler versions, package availability and host dependencies can affect a rebuild.

## Contents

- [1. What you will build](#1-what-you-will-build)
- [2. Read this first: the two routes](#2-read-this-first-the-two-routes)
- [3. Tools, versions and folder structure](#3-tools-versions-and-folder-structure)
- [4. Route A — Boot the prebuilt system (quick start)](#4-route-a--boot-the-prebuilt-system-quick-start)
- [5. Route B — Build the lab from scratch](#5-route-b--build-the-lab-from-scratch)
- [6. Understand the boot sequence](#6-understand-the-boot-sequence)
- [7. Inspect and validate each artifact](#7-inspect-and-validate-each-artifact)
- [8. Run your newly built system](#8-run-your-newly-built-system)
- [9. Verified results](#9-verified-results)
- [10. Troubleshooting: start with the symptom](#10-troubleshooting-start-with-the-symptom)
- [11. Debugging checklist](#11-debugging-checklist)
- [12. Optional: U-Boot and GitHub submission](#12-optional-u-boot-and-github-submission)
- [13. Completion checklist and limitations](#13-completion-checklist-and-limitations)
- [14. Reference links](#14-reference-links)

---

## 1. What you will build

An embedded Linux system combines a kernel, hardware description, and userspace:

```text
                     UBUNTU HOST (x86_64)
           ┌────────────────────────────────┐
           │ Linux 5.15 source + .config    │──► zImage
           │ Device Tree sources           │──► vexpress-v2p-ca9.dtb
           │ BusyBox 1.35 + startup files  │──► rootfs/
           │ rootfs/ + device nodes        │──► initramfs.cpio.gz
           └────────────────────────────────┘
                          │
               qemu-system-arm (-kernel,
                   -dtb, -initrd)
                          ▼
                 ARM Cortex-A9 guest
                          │
                   Linux kernel 5.15
                          │
               /sbin/init (BusyBox, PID 1)
                          │
          /etc/inittab → /etc/init.d/rcS
                          │
                 Interactive shell: / #
```

**Lab goal:** reach `/ #` in the QEMU console and verify the kernel, ARM CPU, root filesystem, mounted pseudo-filesystems and init process.

**Important:** The tested command uses **QEMU direct kernel boot**, so **U-Boot does not run** in that boot path. The `u-boot` binary is retained as a separate build artifact, not as proof of a successful U-Boot boot.

## 2. Read this first: the two routes

| Route | Choose this when... | Starts at | What it proves |
| --- | --- | --- | --- |
| **A — Quick start** | You want to run/check Lab 01 immediately | [Section 4](#4-route-a--boot-the-prebuilt-system-quick-start) | The committed images boot in QEMU |
| **B — From scratch** | You want to learn how to build each component | [Section 5](#5-route-b--build-the-lab-from-scratch) | You can attempt to reproduce the build and diagnose each stage |

**Rule for both routes:** The commands run on the **Ubuntu host** (`user@ubuntu:~$`) unless a step explicitly says **inside QEMU** (`/ #`). Avoid running host build, `apt`, or Git commands inside the BusyBox guest.

**Do not overwrite known-good outputs.** Route B uses a separate `$LAB_WORK` directory; it does not replace the files in this repository.

## 3. Tools, versions and folder structure

### 3.1 Software versions

| Item | Version/choice | Why it matters |
| --- | --- | --- |
| Host OS | Ubuntu (package commands below use `apt`) | Cross-compilation host |
| CPU target | ARMv7-A / Cortex-A9 | Matches `vexpress-a9` |
| Kernel | **Linux 5.15.0** (`linux-5.15.tar.xz`) | Matches the verified kernel version |
| BusyBox | **1.35.0** | Minimal shell and utilities |
| Cross-compiler | `arm-linux-gnueabihf-` | Produces ARM Linux binaries |
| QEMU | `qemu-system-arm` | Emulates target board |
| DTB | `vexpress-v2p-ca9.dtb` | Matches emulated hardware |
| Root filesystem | `newc` cpio archive + gzip | Initial userspace in memory |
| Boot console | `ttyAMA0,115200` | PL011 serial output |

The original saved kernel configuration includes `CONFIG_ARCH_VEXPRESS=y`, `CONFIG_CPU_V7=y`, `CONFIG_AEABI=y`, `CONFIG_SMP=y`, `CONFIG_BLK_DEV_INITRD=y`, `CONFIG_RD_GZIP=y` and PL011 console support. The saved BusyBox configuration includes `CONFIG_STATIC=y`, `CONFIG_INIT=y`, `CONFIG_ASH=y` and `CONFIG_CROSS_COMPILER_PREFIX="arm-linux-gnueabihf-"`.

### 3.2 GitHub repository structure

```text
embedded_lab1/
├── README.md
├── configs/
│   ├── kernel.config          # Saved Linux kernel settings
│   ├── busybox.config         # Saved BusyBox settings
│   └── uboot.config           # Saved U-Boot settings
├── diagrams/
│   └── Embedded_Linux_Boot.pdf
├── output/
│   ├── zImage                 # Verified Linux image
│   ├── vexpress-v2p-ca9.dtb   # Verified Device Tree
│   ├── initramfs.cpio.gz      # Verified compressed rootfs
│   └── u-boot                 # Separate artifact, not used by direct boot
├── report/
│   └── Lab01_Report.pdf
└── rootfs/
    └── initramfs/
        ├── init
        ├── bin/               # busybox and symlinks
        ├── sbin/              # init applet symlink
        ├── etc/               # inittab, init.d/rcS, hostname
        ├── usr/
        ├── dev/               # Git placeholders, NOT device nodes
        ├── proc/
        ├── sys/
        └── tmp/
```

Supplementary files: [Boot diagram](diagrams/Embedded_Linux_Boot.pdf) · [Lab report](report/Lab01_Report.pdf).

### 3.3 Install tools on Ubuntu

```bash
sudo apt update
sudo apt install -y \
  build-essential git wget ca-certificates \
  bc bison flex libssl-dev libelf-dev libncurses-dev \
  gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
  qemu-system-arm cpio gzip bzip2 xz-utils \
  device-tree-compiler file
```

**Check before moving on:**

```bash
command -v qemu-system-arm
command -v arm-linux-gnueabihf-gcc
command -v arm-linux-gnueabihf-readelf
command -v cpio
qemu-system-arm --version
arm-linux-gnueabihf-gcc --version
```

Expected: commands resolve to executable paths, and version commands print versions. If `command -v` prints nothing, the corresponding tool is not available in your current `PATH`. Check the `apt` error before continuing.

> **Toolchain note:** A packaged cross-compiler may differ from the toolchain used for the verified images. A mismatch may cause build errors, especially with older U-Boot or a custom compiler defaulting to Thumb mode. Record the version you actually use.

### 3.4 Clone the Lab 01 repository

```bash
cd "$HOME"
git clone https://github.com/Longlmao123/LOS201.git
cd "$HOME/LOS201/embedded_lab1"
ls -l configs output rootfs
```

If you already cloned `~/LOS201`, **do not clone again**. Simply enter `cd ~/LOS201/embedded_lab1`.

---

## 4. Route A — Boot the prebuilt system (quick start)

### Step A1 — Verify required files on the Ubuntu host

```bash
cd ~/LOS201/embedded_lab1
ls -lh output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz
file output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz
gzip -t output/initramfs.cpio.gz
echo "gzip status: $?"
```

**Checkpoint:** All three files exist, and gzip status is `0`. A successful `gzip -t` normally has no output. If a file is missing, check `pwd` and the `output/` directory first.

### Step A2 — Run QEMU

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

**Checkpoint:** Boot logs appear, followed by the message below and a BusyBox shell:

```text
Boot completed! Welcome to Embedded Linux Lab 1
/ #
```

If you see a blank terminal, start with [Troubleshooting: no serial output](#10-troubleshooting-start-with-the-symptom). **Exit QEMU:** press **Ctrl+A**, release, then **X** (serial console escape sequence).

### Step A3 — Verify from **inside QEMU** (`/ #`)

```sh
uname -a
cat /proc/cpuinfo
hostname
mount
ps
```

See [Section 9](#9-verified-results) for actual results from this lab. If this route works, you already have a functioning Lab 01 system and can proceed to Route B to learn the build process.

---

## 5. Route B — Build the lab from scratch

### Before starting: use a **new work directory**

Open a terminal on Ubuntu (not in QEMU), then set two paths:

```bash
export LAB_REPO="$HOME/LOS201/embedded_lab1"
export LAB_WORK="$HOME/embedded_lab1_from_scratch"
mkdir -p "$LAB_WORK"/{downloads,kernel,rootfs,output}
printf 'Repository: %s\nWorkspace:  %s\n' "$LAB_REPO" "$LAB_WORK"
test -f "$LAB_REPO/configs/kernel.config" && echo 'Kernel config: found'
```

These variables belong to the **current terminal session**. If you close it, run the two `export` commands again. All work below goes to `$LAB_WORK`, so the checked-in `output/` files remain unchanged.

> **Storage:** Extracting and building Linux source requires multiple gigabytes. Check with `df -h "$HOME"` first. Do not copy kernel build directories into Git or accidentally commit `vmlinux.o` (often over 100 MB).

### Step B1 — Download source archives

**Linux 5.15.0** and **BusyBox 1.35.0** are downloaded from their upstream sites:

```bash
cd "$LAB_WORK/downloads"

wget -c https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.tar.xz
wget -c https://busybox.net/downloads/busybox-1.35.0.tar.bz2

ls -lh linux-5.15.tar.xz busybox-1.35.0.tar.bz2
xz -t linux-5.15.tar.xz
bzip2 -t busybox-1.35.0.tar.bz2
```

**Checkpoint:** Both archives exist and both integrity checks exit successfully. `-t` validates archive compression structure; for supply-chain authenticity, additionally compare upstream checksums/signatures when provided.

**Common mistakes:**

- `404 Not Found` → check version and official download URL; do not silently replace 5.15.0 with the latest kernel.
- `Temporary failure in name resolution` → check host networking and DNS using `getent hosts cdn.kernel.org`.
- `No space left on device` → run `df -h` and free space outside the lab before retrying.

### Step B2 — Extract sources

```bash
mkdir -p "$LAB_WORK/kernel" "$LAB_WORK/rootfs"
tar -xJf "$LAB_WORK/downloads/linux-5.15.tar.xz" -C "$LAB_WORK/kernel"
tar -xjf "$LAB_WORK/downloads/busybox-1.35.0.tar.bz2" -C "$LAB_WORK/rootfs"

ls -ld "$LAB_WORK/kernel/linux-5.15"
ls -ld "$LAB_WORK/rootfs/busybox-1.35.0"
```

**Checkpoint:** Each directory exists and contains a `Makefile`. Avoid running kernel `make` in the archive/download folder by mistake.

### Step B3 — Configure the Linux kernel

Use **the saved configuration from this Lab 01**, rather than a generic desktop ARM configuration:

```bash
cd "$LAB_WORK/kernel/linux-5.15"
cp "$LAB_REPO/configs/kernel.config" .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
```

**Check key options:**

```bash
grep -E '^(CONFIG_ARCH_VEXPRESS|CONFIG_CPU_V7|CONFIG_AEABI|CONFIG_SMP|CONFIG_BLK_DEV_INITRD|CONFIG_RD_GZIP|CONFIG_SERIAL_AMBA_PL011|CONFIG_SERIAL_AMBA_PL011_CONSOLE)=' .config
```

For this lab, verify that they are enabled (`=y`). Their roles:

| Option | Role | Typical symptom if missing |
| --- | --- | --- |
| `CONFIG_ARCH_VEXPRESS` | VExpress platform | Kernel does not support selected board |
| `CONFIG_CPU_V7`, `CONFIG_AEABI` | ARMv7/AABI support | Build or boot incompatibility |
| `CONFIG_SMP` | Multiple CPUs | Only one CPU online |
| `CONFIG_BLK_DEV_INITRD`, `CONFIG_RD_GZIP` | Initramfs/gzip support | Unable to use the supplied rootfs archive |
| `CONFIG_SERIAL_AMBA_PL011`, `..._CONSOLE` | PL011 serial console | No expected console output |

> **If `olddefconfig` changes an option:** Compare `cp .config "$LAB_WORK/kernel-after.config"` with the saved config using `diff -u "$LAB_REPO/configs/kernel.config" "$LAB_WORK/kernel-after.config" | head -80`. Differences can result from version/toolchain differences. Resolve them before assuming that your rebuild matches the original.

### Step B4 — Build `zImage` and the DTB

```bash
cd "$LAB_WORK/kernel/linux-5.15"
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage dtbs
```

`-j2` keeps peak resource usage moderate. Use more jobs only if sufficient RAM is available.

**Check both artifacts:**

```bash
ls -lh arch/arm/boot/zImage
find arch/arm/boot/dts -name 'vexpress-v2p-ca9.dtb' -print
file arch/arm/boot/zImage
```

Then stage the build result **without touching the repository's verified files**:

```bash
DTB_PATH="$(find "$LAB_WORK/kernel/linux-5.15/arch/arm/boot/dts" \
  -name 'vexpress-v2p-ca9.dtb' -type f -print -quit)"

if [ -z "$DTB_PATH" ] || [ ! -f "$DTB_PATH" ]; then
  echo 'ERROR: vexpress-v2p-ca9.dtb was not built; fix the kernel build first.'
else
  cp "$LAB_WORK/kernel/linux-5.15/arch/arm/boot/zImage" "$LAB_WORK/output/"
  cp "$DTB_PATH" "$LAB_WORK/output/"
  ls -lh "$LAB_WORK/output/"
fi
```

**Common mistakes:** Wrong cross-compiler prefix, missing `bison`/`flex`/OpenSSL headers, `Permission denied` on extracted source, or using an unrelated DTB. Read the **first actual compiler error**, not only the last `make: *** Error 2` line. If needed, retry a single-job build and save a log:

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j1 zImage dtbs \
  2>&1 | tee "$LAB_WORK/kernel-build.log"
```

> If an ARM assembler says `selected processor does not support 'isb' in ARM mode`, first inspect `arm-linux-gnueabihf-gcc -v` and the effective target flags; some custom compilers have unexpected defaults. For this specific diagnosis, explicitly selecting ARMv7-A/ARM mode (for example `KCFLAGS='-march=armv7-a -marm -Wa,-march=armv7-a'`) may help, but **only** after verifying that the flags are compatible with the toolchain and the kernel build. Do not apply flags blindly to unrelated build errors.

### Step B5 — Configure and build BusyBox

```bash
cd "$LAB_WORK/rootfs/busybox-1.35.0"
cp "$LAB_REPO/configs/busybox.config" .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2
```

**Check the resulting binary:**

```bash
file busybox
arm-linux-gnueabihf-readelf -h busybox | grep -E 'Class:|Machine:'
grep -E '^(CONFIG_STATIC|CONFIG_INIT|CONFIG_ASH|CONFIG_CROSS_COMPILER_PREFIX)=' .config
```

Expected: **ARM**, not x86-64. The saved configuration requests a static BusyBox (`CONFIG_STATIC=y`). If the actual executable turns out dynamic, use `arm-linux-gnueabihf-readelf -l busybox | grep -A1 INTERP` to inspect the interpreter and make sure the target rootfs has matching ARM shared libraries.

**Install BusyBox into the fresh rootfs staging directory:**

```bash
export STAGE="$LAB_WORK/initramfs-stage"
mkdir -p "$STAGE"
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- CONFIG_PREFIX="$STAGE" install
ls -l "$STAGE/bin/busybox" "$STAGE/bin/sh" "$STAGE/sbin/init"
```

**Checkpoint:** `/bin/busybox` exists, `/bin/sh` and `/sbin/init` are present (usually BusyBox symlinks). If `sbin/init` is missing, check whether `CONFIG_INIT=y` actually survived `olddefconfig` and whether `make install` completed.

### Step B6 — Create the complete minimal rootfs

**Create directories** in the staging rootfs:

```bash
export STAGE="$LAB_WORK/initramfs-stage"
mkdir -p "$STAGE"/{dev,proc,sys,tmp,etc/init.d,bin,sbin,lib,usr/bin,usr/sbin}
```

**Create top-level `/init`:**

```bash
cat > "$STAGE/init" <<'INIT_EOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
exec /sbin/init
INIT_EOF
chmod 755 "$STAGE/init"
```

**Create `/etc/inittab`:**

```bash
cat > "$STAGE/etc/inittab" <<'INITTAB_EOF'
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/swapoff -a
::shutdown:/bin/umount -a -r
::restart:/sbin/init
INITTAB_EOF
```

**Create `/etc/init.d/rcS`:**

```bash
cat > "$STAGE/etc/init.d/rcS" <<'RCS_EOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
echo 'Boot completed! Welcome to Embedded Linux Lab 1'
hostname embedded-lab
RCS_EOF
chmod 755 "$STAGE/etc/init.d/rcS"
printf 'embedded-lab\n' > "$STAGE/etc/hostname"
```

These startup file contents match the files observed in this Lab 01's repository. With `rdinit=/sbin/init`, BusyBox `/sbin/init` starts directly; the top-level `/init` file is included, but **not selected** by that parameter.

**Check scripts and applets:**

```bash
ls -l "$STAGE/init" "$STAGE/etc/init.d/rcS" \
      "$STAGE/bin/busybox" "$STAGE/bin/sh" "$STAGE/sbin/init"
head -n 1 "$STAGE/init" "$STAGE/etc/init.d/rcS"
```

**Common mistakes:** Forgot `chmod +x`, used Windows CRLF line endings (the kernel sees `#!/bin/sh\r`), deleted BusyBox symlinks, or edited `rcS` **after** packaging the archive. To detect CRLF: `file "$STAGE/etc/init.d/rcS"`; convert line endings if necessary.

### Step B7 — Create special device nodes safely

Linux needs device nodes such as `/dev/console`. These are **not regular files** and cannot be represented by Git.

**Only operate inside your separate staging directory.** First inspect:

```bash
export STAGE="$LAB_WORK/initramfs-stage"
printf 'Staging directory: %s\n' "$STAGE"
ls -ld "$STAGE" "$STAGE/dev"
ls -l "$STAGE/dev"
```

For a fresh staging rootfs, create these **character devices**:

```bash
sudo mknod -m 600 "$STAGE/dev/console" c 5 1
sudo mknod -m 666 "$STAGE/dev/null"    c 1 3
sudo mknod -m 666 "$STAGE/dev/zero"    c 1 5
sudo mknod -m 666 "$STAGE/dev/random"  c 1 8
```

**Verify type and major/minor:**

```bash
ls -l "$STAGE/dev/"
```

The leading `c` means **character device**, and `5, 1` for `console` means major 5, minor 1. If `mknod` says `File exists`, **do not delete the node automatically**; inspect it first. `.gitkeep` is only a repository placeholder, not a substitute for a device node.

### Step B8 — Package `initramfs.cpio.gz`

**Check prerequisites:**

```bash
test -x "$STAGE/bin/busybox" && echo 'BusyBox: OK'
test -x "$STAGE/etc/init.d/rcS" && echo 'rcS: OK'
test -e "$STAGE/sbin/init" && echo 'init: present'
ls -l "$STAGE/dev/console"
```

**Build the archive with normalized root ownership in its metadata:**

```bash
mkdir -p "$LAB_WORK/output"
set -o pipefail
sudo bash -c 'cd "$1" && find . -print0 | cpio --null -o -H newc --owner=0:0' \
  _ "$STAGE" | gzip -9 > "$LAB_WORK/output/initramfs.cpio.gz"
```

**Important:** `sudo` is used only to read special nodes and create correct archive metadata from the staging directory; the compressed output is written to `$LAB_WORK/output/` by your normal user. Ensure the `$STAGE` path is correct before running any privileged command.

**Check the package before booting:**

```bash
gzip -t "$LAB_WORK/output/initramfs.cpio.gz"
gzip -dc "$LAB_WORK/output/initramfs.cpio.gz" | cpio -t | head -35
gzip -dc "$LAB_WORK/output/initramfs.cpio.gz" | cpio -tv | \
  grep -E '(dev/console|dev/null|sbin/init|etc/inittab|etc/init.d/rcS)'
```

**Checkpoint:** gzip validates; the archive contains `./sbin/init`, `./etc/inittab`, `./etc/init.d/rcS`, and device nodes. The exact output from `cpio -tv` is permission/type/size sensitive, so check existence and type rather than expecting an identical text listing.

**If something is missing:** Fix it in `$STAGE`, **repack**, and rerun the checks. Editing `rootfs/initramfs/` or `$STAGE` does not modify an already-created `initramfs.cpio.gz`.

### Step B9 — Confirm build products

```bash
ls -lh "$LAB_WORK/output/zImage" \
       "$LAB_WORK/output/vexpress-v2p-ca9.dtb" \
       "$LAB_WORK/output/initramfs.cpio.gz"
gzip -t "$LAB_WORK/output/initramfs.cpio.gz"
```

At this point Route B should have produced **three** files needed by QEMU. Do not copy them into `~/LOS201/embedded_lab1/output/` until they pass the boot test.

---

## 6. Understand the boot sequence

This is the **actual direct-boot path** used by the lab:

| Stage | What happens | Where to debug if it fails |
| --- | --- | --- |
| 1. QEMU | Emulates Cortex-A9, RAM and serial console | QEMU command and supported machine types |
| 2. Kernel load | Reads `zImage` and DTB | File path, kernel format, DTB compatibility |
| 3. Initramfs unpack | Expands gzip/cpio archive | `gzip -t`, `cpio -t`, kernel initramfs settings |
| 4. PID 1 | Executes `/sbin/init` from initramfs | BusyBox binary, symlink, permissions, runtime loader |
| 5. `inittab` | Calls `/etc/init.d/rcS` | Inittab syntax and rcS path |
| 6. `rcS` | Mounts `/proc`, `/sys`, `/tmp`; sets hostname | Mount support, scripts, paths, permissions |
| 7. Shell | Starts `/bin/sh`, shows `/ #` | BusyBox `ash`, symlink, console |

### `rdinit=/sbin/init` versus `/init`

The QEMU command includes:

```text
console=ttyAMA0,115200 rdinit=/sbin/init mem=512M
```

The `rdinit` argument instructs the kernel to execute `/sbin/init` from the initial root filesystem. This bypasses the separate top-level `/init` script. When a script seems to be ignored, **check `/proc/cmdline` in the guest first**; it may simply not be the selected entry point.

## 7. Inspect and validate each artifact

Commands in this section run on **Ubuntu**:

| What to inspect | Command | What it tells you |
| --- | --- | --- |
| Kernel file | `file output/zImage` | Image type; may not report full target ABI |
| Kernel configuration | `grep '^CONFIG_ARCH_VEXPRESS=' configs/kernel.config` | Board option |
| DTB structure | `dtc -I dtb -O dts output/vexpress-v2p-ca9.dtb \| head` | DTB can be parsed |
| BusyBox target | `file rootfs/initramfs/bin/busybox` | ARM vs x86-64, static vs dynamic clues |
| Init link | `ls -l rootfs/initramfs/sbin/init` | Init exists/resolves |
| Shell link | `ls -l rootfs/initramfs/bin/sh` | Shell exists/resolves |
| Initramfs integrity | `gzip -t output/initramfs.cpio.gz` | gzip is not corrupt |
| Archive content | `gzip -dc output/initramfs.cpio.gz \| cpio -t \| head` | Embedded filesystem members |

`file` alone does not prove an executable runs; always combine binary checks with a QEMU boot attempt.

---

## 8. Run your newly built system

The following uses the Route B images (not the repository's prebuilt images):

```bash
qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel "$LAB_WORK/output/zImage" \
  -dtb "$LAB_WORK/output/vexpress-v2p-ca9.dtb" \
  -initrd "$LAB_WORK/output/initramfs.cpio.gz" \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

**Expected result:** Boot messages, `Boot completed! Welcome to Embedded Linux Lab 1`, then `/ #`. If it fails, use the [symptom table](#10-troubleshooting-start-with-the-symptom) and **compare with Route A**:

- **Route A works, Route B fails:** The host/QEMU command is probably fine. Focus on the new kernel, DTB or initramfs build.
- **Both routes fail:** Check installed QEMU, machine type, command line, terminal/serial setup and host environment first.

After success, use the commands in [Section 9](#9-verified-results) inside the guest to verify. Do **not** claim the fresh build passed until you have actually observed it.

---

## 9. Verified results

These are results the student **actually provided** from the successful QEMU run (20 October 2026). They correspond to **Route A's existing images**, not a separately verified Route B rebuild.

| Verification | Observed evidence | Status |
| --- | --- | --- |
| Interactive shell | `/ #` prompt | **PASS** |
| Kernel | `Linux embedded-lab 5.15.0 ... armv7l GNU/Linux` | **PASS** |
| CPU | `ARMv7 Processor rev 0`, part `0xc09` | **PASS** |
| Reported CPU entries | `processor: 0`, `processor: 1` | **PASS** |
| Emulated board | `Hardware: ARM-Versatile Express` | **PASS** |
| Hostname | `embedded-lab` | **PASS** |
| RootFS | `rootfs on / type rootfs` | **PASS** |
| procfs | `none on /proc type proc` | **PASS** |
| sysfs | `none on /sys type sysfs` | **PASS** |
| tmpfs | `none on /tmp type tmpfs` | **PASS** |
| Init | `PID 1 root ... init` | **PASS** |

**Observed `uname -a`:**

```text
Linux embedded-lab 5.15.0 #1 SMP Mon Oct 5 16:09:28 +07 2026 armv7l GNU/Linux
```

**Observed `mount`:**

```text
rootfs on / type rootfs (rw,size=244912k,nr_inodes=61228)
none on /proc type proc (rw,relatime)
none on /sys type sysfs (rw,relatime)
none on /tmp type tmpfs (rw,relatime)
```

**Recheck yourself inside QEMU:**

```sh
uname -a
cat /proc/cpuinfo
hostname
mount
ps
cat /proc/cmdline
```

> The recorded `ps` output includes some kernel worker names associated with CPUs beyond `processor: 0` and `processor: 1`. Those individual worker names are **not** used as evidence for CPU count. When validating a new run, compare `cat /proc/cpuinfo` and `cat /sys/devices/system/cpu/online` (if present) with your QEMU `-smp` argument.

---

## 10. Troubleshooting: start with the symptom

**How to use this section:** Identify the *first* failing stage. Check the command/setting listed below before changing configurations. Fix **one issue at a time**, rerun the same check and keep a record of the first error.

### 10.1 Fast error lookup

| Symptom | Check first | Likely next action |
| --- | --- | --- |
| `qemu-system-arm: command not found` | `command -v qemu-system-arm` | Install QEMU on **host** |
| Cannot open `zImage` / DTB / archive | `pwd`, `ls -lh output/` | Run from correct folder |
| Unsupported `vexpress-a9` | `qemu-system-arm -machine help` | Use supported machine name |
| Nothing on serial console | `-nographic`, `console=ttyAMA0`, PL011 config | Restore verified command |
| Kernel panic: no working init | Archive `/sbin/init`, BusyBox architecture | Fix rootfs and repackage |
| `Exec format error` | `file bin/busybox` | Build ARM binary |
| `not found` although BusyBox exists | ELF interpreter/shared libraries | Use static or add ARM libraries |
| Cannot unpack initramfs | `gzip -t`, `cpio -t`, kernel config | Repair/repackage archive |
| `/dev/console` warning | `cpio -tv`, `dev/console` | Recreate proper device node |
| Missing `/proc` or `/sys` | `inittab`, `rcS`, `mount` | Fix startup, repackage |
| Missing `/ #` shell | `/bin/sh`, BusyBox `ash`, `inittab` | Restore symlink/applets |
| Changes to `rcS` do not appear | `stat` archive, `rdinit=` | Repackage new archive |
| Kernel build aborts | First `error:` in build log | Fix toolchain/dependency/config |
| U-Boot `rsa-sign.c` failure | OpenSSL, `TOOLS_FIT_SIGNATURE` | Diagnose separate U-Boot build |
| Git `Author identity unknown` | `git config user.name/user.email` | Set Git commit identity |
| Git `authentication failed` | `gh auth status` | Authenticate via `gh`, not password |
| Git `fetch first` | `git fetch`, `git status` | `git pull --rebase` (resolve conflicts) |
| Git `403` | Push rights to repository | Ask owner for **Write** access |

### 10.2 Detailed fixes: open only the problem you have

<details>
<summary><strong>1. QEMU missing, unknown board or missing image file</strong></summary>

**Where it fails:** On the Ubuntu host, before the Linux kernel starts.

```bash
command -v qemu-system-arm
qemu-system-arm -machine help | grep -i vexpress
pwd
ls -lh output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz
```

**Fix:** Install `qemu-system-arm` if missing. Use the supported board name `vexpress-a9`, check capitalization (`zImage`), and execute the command from the directory containing `output/` or use absolute paths. A missing image is a *host file path* problem, not an initramfs/kernel problem.
</details>

<details>
<summary><strong>2. No boot log / black terminal after launching QEMU</strong></summary>

**Where it fails:** Serial output or early kernel initialization.

```bash
# Run on Ubuntu, inside the repository:
grep -E '^CONFIG_SERIAL_AMBA_PL011(_CONSOLE)?=' configs/kernel.config
file output/zImage output/vexpress-v2p-ca9.dtb
```

**Fix:** Restore `-nographic` and `console=ttyAMA0,115200`; verify PL011 serial console settings and the VExpress DTB. If the verified image boots but a rebuilt image does not, investigate the rebuilt kernel configuration. **No output alone does not prove a kernel panic.**
</details>

<details>
<summary><strong>3. Device Tree / FDT mismatch</strong></summary>

**Where it fails:** Board initialization, unexpected missing peripherals, FDT-related logs.

```bash
file output/vexpress-v2p-ca9.dtb
dtc -I dtb -O dts output/vexpress-v2p-ca9.dtb > /tmp/vexpress-check.dts
head -25 /tmp/vexpress-check.dts
```

**Fix:** Use `vexpress-v2p-ca9.dtb` generated from the matching ARM Linux source for `vexpress-a9`. A DTB from Raspberry Pi or another ARM board is not interchangeable.
</details>

<details>
<summary><strong>4. Kernel panic: `No working init found` / cannot execute PID 1</strong></summary>

**Where it fails:** After kernel/initramfs initialization, before userspace starts.

```bash
gzip -t output/initramfs.cpio.gz
gzip -dc output/initramfs.cpio.gz | cpio -t | \
  grep -E '(sbin/init|bin/busybox|etc/inittab|etc/init.d/rcS)'
ls -l rootfs/initramfs/sbin/init rootfs/initramfs/bin/busybox
file rootfs/initramfs/bin/busybox
```

**Fix:** Ensure the **archive** (not merely the Git directory) contains a working `/sbin/init`, an executable ARM BusyBox, valid symlinks and any required ARM runtime libraries. Rebuild the cpio archive after changing rootfs files, then try QEMU again.
</details>

<details>
<summary><strong>5. BusyBox `Exec format error` or `not found`</strong></summary>

**Where it fails:** Userspace binary launch.

```bash
file rootfs/initramfs/bin/busybox
arm-linux-gnueabihf-readelf -h rootfs/initramfs/bin/busybox | grep Machine
arm-linux-gnueabihf-readelf -l rootfs/initramfs/bin/busybox | grep -A1 INTERP || true
```

**Fix:** `Exec format error` normally points to the wrong architecture or script format: rebuild BusyBox for **ARM**, not x86-64. A dynamic ARM executable can say `not found` when its interpreter or shared libraries are missing: supply matching **ARM** libraries or use the static BusyBox configuration. Check `#!/bin/sh` and Unix line endings in scripts.
</details>

<details>
<summary><strong>6. `initramfs` gzip/cpio error or root filesystem panic</strong></summary>

**Where it fails:** Kernel unpacking the initial archive.

```bash
gzip -t output/initramfs.cpio.gz
gzip -dc output/initramfs.cpio.gz | cpio -t | head -40
grep -E '^CONFIG_(BLK_DEV_INITRD|RD_GZIP)=' configs/kernel.config
```

**Fix:** Package a valid `newc` cpio archive and compress it with gzip. Verify kernel initramfs/gzip support and `-initrd`. For errors mentioning `unknown-block(...)`, check the **full preceding kernel log** and the actual root/initrd options before guessing.
</details>

<details>
<summary><strong>7. `/dev/console` warning or missing device nodes</strong></summary>

**Where it fails:** Console setup and early userspace I/O.

```bash
gzip -dc output/initramfs.cpio.gz | cpio -tv | \
  grep -E 'dev/(console|null|zero|random)'
ls -l rootfs/initramfs/dev/
```

**Fix:** Git cannot store device nodes. Recreate `dev/console` (character 5:1), `dev/null` (1:3), etc. in the **staging directory**, check with `ls -l`, and repackage. `.gitkeep` is **not** a valid `/dev/console`.
</details>

<details>
<summary><strong>8. `/proc`, `/sys`, `/tmp` missing or hostname incorrect</strong></summary>

**Where it fails:** `/etc/inittab` → `/etc/init.d/rcS`.

```bash
cat rootfs/initramfs/etc/inittab
cat rootfs/initramfs/etc/init.d/rcS
ls -l rootfs/initramfs/etc/init.d/rcS
```

Inside QEMU:

```sh
mount
hostname
cat /proc/cmdline
```

**Fix:** Confirm the `::sysinit:/etc/init.d/rcS` entry, executable `rcS`, mount points and required filesystem support. Remember to **rebuild the archive** after editing. Duplicate mount attempts from other boot paths are not necessarily fatal: inspect their actual output.
</details>

<details>
<summary><strong>9. Shell never appears; system boots but no `/ #`</strong></summary>

**Where it fails:** BusyBox inittab/shell/serial console.

```bash
ls -l rootfs/initramfs/bin/sh rootfs/initramfs/sbin/init
cat rootfs/initramfs/etc/inittab
grep '^CONFIG_ASH=' configs/busybox.config
```

**Fix:** Restore BusyBox applet links and the `::askfirst:-/bin/sh` entry; verify PL011 console settings. If you changed initramfs scripts, repackage and test the updated archive.
</details>

<details>
<summary><strong>10. Modified `/init` or `rcS`, but QEMU still shows old behavior</strong></summary>

**Where it fails:** Stale archive or wrong selected init program.

```bash
stat rootfs/initramfs/etc/init.d/rcS output/initramfs.cpio.gz
gzip -dc output/initramfs.cpio.gz | cpio -t | grep -E '(init|rcS)'
```

**Fix:** Repackage a new `initramfs.cpio.gz` from the updated **staging directory** and explicitly boot that file. Note that `rdinit=/sbin/init` selects BusyBox init, **not** top-level `/init`.
</details>

<details>
<summary><strong>11. Kernel/BusyBox compile fails (`Error 2`, missing headers, wrong ARM flags)</strong></summary>

**Where it fails:** Ubuntu host compilation.

```bash
arm-linux-gnueabihf-gcc -v
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j1 zImage dtbs 2>&1 | tee kernel-build.log
```

**Fix:** Look for the **first** explicit `error:` or unsupported instruction in `kernel-build.log`. Recheck installed dependencies, `.config`, architecture, and effective compiler target. `make: *** Error 2` is generally a final symptom, not the actual cause. For unusual `isb` assembler diagnostics on a custom toolchain, investigate ARM mode/ARMv7-A target flags rather than editing unrelated kernel source.
</details>

<details>
<summary><strong>12. U-Boot host-tools / `rsa-sign.c` compilation problem</strong></summary>

**Where it fails:** Optional U-Boot 2022.04 build, not the direct-QEMU boot.

```bash
grep '^CONFIG_TOOLS_FIT_SIGNATURE=' configs/uboot.config
openssl version
```

**Fix:** Check the actual C compiler error, installed OpenSSL development headers and U-Boot 2022.04 tool compatibility. The saved config enables `CONFIG_TOOLS_FIT_SIGNATURE=y`. Disabling the **host signing tool** may be a lab-only workaround **if signing is not required**; do not disable security features for a product that relies on verified/secure boot. Test the resulting U-Boot separately.
</details>

<details>
<summary><strong>13. Git commit/push errors</strong></summary>

**Where it fails:** Git operations on Ubuntu, **not** Linux guest boot.

```bash
git status
git config --local user.name
git config --local user.email
gh auth status  # if GitHub CLI is installed
```

**Fix:** `Author identity unknown`: set commit name/email (prefer locally for this repository). `Authentication failed`: use `gh auth login` + `gh auth setup-git` or a GitHub PAT, **not your account password**. `fetch first`: fetch and rebase (resolve conflicts before push); **never force-push a shared `main`**. A `403` after successful authentication usually requires checking Write permission with the repository owner.
</details>

---

## 11. Debugging checklist

Use this **five-question procedure** instead of changing settings at random:

1. **Host or guest?** Do you see `user@ubuntu:...$` (host) or `/ #` (guest)?
2. **Which stage failed first?** Download → extract → config → compile → package → QEMU launch → kernel → PID 1 → shell.
3. **What was the first real error?** Capture the first `error:` or kernel panic message, not only the last line of output.
4. **Which exact artifact was used?** Check `pwd`, paths, timestamps, `file`, `.config`, and `gzip -t`.
5. **What one change was made?** Re-run the **same checkpoint**. If it fails, revert that single change before trying another.

### Keep an evidence trail (Ubuntu host)

```bash
# Command/environment evidence
qemu-system-arm --version
arm-linux-gnueabihf-gcc --version
pwd
ls -lh output/

# Save your build output when troubleshooting the kernel:
# make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j1 zImage dtbs \
#   2>&1 | tee kernel-build.log
```

If booting repeatedly, note the exact kernel, DTB and initramfs paths passed to QEMU. You can save the terminal log for reference, but avoid treating a rebuild as successful until it actually reaches the interactive guest shell and passes the runtime checks.

### “Where should I look first?”

| Error occurs... | Inspect... | Do **not** start by... |
| --- | --- | --- |
| Before `qemu-system-arm` starts | Host tools, paths, QEMU options | Editing kernel drivers |
| Before kernel messages | Serial setup, zImage, DTB | Rewriting `inittab` |
| After kernel panic during rootfs/init | initramfs, `/sbin/init`, device nodes | Reinstalling QEMU |
| After entering BusyBox shell | `inittab`, `rcS`, mounts, BusyBox symlinks | Rebuilding the entire kernel |
| During compile | Toolchain flags, packages, **first** compile error | Editing QEMU runtime arguments |
| During Git push | Authentication, remote history, Write permission | Rebuilding Lab 01 |

---

## 12. Optional: U-Boot and GitHub submission

### 12.1 U-Boot 2022.04: separate build (not part of verified boot)

To explore the U-Boot part of the lab, obtain **the matching release**, configure it for the intended target using `configs/uboot.config`, and build with the same ARM cross-compiler. This step is **optional for the direct-kernel QEMU path above**.

```bash
# Ubuntu host: additional upstream source (requires Git and network)
git clone --depth 1 --branch v2022.04 \
  https://github.com/u-boot/u-boot.git "$LAB_WORK/uboot-2022.04"

cd "$LAB_WORK/uboot-2022.04"
cp "$LAB_REPO/configs/uboot.config" .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2
file u-boot
```

**Limitations:** Whether this succeeds depends on the original board configuration, host libraries and U-Boot release compatibility. It does not establish that a particular U-Boot image will boot with the QEMU direct-boot command. The saved `output/u-boot` is a separate artifact.

### 12.2 Add only relevant files to GitHub

**Recommended submitted files:** configs, final boot images, the minimal source rootfs, README, report and diagram. **Do not commit:** `linux-5.15/` build objects, the entire unpacked source tree, `vmlinux.o`, large tar archives or personal downloads.

From an existing local checkout:

```bash
cd ~/LOS201
git status
git add embedded_lab1/README.md
git diff --cached --check
git commit -m "Add complete Lab 01 from-scratch guide"
git pull --rebase origin main
git push origin main
```

If `git pull --rebase` reports a conflict, **stop** and resolve it before pushing. Do not use `git push --force` on a shared repository. If prompted for credentials over HTTPS, use GitHub CLI or a Personal Access Token; GitHub account passwords are not accepted for Git operations.

**Note:** The command above stages *only the README*. Other changes should be reviewed and added intentionally.

---

## 13. Completion checklist and limitations

Before marking Lab 01 complete, confirm:

- [ ] Correct cross-compiler (`arm-linux-gnueabihf-gcc`) available on Ubuntu.
- [ ] Source versions match **Linux 5.15.0** and **BusyBox 1.35.0**.
- [ ] Kernel config includes VExpress, ARMv7, initramfs/gzip and PL011 console.
- [ ] `zImage` and `vexpress-v2p-ca9.dtb` exist.
- [ ] BusyBox executable is built for **ARM**; `/bin/sh` and `/sbin/init` are available.
- [ ] `inittab`, `rcS` and executable permissions are correct.
- [ ] Staging rootfs contains required special device nodes.
- [ ] `gzip -t initramfs.cpio.gz` passes and cpio lists startup files.
- [ ] QEMU reaches `/ #`.
- [ ] `uname -a`, `/proc/cpuinfo`, `hostname`, `mount` and `ps` are checked.
- [ ] Results are documented without claiming tests that were not run.

**Known limits:** The saved repository omits the upstream kernel, U-Boot and BusyBox source trees and intermediate build files. A fresh build depends on compatible versions and host packages. The provided runtime log verifies core boot and basic userspace, **not** networking, advanced drivers, real-hardware behavior, or U-Boot booting. The Git-tracked rootfs omits special device nodes; the supplied prebuilt cpio archive was used for the verified boot.

---

## 14. Reference links

- [QEMU — Arm Versatile Express boards](https://www.qemu.org/docs/master/system/arm/vexpress.html)
- [QEMU — Direct Linux boot](https://www.qemu.org/docs/master/system/linuxboot.html)
- [Linux kernel 5.x sources](https://www.kernel.org/pub/linux/kernel/v5.x/)
- [BusyBox 1.35.0 source](https://busybox.net/downloads/busybox-1.35.0.tar.bz2)
- [U-Boot source and release tags](https://docs.u-boot-project.org/en/latest/build/source.html)

---

**Result:** A successful Lab 01 boots the ARMv7-A kernel and BusyBox rootfs on QEMU, reaches `/ #`, and demonstrates the required kernel, init and pseudo-filesystem checks. Use the **quick start** for verification and the **from-scratch route** for learning and reproduction.
