#!/bin/sh
set -eu
app=${1:-build/Stats-Tiger.app/Contents/MacOS/Stats-Tiger}
if [ ! -x "$app" ]; then
    printf '%s\n' 'FAIL: app executable missing' >&2
    exit 1
fi
output=$("$app" --self-check)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -E '^CPU: [0-9]+\.[0-9]%$' >/dev/null
printf '%s\n' "$output" | grep -E '^Memory: [0-9]+\.[0-9] / [0-9]+\.[0-9] GiB$' >/dev/null
if ! printf '%s\n' "$output" | grep -E '^Root disk available: [0-9]+\.[0-9] / [0-9]+\.[0-9] GiB$' >/dev/null; then
    printf '%s\n' 'FAIL: root disk capacity missing from self-check' >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -E '^Network: down [0-9]+\.[0-9] / up [0-9]+\.[0-9] KiB/s$' >/dev/null; then
    printf '%s\n' 'FAIL: network throughput missing from self-check' >&2
    exit 1
fi
if "$app" --not-an-option >/dev/null 2>&1; then
    printf '%s\n' 'FAIL: unknown option must fail' >&2
    exit 1
fi
printf '%s\n' 'PASS headless self-check and option errors'
