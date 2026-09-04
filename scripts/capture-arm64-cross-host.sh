#!/usr/bin/env bash

# Capture one identical ARM64 QEMU-TCG guest run in the cross-host CSV schema.
set -euo pipefail

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$source_dir"

environment="${ARM64_MATRIX_ENVIRONMENT:?set ARM64_MATRIX_ENVIRONMENT}"
output_dir="${ARM64_MATRIX_OUTPUT_DIR:?set ARM64_MATRIX_OUTPUT_DIR}"
host_os="${ARM64_MATRIX_HOST_OS:-$(uname -s)}"
host_arch="${ARM64_MATRIX_HOST_ARCH:-$(uname -m)}"
host_cpu="${ARM64_MATRIX_HOST_CPU:-}"
commit_sha="${ARM64_MATRIX_COMMIT_SHA:-$(git rev-parse HEAD)}"
compiler="${AARCH64_C_COMPILER:-clang}"
qemu_bin="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"

mkdir -p "$output_dir"

if [[ -z "$host_cpu" ]]; then
	if [[ "$host_os" == "Linux" ]] && command -v lscpu >/dev/null 2>&1; then
		host_cpu="$(lscpu | awk -F: '/Model name:/ {sub(/^[[:space:]]+/, "", $2); print $2; exit}')"
	elif [[ "$host_os" == "Darwin" ]]; then
		host_cpu="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || true)"
	fi
fi

qemu_version="$($qemu_bin --version | head -n 1)"
compiler_version="$($compiler --version | head -n 1)"
linker_version="$($compiler -fuse-ld=lld -Wl,--version 2>&1 | head -n 1 || true)"
run_log="$output_dir/qemu-output.txt"
normalized_log="$output_dir/qemu-output-normalized.txt"

csv_quote() {
	local value="$1"
	value=${value//\"/\"\"}
	printf '"%s"' "$value"
}

write_row() {
	local first=1 value
	for value in "$@"; do
		if [[ $first -eq 0 ]]; then printf ','; fi
		csv_quote "$value"
		first=0
	done
	printf '\n'
}

bench_csv="$output_dir/arm64-cross-host-benchmarks.csv"
test_csv="$output_dir/arm64-cross-host-tests.csv"
environment_csv="$output_dir/arm64-cross-host-environments.csv"

printf 'environment,host_os,host_arch,host_cpu,execution_backend,qemu_version,qemu_machine,qemu_cpu,benchmark,metric,value,unit,iterations,commit_sha\n' > "$bench_csv"
printf 'environment,host_os,host_arch,host_cpu,execution_backend,qemu_version,qemu_machine,qemu_cpu,test_id,result,commit_sha\n' > "$test_csv"
printf 'environment,host_os,host_arch,host_cpu,execution_backend,qemu_version,qemu_machine,qemu_cpu,qemu_memory,qemu_command,compiler,compiler_version,linker,build_flags,commit_sha\n' > "$environment_csv"

bash scripts/build-arm64.sh
QEMU_SYSTEM_AARCH64="$qemu_bin" QEMU_ACCEL=tcg QEMU_MACHINE=virt QEMU_CPU=max QEMU_MEMORY=128M \
	bash scripts/run-qemu.sh | tee "$run_log"
tr -d '\r' < "$run_log" > "$normalized_log"

if ! grep -q 'All 25 ARM64 Execution/Runtime Foundation Tests Passed!' "$run_log"; then
	echo "Missing canonical ARM64 25-test success marker." >&2
	exit 1
fi
if ! grep -q 'EXECUTION RUNTIME FOUNDATION PASS' "$run_log"; then
	echo "Missing ARM64 execution-runtime success marker." >&2
	exit 1
fi

while IFS=, read -r record case p50 p95 p99 mean minimum maximum; do
	[[ "$record" == "BENCH" ]] || continue
	for metric_value in "p50:$p50" "p95:$p95" "p99:$p99" "mean:$mean" "min:$minimum" "max:$maximum"; do
		metric="${metric_value%%:*}"
		value="${metric_value#*:}"
		write_row "$environment" "$host_os" "$host_arch" "$host_cpu" "tcg" "$qemu_version" "virt" "max" "$case" "$metric" "$value" "generic-counter-ticks" "" "$commit_sha" >> "$bench_csv"
	done
done < "$normalized_log"

while IFS=, read -r record test_id result; do
	[[ "$record" == "TEST" ]] || continue
	write_row "$environment" "$host_os" "$host_arch" "$host_cpu" "tcg" "$qemu_version" "virt" "max" "$test_id" "$result" "$commit_sha" >> "$test_csv"
done < "$normalized_log"

test_count="$(tail -n +2 "$test_csv" | wc -l | tr -d ' ')"
pass_count="$(awk -F, 'NR > 1 && $10 == "\"PASS\"" { count++ } END { print count + 0 }' "$test_csv")"
if [[ "$test_count" != "25" || "$pass_count" != "25" ]]; then
	echo "Expected 25 passing ARM64 tests; captured ${pass_count}/${test_count}." >&2
	exit 1
fi

qemu_command="$qemu_bin -accel tcg -M virt -cpu max -m 128M -nographic -monitor none -serial stdio -kernel build-arm64/lettuce-arm64.elf"
write_row "$environment" "$host_os" "$host_arch" "$host_cpu" "tcg" "$qemu_version" "virt" "max" "128M" "$qemu_command" "$compiler" "$compiler_version" "$linker_version" "-ffreestanding -fno-stack-protector -fno-pic -fno-asynchronous-unwind-tables -mgeneral-regs-only -O2 -Wall -Wextra -Werror --target=aarch64-none-elf -fuse-ld=lld -nostdlib" "$commit_sha" >> "$environment_csv"

echo "Captured ARM64 QEMU-TCG cross-host evidence in $output_dir"
