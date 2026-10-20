#!/bin/sh

set -e

MODULE="/lib/modules/5.15.0/lab2_driver.ko"
DEVICE="/dev/lab2"
PROC="/proc/lab2_info"
SYSFS="/sys/class/lab2_class/lab2"

echo "========== LAB-02: PROCFS & SYSFS TEST =========="

# Step 1: Check kernel module
echo "[1] Checking kernel module..."

if grep -q '^lab2_driver ' /proc/modules; then
    echo "PASS: lab2_driver is already loaded"
else
    insmod "$MODULE"
    echo "PASS: lab2_driver loaded"
fi

# Step 2: Check device
echo "[2] Checking character device..."

if [ ! -c "$DEVICE" ]; then
    if [ -e "$DEVICE" ]; then
        echo "FAIL: Invalid device node"
        exit 1
    fi

    mknod "$DEVICE" c 240 0
fi

echo "PASS: Device exists"

# Step 3: Check procfs
echo "[3] Checking procfs..."

if [ ! -f "$PROC" ]; then
    echo "FAIL: /proc/lab2_info not found"
    exit 1
fi

cat "$PROC"

# Step 4: Write data
echo "[4] Writing test data..."

EXPECTED="Embedded Linux Lab2 Test Data"

echo "$EXPECTED" > "$DEVICE"

# Step 5: Verify procfs after write
echo "[5] Checking procfs after WRITE..."

cat "$PROC"

if grep -Fq "$EXPECTED" "$PROC"; then
    echo "PASS: procfs contains expected data"
else
    echo "FAIL: procfs data mismatch"
    exit 1
fi

# Step 6: Check sysfs
echo "[6] Checking sysfs attributes..."

for ATTR in buffer_len open_count last_data
do
    if [ ! -f "$SYSFS/$ATTR" ]; then
        echo "FAIL: Missing sysfs attribute: $ATTR"
        exit 1
    fi
done

BUFFER_LEN=$(cat "$SYSFS/buffer_len")
OPEN_COUNT=$(cat "$SYSFS/open_count")
LAST_DATA=$(cat "$SYSFS/last_data")

echo "buffer_len : $BUFFER_LEN"
echo "open_count : $OPEN_COUNT"
echo "last_data  : $LAST_DATA"

# Step 7: Verify sysfs data
echo "[7] Verifying sysfs..."

EXPECTED_LEN=$(printf '%s\n' "$EXPECTED" | wc -c)
EXPECTED_LEN=$(echo "$EXPECTED_LEN" | tr -d ' ')

if [ "$LAST_DATA" != "$EXPECTED" ]; then
    echo "FAIL: sysfs last_data mismatch"
    exit 1
fi

if [ "$BUFFER_LEN" != "$EXPECTED_LEN" ]; then
    echo "FAIL: sysfs buffer_len mismatch"
    exit 1
fi

case "$OPEN_COUNT" in
    ''|*[!0-9]*)
        echo "FAIL: Invalid open_count"
        exit 1
        ;;
esac

echo "PASS: sysfs data verified"

echo "=========================================="
echo "=== PROCFS & SYSFS TEST PASSED ==="
echo "=========================================="
