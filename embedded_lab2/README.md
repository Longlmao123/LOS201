# Lab 02 - Linux Character Device Driver, procfs and sysfs

> **Course:** LOS201 - Embedded Linux Systems<br>
> **Target:** ARMv7-A / Cortex-A9 on QEMU `vexpress-a9`<br>
> **Kernel:** Linux 5.15.0 | **Userspace:** BusyBox (ARM, statically linked)<br>
> **Lab driver:** `lab2_driver.ko` | **Device:** `/dev/lab2` | **Major/minor:** `240:0`

**Start here:** If you only want to run the submitted build, follow **Path A**. To learn how to assemble and build Lab 02 yourself, follow **Path B**. If a command fails, use the *Checkpoint / If it fails* box in that step and the troubleshooting reference at the end.

> **Evidence policy:** The supplied `logs/lab2_full_test.log` demonstrates a successful QEMU boot, module load, Character Driver read/write tests and `DRIVER_TEST_EXIT_CODE=0`. The procfs/sysfs interface values were observed in the conversation, and the student separately reported that the updated procfs/sysfs script passed; **that separate run is not present in the uploaded log**. This README does not present unrecorded tests as logged evidence.

## Contents

1. [What you will build](#1-what-you-will-build)
2. [How the driver works](#2-how-the-driver-works)
3. [Folder layout and prerequisites](#3-folder-layout-and-prerequisites)
4. [Path A - Run the provided build](#4-path-a---run-the-provided-build)
5. [Path B - Build Lab 02 from scratch](#5-path-b---build-lab-02-from-scratch)
6. [Test the Character Device](#6-test-the-character-device)
7. [Test procfs and sysfs](#7-test-procfs-and-sysfs)
8. [Record and interpret the results](#8-record-and-interpret-the-results)
9. [Troubleshooting: symptoms, checks and fixes](#9-troubleshooting-symptoms-checks-and-fixes)
10. [Code walkthrough](#10-code-walkthrough)
11. [Limitations and safe improvements](#11-limitations-and-safe-improvements)
12. [Submission checklist and Git](#12-submission-checklist-and-git)

---

## 1. What you will build

Lab 02 introduces a loadable Linux **Character Device Driver** for an ARMv7 virtual machine. It provides three ways to interact with the same kernel module:

| Interface | Purpose | User command |
| --- | --- | --- |
| `/dev/lab2` | Write bytes into a 1,024-byte kernel buffer and read them back | `echo ... > /dev/lab2`; `cat /dev/lab2` |
| `/proc/lab2_info` | View human-readable driver statistics | `cat /proc/lab2_info` |
| `/sys/class/lab2_class/lab2/` | View read-only driver attributes | `cat .../buffer_len`, `open_count`, `last_data` |

**Key parameters from the supplied source:** `DEVICE_NAME="lab2"`, `CLASS_NAME="lab2_class"`, `DRIVER_MAJOR=240`, `BUFFER_SIZE=1024`, `MODULE_VERSION="1.0"`. The driver accepts at most **1,023 data bytes per write**, retaining one byte for the string terminator. An oversized write is **truncated**, not rejected.

[View the architecture diagram (PNG)](diagrams/Lab02_Character_Driver_Architecture.png) · [Open the diagram (PDF)](diagrams/Lab02_Character_Driver_Architecture.pdf) · [Open the lab report (PDF)](report/Lab02_Report.pdf)

### Boot flow in one minute

```text
Ubuntu host -> QEMU (-M vexpress-a9)
   -> Linux 5.15 zImage + matching DTB + Lab 02 initramfs
   -> BusyBox /sbin/init (PID 1)
   -> /etc/inittab -> /etc/init.d/rcS
   -> mount /proc, /sys, /tmp, /dev (when supported)
   -> insmod lab2_driver.ko
   -> create /dev/lab2 (240:0) + /proc/lab2_info + sysfs attributes
   -> interactive BusyBox shell (/ #)
```

This is **direct QEMU kernel boot**. U-Boot is **not** executed by the command in this lab.

## 2. How the driver works

```text
                      Userspace (BusyBox)
       +-------------------+----------------------+-------------------+
       |                   |                      |                   |
 echo/cat /dev/lab2   cat /proc/lab2_info    cat .../buffer_len, ...
       |                   |                      |
      VFS                procfs                  sysfs
       |                   |                      |
  lab2_open/read/     lab2_proc_show()      *_show() (read-only)
  write/release            |                      |
       +-------------------+----------------------+
                           |
                 lab2_driver.ko (kernel)
                 device_buffer[1024]
                 buffer_len, open_count
```

- `echo` opens the device and writes text **plus a newline**. `cat` opens it and reads until EOF.
- `/proc` displays driver name, major, capacity, current length, opens and last data.
- `/sys` exposes **read-only** `buffer_len`, `open_count` and `last_data`. Here, `buffer_len` is **current valid data length**, **not** the 1,024-byte capacity.
- This module registers **major 240 explicitly**. If another driver already owns that major, registration fails. The number is specific to this exercise, not a universally reserved production major.

## 3. Folder layout and prerequisites

### 3.1 Repository files

```text
LOS201/
├── embedded_lab1/
│   └── output/
│       ├── zImage                     # verified Linux 5.15 ARM kernel
│       └── vexpress-v2p-ca9.dtb      # matching Device Tree Blob
└── embedded_lab2/
    ├── README.md
    ├── driver/
    │   ├── lab2_driver.c             # original lab source (C)
    │   ├── Makefile                  # out-of-tree kernel module build
    │   └── lab2_driver.ko            # built ARM module (5.15.0)
    ├── rootfs/initramfs/
    │   ├── bin/busybox               # ARM static BusyBox
    │   ├── sbin/init                 # BusyBox symlink
    │   ├── etc/inittab
    │   ├── etc/init.d/rcS
    │   ├── lib/modules/5.15.0/lab2_driver.ko
    │   └── usr/bin/
    │       ├── test_driver.sh
    │       └── test_procfs.sh
    ├── output/initramfs_lab2.cpio.gz
    ├── logs/lab2_full_test.log       # supplied boot + Character Driver log
    ├── diagrams/
    └── report/
```

`configs/` is empty in the submitted Lab 02 archive. The build depends on the **Lab 01 kernel source/configuration**, which is **not included** in this Lab 02 archive. The uploaded archive also does not include a completed MTD/JFFS2 test log. These should not be silently advertised as completed Lab 02 artifacts.

### 3.2 Install the host tools (Ubuntu)

Commands in this section run on the **Ubuntu host** (`user@host:~$`), **not** in QEMU (`/ #`).

```bash
sudo apt update
sudo apt install -y qemu-system-arm gcc-arm-linux-gnueabihf \
  binutils-arm-linux-gnueabihf make bc bison flex \
  libssl-dev libelf-dev libncurses-dev \
  cpio gzip file kmod device-tree-compiler git
```

Verify:

```bash
command -v qemu-system-arm
command -v arm-linux-gnueabihf-gcc
command -v cpio
qemu-system-arm --version
arm-linux-gnueabihf-gcc --version
```

**Checkpoint:** every `command -v` returns a path. If not, install the named tool on **Ubuntu** and retry. Toolchain versions can differ from the compiler used for the supplied `.ko`.

### 3.3 Get the files

```bash
cd ~
git clone https://github.com/Longlmao123/LOS201.git
cd ~/LOS201
ls embedded_lab1/output/{zImage,vexpress-v2p-ca9.dtb}
ls embedded_lab2/driver/{lab2_driver.c,Makefile,lab2_driver.ko}
```

If `~/LOS201` already exists, **do not clone on top of it**; run `cd ~/LOS201 && git pull --ff-only` only when your working tree is clean and you want remote updates.

## 4. Path A - Run the provided build

Use this path first to distinguish an environment problem from a source-build problem.

### A1. Check the three boot artifacts

```bash
cd ~/LOS201/embedded_lab2
ls -lh ../embedded_lab1/output/zImage \
  ../embedded_lab1/output/vexpress-v2p-ca9.dtb \
  output/initramfs_lab2.cpio.gz
gzip -t output/initramfs_lab2.cpio.gz
file driver/lab2_driver.ko rootfs/initramfs/bin/busybox
readelf -p .modinfo driver/lab2_driver.ko | grep vermagic
```

**Expected:** gzip integrity check succeeds; `.ko` reports ARM relocatable; BusyBox reports ARM statically linked; module `vermagic` begins with `5.15.0` (the archived module reports `5.15.0 SMP mod_unload ARMv7 p2v8`). **If it fails:** verify paths and do not boot a truncated archive.

### A2. Start QEMU (copy the entire command)

```bash
cd ~/LOS201/embedded_lab2
qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ../embedded_lab1/output/zImage \
  -dtb ../embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs_lab2.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

**Expected at boot:**

```text
[ LAB-02 ] Loading lab2_driver module...
lab2_driver: loaded, major=240
[ LAB-02 ] lab2_driver loaded OK
[ LAB-02 ] System initialization complete!
Please press Enter to activate this console.
```

Press **Enter** for the guest prompt `/ #`. QEMU is now running Linux/ARM; Ubuntu-host paths and `apt` are not available in the guest.

**Exit QEMU:** press **Ctrl+A**, release, then **X**. This QEMU keyboard shortcut applies to `-nographic` serial mode.

### A3. Verify basic interfaces inside QEMU (`/ #`)

```sh
uname -a
lsmod
cat /proc/devices | grep -w 240
ls -l /dev/lab2
cat /proc/lab2_info
ls -l /sys/class/lab2_class/lab2
```

**Expected:** `5.15.0`, `armv7l`, `lab2_driver` in `lsmod`, character node major/minor `240,0`, procfs statistics and sysfs symlink/attributes. If the device exists but `cat /dev/lab2` reports an error, compare the module's registered major with `ls -l /dev/lab2`.

## 5. Path B - Build Lab 02 from scratch

> **Important:** The supplied `.ko` and compressed initramfs are demonstrably bootable. The following commands are a **reproduction recipe**, not a claim that a clean-room rebuild was performed while writing this documentation. Build in a **separate workspace**; never overwrite the verified image until the new one passes QEMU tests.

### B1. Obtain a configured Linux 5.15 kernel build tree

You need the **same kernel release and compatible configuration/toolchain** that will run in QEMU. The actual Makefile assumes:

```text
KDIR=$(HOME)/embedded_lab1/kernel/linux-5.15
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
```

If you already completed Lab 01 using that source directory, verify it:

```bash
KDIR="$HOME/embedded_lab1/kernel/linux-5.15"
test -f "$KDIR/Makefile" && echo 'Kernel sources present'
test -f "$KDIR/.config" && echo 'Kernel configuration present'
```

If starting **without Lab 01 source**, obtain a trusted Linux **5.15.0** source tree and the corresponding build configuration (for example, the `embedded_lab1/configs/kernel.config` retained in this repository). A simple reference workflow is:

```bash
mkdir -p ~/embedded_lab1/kernel
cd ~/embedded_lab1/kernel
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.tar.xz
# Verify the downloaded file before extracting.
tar -xf linux-5.15.tar.xz
cd linux-5.15
cp ~/LOS201/embedded_lab1/configs/kernel.config .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage dtbs modules
```

**Check:**

```bash
file arch/arm/boot/zImage
find arch/arm/boot/dts -name 'vexpress-v2p-ca9.dtb' -print
make -s ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- kernelrelease
```

The release should be compatible with the module you intend to load (`5.15.0` here). **Do not simply rename a different-version kernel or `.ko`.** For more about constructing Linux/BusyBox from nothing, also see the [Lab 01 README](../embedded_lab1/README.md).

**If an assembler error mentions unsupported ARM instructions (`isb`, Thumb/architecture flags):** confirm the cross-compiler target, kernel config and chosen toolchain; the setup used in an earlier lab environment sometimes required `KCFLAGS="-march=armv7-a -Wa,-march=armv7-a"`. Treat that as a **diagnostic workaround**, not a universal default; rebuild and boot-test afterward.

### B2. Build the actual Character Device module

```bash
cd ~/LOS201/embedded_lab2/driver
make KDIR="$HOME/embedded_lab1/kernel/linux-5.15"
file lab2_driver.ko
readelf -p .modinfo lab2_driver.ko | grep -E 'name=|vermagic=|version='
```

**Expected:** an ARM 32-bit **relocatable** `.ko` with `name=lab2_driver` and a `vermagic` compatible with the running kernel. Its Makefile uses Linux's standard out-of-tree build, `make -C $(KDIR) M=$(PWD) ... modules`.

**If it fails:**

- `.../build: No such file or directory`: incorrect `KDIR`, missing prepared/built kernel tree.
- `No rule to make target 'modules'` or configuration errors: prepare/configure the kernel tree and build the matching kernel first.
- `unsupported architecture` / assembler instructions: examine the ARM cross-compiler and flags.
- `Invalid module format` in QEMU later: `uname -r` and `.ko` `vermagic` do not match, or another module-compatibility constraint differs; review `dmesg` before rebuilding.

### B3. Prepare a safe RootFS staging directory

Use the original lab's **known layout** and preserve the symbolic links; do not create a second `roots/` directory. This package includes an inspectable `rootfs/initramfs/` tree. It contains BusyBox, startup scripts and the module, but **Git does not preserve special device nodes**.

```bash
cd ~/LOS201/embedded_lab2
STAGE="$HOME/embedded_lab2_stage"
mkdir -p "$STAGE"
cp -a rootfs/initramfs/. "$STAGE/"
mkdir -p "$STAGE/lib/modules/5.15.0" "$STAGE/dev" "$STAGE/proc" "$STAGE/sys" "$STAGE/tmp"
cp driver/lab2_driver.ko "$STAGE/lib/modules/5.15.0/lab2_driver.ko"
chmod +x "$STAGE/etc/init.d/rcS" \
  "$STAGE/usr/bin/test_driver.sh" "$STAGE/usr/bin/test_procfs.sh"
```

**Checkpoint:**

```bash
file "$STAGE/bin/busybox"
ls -l "$STAGE/sbin/init" "$STAGE/etc/inittab" "$STAGE/etc/init.d/rcS"
ls -l "$STAGE/lib/modules/5.15.0/lab2_driver.ko"
sh -n "$STAGE/etc/init.d/rcS" "$STAGE/usr/bin/test_driver.sh" "$STAGE/usr/bin/test_procfs.sh"
```

`/sbin/init` is a symlink to BusyBox. The demonstrated QEMU command uses **`rdinit=/sbin/init`**, so the absence of a top-level `/init` in this Lab 02 RootFS is intentional for this boot path.

> **Very important:** Do not run `insmod` on the Ubuntu host. `lab2_driver.ko` targets ARM/Linux 5.15. It must be inserted **inside the ARM QEMU guest**.

### B4. Understand startup files before packaging

`etc/inittab` in the submitted RootFS:

```text
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/bin/umount -a -r
```

`etc/init.d/rcS` mounts procfs/sysfs/tmpfs, tries devtmpfs for `/dev`, creates basic `/dev` nodes where necessary, sets hostname `embedded-lab2`, inserts `/lib/modules/5.15.0/lab2_driver.ko` and creates `/dev/lab2` with major/minor `240:0`.

**Inspect your copy; do not replace the verified file blindly:**

```bash
sed -n '1,180p' "$STAGE/etc/init.d/rcS"
grep -nE 'mount |hostname|insmod|mknod|chmod' "$STAGE/etc/init.d/rcS"
```

**Watch out:** the current `rcS` prints that `/dev/lab2` was created after `mknod` without independently checking whether that call succeeded. The reliable checkpoint is `ls -l /dev/lab2` **inside QEMU**, followed by an actual read/write test.

### B5. Recreate device nodes only in staging

The Git checkout does not include special device-node metadata. Create them in the **staging directory**, never on the host's real `/dev` and never in `~/LOS201`:

```bash
cd "$STAGE"
# Run only if the corresponding node does not already exist.
[ -e dev/console ] || sudo mknod -m 600 dev/console c 5 1
[ -e dev/null ]    || sudo mknod -m 666 dev/null    c 1 3
[ -e dev/zero ]    || sudo mknod -m 666 dev/zero    c 1 5
[ -e dev/random ]  || sudo mknod -m 666 dev/random  c 1 8
ls -l dev/console dev/null dev/zero dev/random
```

**Expected:** a `c` at the start of each device listing and major/minor values `5,1`, `1,3`, `1,5`, `1,8`. If device creation is denied, use a host with permitted privileges or preserve the original tested initramfs; ordinary Git checkouts do not restore character devices.

### B6. Package a *new*, non-destructive initramfs

```bash
mkdir -p ~/embedded_lab2_build
NEW="$HOME/embedded_lab2_build/initramfs_lab2_rebuilt.cpio.gz"
# The stage should contain only the intended RootFS contents.
cd "$STAGE"
sudo find . -print0 | sudo cpio --null -o --format=newc --quiet | gzip -9 > "$NEW"
gzip -t "$NEW"
gzip -dc "$NEW" | cpio -t 2>/dev/null | \
  grep -E '(lab2_driver.ko|test_driver.sh|test_procfs.sh|inittab|rcS|busybox)'
```

**Checkpoint:** `gzip -t` exits `0`; the archive lists `/sbin/init`, the module, startup files and both tests. If file names have a leading `./`, that is normal for this cpio command.

**Do not overwrite** `output/initramfs_lab2.cpio.gz` yet. Test `"$NEW"` first, then copy it only if it boots and passes the tests.

### B7. Boot your rebuilt image

From the host, run the Path A QEMU command, but replace its `-initrd` value with your *absolute* rebuilt image path:

```text
-initrd /home/YOUR_USERNAME/embedded_lab2_build/initramfs_lab2_rebuilt.cpio.gz
```

Do not paste the placeholder username literally. On the guest, run the test commands in Sections 6 and 7. If the newly rebuilt module does not load, compare kernel `uname -r`, `vermagic`, ABI and `dmesg` against the known-good image.

## 6. Test the Character Device

Run in **QEMU** at `/ #`:

```sh
sh /usr/bin/test_driver.sh
echo "DRIVER_TEST_EXIT_CODE=$?"
```

This script checks that the module is already loaded (otherwise inserts it), finds or creates the character node, writes a message, reads it back, compares values, then repeats with `Message_1` through `Message_3`.

**Successful run (logged in the uploaded archive):**

```text
PASS: lab2_driver is already loaded
PASS: Character device exists
PASS: WRITE completed
PASS: READ data matches WRITE data
PASS: Message_1
PASS: Message_2
PASS: Message_3
=== CHARACTER DRIVER TEST PASSED ===
DRIVER_TEST_EXIT_CODE=0
```

Then inspect:

```sh
cat /proc/lab2_info
dmesg | grep lab2 | tail -20
```

**Why the script checks `/proc/modules` first:** `rcS` already inserts the module during boot. Running `insmod` a second time raises `File exists`; this means the module is present, not that its first load failed. The original test script stopped on that harmless duplicate insertion; the updated script does not.

## 7. Test procfs and sysfs

Inside **QEMU**:

```sh
sh /usr/bin/test_procfs.sh
echo "PROCFS_TEST_EXIT_CODE=$?"
```

The script reads procfs, writes `Embedded Linux Lab2 Test Data` into the character device, confirms the string appears in procfs, then verifies sysfs `buffer_len`, `open_count` and `last_data`. It only prints the final `PASSED` marker if its required checks succeed.

Manual inspection:

```sh
cat /proc/lab2_info
cat /sys/class/lab2_class/lab2/buffer_len
cat /sys/class/lab2_class/lab2/open_count
cat /sys/class/lab2_class/lab2/last_data
```

**Meaning of the values:**

- `buffer_len`: number of bytes currently stored, e.g. `30` after `echo 'Hello from userspace, LAB-02!'` (`echo` adds `\n`).
- `open_count`: cumulative number of **opens of `/dev/lab2`**, not the number of procfs or sysfs reads; it increases with reads and writes.
- `last_data`: last payload stored in the device buffer; the current driver prints it as text.

**Evidence note:** The student observed `buffer_len=30`, `open_count=8` and matching `last_data` manually, and reported the updated procfs/sysfs script passed. However, the uploaded `lab2_full_test.log` records **only** the Character Driver script and its exit code. Capture a separate test log if you require audited proof for procfs/sysfs.

## 8. Record and interpret the results

To generate a **new combined** evidence log on the Ubuntu host:

```bash
cd ~/LOS201/embedded_lab2
mkdir -p logs
script -q -f -c "qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel $HOME/LOS201/embedded_lab1/output/zImage \
  -dtb $HOME/LOS201/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs_lab2.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'" \
  logs/lab2_full_test_combined.log
```

Once in QEMU (`/ #`), **type both tests** and record exit codes immediately:

```sh
sh /usr/bin/test_driver.sh
echo "DRIVER_TEST_EXIT_CODE=$?"
sh /usr/bin/test_procfs.sh
echo "PROCFS_TEST_EXIT_CODE=$?"
cat /proc/lab2_info
lsmod
```

Exit using **Ctrl+A, X**, then on the Ubuntu host:

```bash
grep -aE 'TEST PASSED|TEST FAILED|DRIVER_TEST_EXIT_CODE|PROCFS_TEST_EXIT_CODE' \
  logs/lab2_full_test_combined.log
```

**Interpretation:** An exit code `0` is meaningful **only** alongside the script's comparisons and `PASSED` marker. The `script` utility may add terminal control characters; `grep -a` handles its raw terminal recording. Preserve the original `.log` as the audit source.

### Results supported by uploaded evidence

| Check | Observed | Evidence |
| --- | --- | --- |
| QEMU guest | Linux 5.15.0, ARMv7, BusyBox console | `logs/lab2_full_test.log` |
| Module registration | `lab2_driver: loaded, major=240` | Same log |
| Character node | `/dev/lab2`, `240:0` | Same log |
| Read/write equality | Initial string + `Message_1..3` | Same log |
| Script exit | `DRIVER_TEST_EXIT_CODE=0` | Same log |
| Procfs/sysfs manual read | Matching attributes displayed | Earlier student terminal output in conversation |
| Updated procfs/sysfs script | Student reported PASS | **Not present in supplied log** |
| MTD/JFFS2 | Not established by this submission | No dedicated evidence |

## 9. Troubleshooting: symptoms, checks and fixes

Use the **first failing stage** rather than changing several things at once.

| Symptom | First check (where?) | Likely direction |
| --- | --- | --- |
| `qemu-system-arm: command not found` | `command -v qemu-system-arm` (host) | Install QEMU |
| `Could not open ... zImage` | `pwd; ls ../embedded_lab1/output/` (host) | Run from correct folder |
| Black serial screen | QEMU `-nographic`, `console=ttyAMA0,115200`, DTB | Restore tested arguments |
| `No working init found` | `cpio -t` for `sbin/init`; `file busybox` | Repair initramfs / ARM BusyBox |
| `insmod: File exists` | `lsmod` (guest) | Already loaded; do **not** insert again |
| `insmod: No such file` on Ubuntu | Prompt is `user@host:$` | Start QEMU; use guest `/ #` |
| `Invalid module format` | Guest `uname -r`, `dmesg`, host `readelf ... | grep vermagic` | Rebuild `.ko` for exact kernel |
| `Unknown symbol` | `dmesg | tail -50` | Kernel config/symbol mismatch |
| `Device or resource busy` during module registration | `cat /proc/devices` | Major `240` conflict |
| `/dev/lab2` missing | `lsmod`, `ls -l /dev/lab2`, `rcS` | Check load success and `mknod` |
| `/proc/lab2_info` missing | `lsmod`, `dmesg | grep lab2` | `proc_create` may have failed |
| `/sys/class/lab2_class/lab2` missing | `mount | grep sysfs`, `dmesg` | Check sysfs mount/class/device creation |
| `buffer_len` differs from expected | `printf` vs `echo`, content length | Remember `echo` appends `\n` |
| Test prints PASS but incorrect data | Inspect shell script's comparisons | Fix assertions, not just echo lines |
| Edited script still behaves like old one | `cpio -t`; repackage `-initrd` image | RootFS tree != compressed archive |
| Kernel module builds for x86 | `file lab2_driver.ko`, `CROSS_COMPILE` | Use ARM cross-toolchain |
| `No rule to make target ... modules` | `KDIR`, kernel `.config` | Prepare matching kernel tree |
| Git can't store `/dev/console` | `ls -l` host vs stage | Create nodes in staging; package cpio |
| Git push rejected (`fetch first`) | `git status`; `git fetch origin` | Rebase clean work; never force-push team branch |

<details>
<summary><strong>Case 1 - Module is already loaded but a test aborts</strong></summary>

Inside QEMU:

```sh
lsmod | grep lab2_driver
grep '^lab2_driver ' /proc/modules
```

The startup `rcS` loads the module automatically. `File exists` from a second `insmod` is expected. Use the updated test script which checks `/proc/modules` before insertion. **Do not** unload a working module just to bypass a duplicate-load check.
</details>

<details>
<summary><strong>Case 2 - Module file exists but cannot be inserted</strong></summary>

Host:

```bash
file driver/lab2_driver.ko
readelf -p .modinfo driver/lab2_driver.ko | grep vermagic
```

Guest:

```sh
uname -r
lsmod
dmesg | tail -50
```

The supplied `.ko` is an **ARM EABI5** module for **Linux 5.15.0**. Compare release, SMP/module flags and the exact error in `dmesg`. A file compiled for ARM cannot be loaded into Ubuntu x86-64.
</details>

<details>
<summary><strong>Case 3 - Device exists, but read/write does not work</strong></summary>

```sh
cat /proc/devices | grep -E '(^| )240($| )'
ls -l /dev/lab2
echo 'abc' > /dev/lab2
cat /dev/lab2
dmesg | tail -30
```

The device file must be a **character** node (`c`) with **major 240, minor 0** for this build. If the major differs, correct the registration or device node only after confirming the driver's actual value. `cat` on the device should reach EOF rather than loop infinitely because the driver updates the read offset.
</details>

<details>
<summary><strong>Case 4 - procfs or sysfs data is absent</strong></summary>

```sh
mount | grep -E '(/proc|/sys)'
ls -l /proc/lab2_info
ls -l /sys/class/lab2_class/lab2
cat /proc/lab2_info
dmesg | grep lab2
```

Check that `rcS` mounted virtual filesystems and the module initialized them. Note that `buffer_len` is the number of **valid bytes**; use `wc -c` or `printf` to check expected lengths. Sysfs attributes are read-only in this source code.
</details>

<details>
<summary><strong>Case 5 - New RootFS files are missing after reboot</strong></summary>

Changing `rootfs/initramfs/usr/bin/test_driver.sh` alone does **not** modify the `.cpio.gz` that QEMU boots. Build a new archive from the staging directory and select that new image with `-initrd`. Keep the last known-good image as a fallback. Be especially careful when `rootfs/` and another similarly named directory such as `roots/` coexist.
</details>

<details>
<summary><strong>Case 6 - Host terminal versus guest terminal</strong></summary>

- **Host:** `user@ubuntu:~/LOS201$` - `make`, `git`, `qemu-system-arm`, `cpio`.
- **Guest:** `/ #` - `insmod`, `lsmod`, `cat /proc/lab2_info`, read/write `/dev/lab2`.

The path `/lib/modules/5.15.0/lab2_driver.ko` is inside the guest initramfs. Typing it in Ubuntu addresses the Ubuntu `/lib`, not the QEMU guest.
</details>

## 10. Code walkthrough

The driver is implemented in [`driver/lab2_driver.c`](driver/lab2_driver.c). Read the functions in this order:

| Function / construct | What it does | Important detail |
| --- | --- | --- |
| `register_chrdev(240, ..., &lab2_fops)` | Registers major 240 | May fail if the major is occupied |
| `class_create(...)` | Creates `lab2_class` | The exact API signature is **kernel-version dependent** |
| `device_create(...)` | Exposes device in sysfs | Does not guarantee a `/dev` node unless devtmpfs/userspace creates it |
| `lab2_open()` | Increments `open_count` | Uses a mutex for the counter increment |
| `lab2_write()` | Copies userspace bytes to kernel buffer | Uses `copy_from_user`, truncates to 1023 bytes and adds `\0` |
| `lab2_read()` | Copies bytes to userspace | Uses `copy_to_user`, updates file offset and returns 0 at EOF |
| `lab2_proc_show()` + `seq_file` | Prints statistics in `/proc/lab2_info` | Current read path does not acquire the data mutex |
| `DEVICE_ATTR_RO(...)` | Defines read-only sysfs fields | Current show functions do not acquire the data mutex |
| `proc_create(...)` | Registers procfs endpoint | Diagnostic warning if creation fails |
| `sysfs_create_group(...)` | Registers three attributes | Failure currently warns but still returns module initialization success |
| `lab2_exit()` | Releases procfs, sysfs, class and device | Module teardown is required on unload |

### Why `read()` returns 0 at EOF

```c
if (*offset >= buffer_len)
    return 0;
```

Userspace `cat` keeps calling `read()` until it receives `0`. Returning a positive length forever can cause `cat` to loop indefinitely.

### Why `copy_from_user` and `copy_to_user` matter

Pointers from userspace must be accessed through kernel's designated user-copy helpers. Ordinary `memcpy` with a userspace pointer is unsafe. Errors should propagate as `-EFAULT` (as this code does on failed copy).

### Why `echo` appears to write one more byte

```sh
# echo includes a newline
printf '%s\n' 'Message_1' | wc -c
# 'Message_1' has 9 characters; with newline => 10 bytes.
```

That matches the uploaded log: the driver reports **10 bytes** for each `Message_1..3` write.

## 11. Limitations and safe improvements

The following are **code-review observations**, not failed test results:

1. **Concurrency:** `lab2_write()` and `lab2_open()` use `lab2_mutex`, but `lab2_read()`, procfs show and sysfs show access shared state without equivalent locking. For multi-process use, review consistent locking/snapshot semantics and avoid unsafe concurrent access.
2. **Registration failure handling:** `sysfs_create_group()` and `proc_create()` can fail while `lab2_init()` still reports success. Consider returning an error with complete reverse-order cleanup when required interfaces fail; only remove a sysfs group during unload if it was created successfully.
3. **Fixed major:** `240` can conflict with another driver; dynamically allocated major numbers are preferable in reusable drivers. Here, the test scripts and `rcS` assume `240:0`.
4. **Single global buffer:** all processes share the same buffer; one write replaces previous data. The driver is for teaching, not for isolation or robust concurrent I/O.
5. **Text-only display:** procfs/sysfs use `%s`; arbitrary binary data containing `\0` will not display completely. Byte-count-based handling is needed for binary data.
6. **Write semantics:** oversized requests are truncated. The implementation also does not use `*offset` for writes, so it behaves as a last-message device, not as a seekable regular file.
7. **Scope:** the supplied evidence does not establish MTD/JFFS2, production hardware, high-load concurrency or other Linux versions. Those require separate experiments.

The archived module was built for Linux 5.15 and its API signatures may not compile unchanged against much newer kernels.

## 12. Submission checklist and Git

### Before you submit

- [ ] `driver/lab2_driver.c`, `Makefile` and ARM `.ko` are present.
- [ ] The RootFS contains BusyBox, `/sbin/init`, `inittab`, `rcS`, module and both tests.
- [ ] `gzip -t output/initramfs_lab2.cpio.gz` succeeds.
- [ ] QEMU reaches `/ #` and `lsmod` shows `lab2_driver`.
- [ ] `/dev/lab2` is a character node with major/minor `240:0`.
- [ ] `test_driver.sh` ends with `PASSED` and exit code `0`.
- [ ] `test_procfs.sh` ends with `PASSED` and exit code `0`.
- [ ] Capture **both** tests in a saved log if the evaluator needs verifiable evidence.
- [ ] `README.md`, architecture diagram and report are in place.
- [ ] No huge Linux source tree, host build objects, passwords or tokens are committed.

### Commit only the Lab 02 materials

Run on Ubuntu in `~/LOS201`:

```bash
cd ~/LOS201
git status --short
git add embedded_lab2
git diff --cached --stat
git diff --cached --check
git commit -m 'Document and submit Embedded Linux Lab 02'
git pull --rebase origin main
git push origin main
```

If `git pull --rebase` reports conflicts, **stop** and inspect `git status` before making changes. Never force-push to a shared `main` branch as a shortcut.

---

**Verification summary:** The provided archive proves successful QEMU startup and Character Driver testing. The documented procfs/sysfs results also draw on the student's separate terminal output and confirmation. This guide uses the supplied driver source and recorded artifacts; it does not imply that new builds or concurrent stress tests were executed during documentation.
