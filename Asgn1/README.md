# Assignment 01 — IOCTL and Miscellaneous Device Drivers

[Back to Main Page](../README.md) · [Task 1](task1_ioctl/) · [Task 2](task2_misc/) · [Logs](logs/) · [PDF Report](bao_cao/SE201095_PhamTranGiaLong_Asgn01_BaoCao.pdf)

Assignment 01 extends the character driver concepts from Lab 02 by introducing the **IOCTL interface**, a userspace control application, and a **counter driver implemented with miscdevice**. Source code, Makefiles, ARM kernel modules, test scripts, and execution evidence are stored together in the repository.

## Contents

- [Objectives and Organization](#objectives-and-organization)
- [Task 1 — IOCTL Driver](#task-1--ioctl-driver)
- [Task 2 — Counter Miscdevice](#task-2--counter-miscdevice)
- [Building on the HOST](#building-on-the-host)
- [Testing on the ARM Target](#testing-on-the-arm-target)
- [Evidence and Analysis](#evidence-and-analysis)
- [Lessons Learned](#lessons-learned)

## Objectives and Organization

| Path | Description |
|---|---|
| [task1_ioctl/asgn1_driver.c](task1_ioctl/asgn1_driver.c) | Character driver and IOCTL callbacks |
| [task1_ioctl/asgn1_ioctl.h](task1_ioctl/asgn1_ioctl.h) | Shared ABI between the driver and userspace |
| [task1_ioctl/asgn1_ctl.c](task1_ioctl/asgn1_ctl.c) | IOCTL control application |
| [task1_ioctl/test_asgn1.sh](task1_ioctl/test_asgn1.sh) | Task 1 test script |
| [task2_misc/counter_driver.c](task2_misc/counter_driver.c) | Miscdevice counter driver and procfs interface |
| [task2_misc/test_counter.sh](task2_misc/test_counter.sh) | Task 2 test script |
| [logs/](logs/) | Test outputs and kernel logs |
| [bao_cao/](bao_cao/) | Design analysis and report |

Environment: Ubuntu HOST, ARM GNU Toolchain 13.2.1, Linux 5.15.0 target kernel, and QEMU Cortex-A9.

The Task 2 directory in the repository is named **`task2_misc`**. Some original logs may still use the earlier working name `task2_counter`.

## Task
