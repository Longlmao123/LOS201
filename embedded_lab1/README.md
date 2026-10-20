# Embedded Linux - Lab 01

## 1. Overview

This lab demonstrates how to build and run an Embedded Linux system using QEMU ARM.

### Development Environment

- Target Architecture: ARM Cortex-A9
- QEMU Machine: vexpress-a9
- Linux Kernel: 5.15
- U-Boot: 2022.04
- BusyBox: 1.35.0

## 2. Directory Structure

embedded_lab1/
├── config/
│   ├── kernel.config
│   ├── uboot.config
│   └── busybox.config
├── rootfs/
│   └── initramfs/
│       ├── init
│       ├── bin/
│       ├── sbin/
│       ├── etc/
│       ├── dev/
│       ├── proc/
│       ├── sys/
│       └── tmp/
└── output/
    ├── zImage
    ├── vexpress-v2p-ca9.dtb
    ├── u-boot
    └── initramfs.cpio.gz

## 3. Components

### Linux Kernel
- Linux kernel version 5.15
- ARM Cortex-A9 support
- Kernel configuration included

### U-Boot
- U-Boot version 2022.04
- Bootloader configuration included

### BusyBox
- BusyBox version 1.35.0
- Minimal Linux user-space utilities

### Root File System
- Initramfs-based root filesystem
- BusyBox shell
- System initialization scripts

## 4. Build Outputs

The output directory contains:

- zImage: Compressed Linux kernel image
- vexpress-v2p-ca9.dtb: Device Tree Binary
- u-boot: U-Boot bootloader binary
- initramfs.cpio.gz: Initial root filesystem archive

## 5. Notes

- Large upstream source trees and intermediate build files are excluded.
- Device nodes cannot be stored directly in Git.
- Required device nodes must be recreated when rebuilding initramfs.
- The original packaged initramfs is available in the output directory.

## 6. Conclusion

Successfully prepared the essential components of an Embedded Linux system for QEMU ARM.
