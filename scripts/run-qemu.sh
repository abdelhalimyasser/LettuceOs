#!/usr/bin/env bash

set -euo pipefail

qemu_bin="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"
if ! command -v "$qemu_bin" >/dev/null 2>&1; then
	echo "qemu-system-aarch64 is not installed; ARM64 execution skipped."
	exit 0
fi

image="${ARM64_IMAGE:-build-arm64/lettuce-arm64.elf}"
if [[ ! -f "$image" ]]; then
	echo "$image is not present; run scripts/build-arm64.sh first."
	exit 0
fi

timeout_seconds="${QEMU_TIMEOUT_SECONDS:-5}"
machine="${QEMU_MACHINE:-virt}"
cpu="${QEMU_CPU:-max}"
memory="${QEMU_MEMORY:-128M}"
accel="${QEMU_ACCEL:-tcg}"

# Keep the primary ARM64 experiment host-independent: every run executes the
# same AArch64 guest on QEMU TCG. Hardware accelerators, if probed elsewhere,
# are deliberately not selected here.
qemu_args=(
	-accel "$accel"
	-M "$machine"
	-cpu "$cpu"
	-m "$memory"
	-nographic
	-monitor none
	-serial stdio
	-kernel "$image"
)

printf 'QEMU_CONFIG,accel=%s,machine=%s,cpu=%s,memory=%s,image=%s\n' \
	"$accel" "$machine" "$cpu" "$memory" "$image"

# GNU timeout is unavailable on the hosted macOS images. A small Bash watchdog
# gives both platforms the same bounded halt-loop behavior.
set +e
"$qemu_bin" "${qemu_args[@]}" &
qemu_pid=$!
(
	sleep "$timeout_seconds"
	if kill -0 "$qemu_pid" 2>/dev/null; then
		kill -TERM "$qemu_pid" 2>/dev/null
	fi
) &
watchdog_pid=$!
wait "$qemu_pid"
status=$?
kill "$watchdog_pid" 2>/dev/null
wait "$watchdog_pid" 2>/dev/null
set -e

if [[ $status -eq 0 || $status -eq 143 ]]; then
	echo "QEMU stopped after ${timeout_seconds}s (expected for the halt loop)."
	exit 0
fi
exit "$status"
