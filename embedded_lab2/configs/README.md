# Lab 02 configuration notes

Lab 02 reuses the Linux 5.15 ARMv7 kernel and QEMU board setup from Lab 01. The submitted Lab 02 archive did **not** contain separate kernel, BusyBox or U-Boot `.config` files.

See [`../../embedded_lab1/configs/`](../../embedded_lab1/configs/) for the preserved Lab 01 configurations, and consult the main [Lab 02 README](../README.md) for module build instructions.

The loadable module is built from [`../driver/lab2_driver.c`](../driver/lab2_driver.c) and [`../driver/Makefile`](../driver/Makefile).
