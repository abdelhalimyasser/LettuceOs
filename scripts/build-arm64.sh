#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

compiler="${AARCH64_C_COMPILER:-}"
if [[ -z "$compiler" ]]; then
    if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        compiler=aarch64-linux-gnu-gcc
    elif command -v aarch64-none-elf-gcc >/dev/null 2>&1; then
        compiler=aarch64-none-elf-gcc
    fi
fi

if [[ -z "$compiler" ]]; then
    echo "No AArch64 cross compiler found; set AARCH64_C_COMPILER to build ARM64."
    exit 0
fi

cmake -S . -B build-arm64 -DCMAKE_C_COMPILER="$compiler" -DCMAKE_SYSTEM_NAME=Linux
cmake --build build-arm64