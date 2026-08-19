#!/bin/sh
# Build the catalog and verify it, printing only the summary.
#
# Piping verify_catalog.py through `tail` in a shell chain hides its exit
# status behind tail's, which lets a failing catalog look like a success.
# This keeps the trimmed output and still exits non-zero when the contract
# is violated.
set -e
cd "$(dirname "$0")/.."
python3 tool/build_catalog.py > /dev/null
out=$(python3 tool/verify_catalog.py 2>&1) || {
    printf '%s\n' "$out" >&2
    exit 1
}
printf '%s\n' "$out" | sed -n '1,2p;$p'
