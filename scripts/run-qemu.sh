#!/usr/bin/env bash

set -euo pipefail

set -euo pipefail

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
	echo "qemu-system-aarch64 is not installed; ARM64 execution skipped."
	exit 0
fi

image="${ARM64_IMAGE:-build-arm64/lettuce-arm64.elf}"
if [[ ! -f "$image" ]]; then
	echo "$image is not present; run scripts/build-arm64.sh first."
	exit 0
fi

timeout_seconds="${QEMU_TIMEOUT_SECONDS:-5}"
set +e
timeout --foreground "$timeout_seconds" qemu-system-aarch64 \
	-M virt -cpu max -nographic -monitor none -serial stdio -kernel "$image"
status=$?
set -e

if [[ $status -eq 124 ]]; then
	echo "QEMU stopped after ${timeout_seconds}s (expected for the halt loop)."
	exit 0
fi
exit "$status"
