#!/bin/sh

set -e

MODULE="/lib/modules/5.15.0/lab2_driver.ko"
DEVICE="/dev/lab2"

echo "========== LAB-02: CHARACTER DRIVER TEST =========="

# Step 1: Check whether the kernel module is loaded
echo "[1] Checking kernel module..."

if grep -q '^lab2_driver ' /proc/modules; then
    echo "PASS: lab2_driver is already loaded"
else
    insmod "$MODULE"
    echo "PASS: lab2_driver loaded successfully"
fi

# Step 2: Verify kernel module
echo "[2] Checking loaded modules..."

lsmod | grep '^lab2_driver '

# Step 3: Verify character device
echo "[3] Checking device node..."

if [ ! -c "$DEVICE" ]; then
    if [ -e "$DEVICE" ]; then
        echo "FAIL: Device path exists but is not a character device"
        exit 1
    fi

    mknod "$DEVICE" c 240 0
fi

ls -l "$DEVICE"
echo "PASS: Character device exists"

# Step 4: Write test
echo "[4] Testing WRITE..."

EXPECTED="Hello from userspace, LAB-02!"

echo "$EXPECTED" > "$DEVICE"

echo "PASS: WRITE completed"

# Step 5: Read test
echo "[5] Testing READ..."

ACTUAL=$(cat "$DEVICE")

if [ "$ACTUAL" = "$EXPECTED" ]; then
    echo "PASS: READ data matches WRITE data"
else
    echo "FAIL: READ data mismatch"
    echo "Expected: [$EXPECTED]"
    echo "Actual  : [$ACTUAL]"
    exit 1
fi

# Step 6: Multiple write/read tests
echo "[6] Testing multiple WRITE/READ operations..."

for i in 1 2 3
do
    EXPECTED="Message_$i"

    echo "$EXPECTED" > "$DEVICE"

    ACTUAL=$(cat "$DEVICE")

    if [ "$ACTUAL" = "$EXPECTED" ]; then
        echo "PASS: Message_$i"
    else
        echo "FAIL: Message_$i"
        exit 1
    fi
done

# Step 7: Kernel log
echo "[7] Kernel messages..."

dmesg | grep lab2 | tail -10 || true

echo "=========================================="
echo "=== CHARACTER DRIVER TEST PASSED ==="
echo "=========================================="
