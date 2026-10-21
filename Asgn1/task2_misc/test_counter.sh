#!/bin/sh
set -eu

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

check_stats()
{
    snapshot=$(cat /proc/counter_info)
    printf '%s\n' "$snapshot"
    for expected in "$@"; do
        printf '%s\n' "$snapshot" | grep -qx "$expected" ||
            fail "Expected $expected"
    done
}

check_read()
{
    actual=$(cat /dev/counter)
    echo "read=$actual; expected=$1"
    [ "$actual" = "$1" ] || fail "Unexpected counter value"
}

echo "=== COUNTER SCRIPT START ==="

if grep -q '^counter_driver ' /proc/modules; then
    fail "counter_driver already loaded; unload it before this test"
fi

insmod "/lib/modules/$(uname -r)/counter_driver.ko"
trap 'rmmod counter_driver' EXIT

[ -c /dev/counter ] || fail "/dev/counter missing"
ls -l /dev/counter

echo "[1] Initial state"
check_stats counter=0 total_reads=0 total_writes=0

echo "[2] Three reads and EOF"
check_read 0
check_read 1
check_read 2
check_stats counter=3 total_reads=3 total_writes=0

echo "[3] Write 10"
printf '10\n' > /dev/counter
check_stats counter=10 total_reads=3 total_writes=1
check_read 10
check_stats counter=11 total_reads=4 total_writes=1

echo "[4] Reject negative and invalid input"
if printf '%s\n' '-1' > /dev/counter; then
    fail "Negative input accepted"
fi
echo "OK: negative input rejected"

if printf 'abc\n' > /dev/counter; then
    fail "Text input accepted"
fi
echo "OK: text input rejected"

check_stats counter=11 total_reads=4 total_writes=1

echo "[5] Reset to zero"
printf '0\n' > /dev/counter
check_stats counter=0 total_reads=4 total_writes=2
check_read 0
check_stats counter=1 total_reads=5 total_writes=2

echo "[6] Unload and cleanup"
rmmod counter_driver
trap - EXIT

if grep -q '^counter_driver ' /proc/modules; then
    fail "Module still loaded"
fi

[ ! -e /proc/counter_info ] || fail "procfs entry remains"
[ ! -e /sys/class/misc/counter ] || fail "sysfs entry remains"
[ ! -e /dev/counter ] || fail "Device node remains"

echo "=== COUNTER SCRIPT PASSED ==="
