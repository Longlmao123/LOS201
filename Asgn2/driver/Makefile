# Pham Tran Gia Long - SE201095: build against the ARM target kernel.
ifneq ($(KERNELRELEASE),)
obj-m += sms_sensor_driver.o
else
KERNEL_DIR ?= $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-

.PHONY: all arm clean ioctl-test
all: arm
arm:
	$(MAKE) -C "$(KERNEL_DIR)" M="$(CURDIR)" ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

ioctl-test:
	$(CROSS_COMPILE)gcc -std=c11 -O2 -g -Wall -Wextra -static sms_ioctl_test.c -o sms_ioctl_test

clean:
	$(MAKE) -C "$(KERNEL_DIR)" M="$(CURDIR)" ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
endif
