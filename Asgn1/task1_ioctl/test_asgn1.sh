#!/bin/sh
set -eu

# Bao loi va dung kiem thu.
fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

# Tham so: so anh va noi dung can chup.
photo()
{
    echo
    echo "[CHUP ANH $1] $2"
    printf 'Chup xong thi nhan Enter: '
    read -r answer
}

# Lay thong ke va doi chieu cac dong mong doi.
check_stats()
{
    STATS=$(asgn1_ctl stats)
    printf '%s\n' "$STATS"
    for EXPECTED in "$@"; do
        printf '%s\n' "$STATS" | grep -qx "$EXPECTED" ||
            fail "Expected $EXPECTED"
    done
}

echo '=== ASGN1 TEST START ==='

if grep -q '^asgn1_driver ' /proc/modules; then
    fail 'Module already loaded'
fi

echo '[1] Load module'
insmod "/lib/modules/$(uname -r)/asgn1_driver.ko"
trap 'rmmod asgn1_driver 2>/dev/null || true' EXIT

[ -c /dev/asgn1 ] || mknod /dev/asgn1 c 241 0
lsmod | grep asgn1_driver
ls -l /dev/asgn1

echo '[2] GET_VERSION'
VERSION=$(asgn1_ctl version)
echo "$VERSION"
[ "$VERSION" = 'version=asgn1_driver v1.0 - SE201095' ] ||
    fail 'Version mismatch'

echo '[3] Default echo mode'
printf 'ABC' > /dev/asgn1
DATA=$(cat /dev/asgn1)
printf 'Expected: [ABC]\nActual:   [%s]\n' "$DATA"
[ "$DATA" = 'ABC' ] || fail 'Default echo mismatch'

check_stats write_count=1 read_count=1 \
    buffer_len=3 last_write_size=3
photo 2 'Load module, version, default echo and stats'

echo '[4] Kernel mode'
asgn1_ctl mode 0
printf 'XYZ' > /dev/asgn1
DATA=$(cat /dev/asgn1)
printf 'Expected: [[KERNEL]XYZ]\nActual:   [%s]\n' "$DATA"
[ "$DATA" = '[KERNEL]XYZ' ] || fail 'Kernel mode mismatch'

check_stats write_count=2 read_count=2 \
    buffer_len=11 last_write_size=3
photo 3 'Kernel mode and statistics'

echo '[5] Echo mode'
asgn1_ctl mode 1
printf 'HELLO' > /dev/asgn1
DATA=$(cat /dev/asgn1)
printf 'Expected: [HELLO]\nActual:   [%s]\n' "$DATA"
[ "$DATA" = 'HELLO' ] || fail 'Echo mode mismatch'

check_stats write_count=3 read_count=3 \
    buffer_len=5 last_write_size=5
cat /proc/asgn1_info
photo 4 'Echo mode and procfs statistics'

echo '[6] RESET_BUFFER'
asgn1_ctl reset
DATA=$(cat /dev/asgn1)
printf 'Read after reset: [%s]\n' "$DATA"
[ -z "$DATA" ] || fail 'Buffer not empty'

check_stats write_count=0 read_count=0 \
    buffer_len=0 last_write_size=0
cat /proc/asgn1_info
photo 5 'Empty buffer and zero read/write counters'

echo '[7] Unload module'
rmmod asgn1_driver
trap - EXIT

if grep -q '^asgn1_driver ' /proc/modules; then
    fail 'Module still loaded'
fi

[ ! -e /proc/asgn1_info ] || fail 'procfs entry remains'
[ ! -d /sys/class/asgn1_class ] || fail 'sysfs class remains'

echo 'Module removed; procfs and sysfs cleaned up'
echo '[8] Kernel log'
dmesg | grep asgn1_driver

echo '=== ASGN1 TEST PASSED ==='
photo 6 'Unload, cleanup, dmesg and final result'
