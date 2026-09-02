#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

set -euo pipefail

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
	echo "qemu-system-aarch64 is not installed; ARM64 execution skipped."
	exit 0
fi

if [[ ! -f build/lettuce-kernel.elf ]]; then
	echo "build/lettuce-kernel.elf is not present; build an ARM64 image first."
	exit 0
fi

exec qemu-system-aarch64 -M virt -cpu max -nographic -kernel build/lettuce-kernel.elf
