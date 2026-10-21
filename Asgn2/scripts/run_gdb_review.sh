#!/bin/bash
# Pham Tran Gia Long - SE201095: capture the real GDB session on Ubuntu.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
make -C "$project_dir/app" debug
debugger="${GDB:-gdb-multiarch}"
command -v "$debugger" >/dev/null
mkdir -p "$project_dir/logs/final_review"
"$debugger" -q -batch -x "$project_dir/scripts/gdb_review.commands" \
    --args "$project_dir/app/sms_app_debug" --mock --other -i 500 -d 3 \
    --log "$project_dir/logs/final_review/gdb_sms.log" \
    > "$project_dir/logs/final_review/gdb_session.txt" 2>&1
cat "$project_dir/logs/final_review/gdb_session.txt"
echo "=== GDB CAPTURE FINISHED; REVIEW THE SESSION OUTPUT ==="
