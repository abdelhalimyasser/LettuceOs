#!/usr/bin/env bash

set -euo pipefail

compiler="${AARCH64_C_COMPILER:-}"
if [[ -z "$compiler" ]]; then
    if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        compiler=aarch64-linux-gnu-gcc
    elif command -v aarch64-none-elf-gcc >/dev/null 2>&1; then
        compiler=aarch64-none-elf-gcc
    elif command -v clang >/dev/null 2>&1; then
        compiler=clang
    fi
fi

if [[ -z "$compiler" ]]; then
    echo "No AArch64 cross compiler found; set AARCH64_C_COMPILER to build ARM64."
    exit 0
fi

build_dir="${ARM64_BUILD_DIR:-build-arm64}"
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$build_dir"

common_flags=(-ffreestanding -fno-stack-protector -fno-pic -fno-asynchronous-unwind-tables -mgeneral-regs-only -O2 -Wall -Wextra -Werror)
objects=()

if [[ "$(basename "$compiler")" == clang* ]]; then
    target_flags=(--target=aarch64-none-elf)
    link_flags=(-fuse-ld=lld)
else
    target_flags=()
    link_flags=()
fi

for source in \
    kernel/arch/arm64/boot.S \
    kernel/arch/arm64/entry.S \
    kernel/arch/arm64/exception.S \
    kernel/arch/arm64/pac.S \
    kernel/arch/arm64/irq.S \
    kernel/arch/arm64/context_switch.S \
    kernel/arch/arm64/elevator.S \
    kernel/arch/arm64/cpu.c \
    kernel/arch/arm64/timer.c \
    kernel/arch/arm64/gic.c \
    kernel/arch/arm64/mmu.c \
    kernel/arch/arm64/mte.c \
    kernel/arch/arm64/poe.c \
    kernel/main/kernel.c \
    kernel/main/context.c \
    kernel/main/protection.c \
    kernel/main/capability.c \
    kernel/main/dispatch.c \
    kernel/main/task.c \
    kernel/scheduler/scheduler.c \
    kernel/scheduler/rr.c \
    kernel/scheduler/eevdf.c \
    ipc/same_layer/validate.c \
    ipc/same_layer/gate.c \
    ipc/cross_layer/call.c \
    ipc/elevator/policy.c \
    ipc/elevator/elevator.c \
    runtime/posix/src/fd.c \
    runtime/posix/src/sys.c \
    runtime/posix/src/posix.c \
    kernel/arch/arm64/main.c; do
    object="$build_dir/$(basename "$source").o"
    "$compiler" "${target_flags[@]}" "${common_flags[@]}" -I"$source_dir/include" -I"$source_dir/kernel/include" -I"$source_dir/kernel/scheduler" -I"$source_dir/runtime/posix/include" -c "$source_dir/$source" -o "$object"
    objects+=("$object")
done

"$compiler" "${target_flags[@]}" "${link_flags[@]}" -nostdlib -Wl,-T,"$source_dir/kernel/arch/arm64/linker.ld" \
    -Wl,-Map,"$build_dir/lettuce-arm64.map" "${objects[@]}" -o "$build_dir/lettuce-arm64.elf"

if command -v llvm-objcopy >/dev/null 2>&1; then
    llvm-objcopy -O binary "$build_dir/lettuce-arm64.elf" "$build_dir/lettuce-arm64.bin"
fi

echo "ARM64 image: $build_dir/lettuce-arm64.elf"