ASSIGNMENT 01 - EMBEDDED LINUX SYSTEMS
Pham Tran Gia Long - SE201095

CONTENTS
- task1_ioctl: source, shared ioctl header, Makefile, ARM module, test script,
  userspace helper source and static ARM helper binary.
- task2_misc: counter source, Makefile, ARM module and test script.
- logs: post-fix test output and full dmesg; raw sessions for traceability.
- bao_cao: 18-page PDF report.

TARGET
Linux 5.15.0 SMP ARMv7; QEMU vexpress-a9, Cortex-A9, 2 CPUs, 512 MB RAM.
Compiler used: arm-none-linux-gnueabihf-gcc 13.2.1.

BUILD ON DEVELOPMENT HOST
Both Makefiles default to $HOME/embedded_lab1/kernel/linux-5.15.
To use another prepared kernel tree:
  make -C task1_ioctl KDIR=/absolute/path/to/linux-5.15
  make -C task2_misc KDIR=/absolute/path/to/linux-5.15
Build helper from task1_ioctl:
  arm-none-linux-gnueabihf-gcc -static -O2 -Wall -Wextra asgn1_ctl.c -o asgn1_ctl
Do not load these ARM modules into an x86 Ubuntu kernel.

RUN IN TARGET ROOTFS
Copy both modules into /lib/modules/5.15.0/.
Copy asgn1_ctl, test_asgn1.sh and test_counter.sh into /usr/bin/, mode 755.
Ensure procfs, sysfs and devtmpfs are mounted; run as root.
Run test_counter.sh, then test_asgn1.sh. Modules must be initially unloaded.
test_asgn1.sh pauses for Enter at screenshot checkpoints.

RECORDED RESULT
The post-fix session ended on 2026-10-21 at 13:01:26 +07:00.
Both scripts printed PASSED; shell recorded both exit statuses as 0.
The FD-hold unload experiment is in task2_session_raw.txt, from an earlier
manual run. That run recorded an unexpected rmmod shell exit status of 0;
module presence and refcount, not that status, establish the blocked unload.
See the report for limitations of the tested cases.

VERIFY FILE INTEGRITY
  sha256sum -c SHA256SUMS
