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
compiler="${ARM64_MATRIX_BUILD_COMPILER:-${AARCH64_C_COMPILER:-clang}}"
qemu_bin="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"
run_timeout="${QEMU_TIMEOUT_SECONDS:-60}"

mkdir -p "$output_dir"

# 1. Collect Host Metadata
host_meta="$output_dir/host-metadata.txt"
{
	echo "host_os=$host_os"
	echo "host_arch=$host_arch"
	echo "uname=$(uname -a)"
	if [[ "$host_os" == "Linux" ]]; then
		command -v lscpu >/dev/null 2>&1 && lscpu || true
		[[ -f /etc/os-release ]] && cat /etc/os-release || true
	elif [[ "$host_os" == "Darwin" || "$host_os" == "macOS" ]]; then
		command -v sw_vers >/dev/null 2>&1 && sw_vers || true
		sysctl -n hw.machine 2>/dev/null && echo "hw.machine=$(sysctl -n hw.machine)" || true
		sysctl -n hw.model 2>/dev/null && echo "hw.model=$(sysctl -n hw.model)" || true
		sysctl -n hw.ncpu 2>/dev/null && echo "hw.ncpu=$(sysctl -n hw.ncpu)" || true
		sysctl -n machdep.cpu.brand_string 2>/dev/null && echo "machdep.cpu.brand_string=$(sysctl -n machdep.cpu.brand_string)" || true
	fi
} > "$host_meta" 2>&1 || true

if [[ -z "$host_cpu" ]]; then
	if [[ "$host_os" == "Linux" ]] && command -v lscpu >/dev/null 2>&1; then
		host_cpu="$(lscpu | awk -F: '/Model name:/ {sub(/^[[:space:]]+/, "", $2); print $2; exit}')"
	elif [[ "$host_os" == "Darwin" || "$host_os" == "macOS" ]]; then
		host_cpu="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || true)"
		if [[ -z "$host_cpu" ]]; then
			host_cpu="$(sysctl -n hw.model 2>/dev/null || true)"
		fi
	fi
fi

# 2. Record QEMU Version
"$qemu_bin" --version > "$output_dir/qemu-version.txt" 2>&1 || true
qemu_version="$($qemu_bin --version | head -n 1)"
compiler_version="${ARM64_MATRIX_BUILD_COMPILER_VERSION:-$($compiler --version 2>/dev/null | head -n 1 || echo "")}"
linker_version="${ARM64_MATRIX_BUILD_LINKER_VERSION:-$($compiler -fuse-ld=lld -Wl,--version 2>&1 | head -n 1 || echo "")}"
run_log="$output_dir/qemu-output.txt"
normalized_log="$output_dir/qemu-output-normalized.txt"

# 3. Build Guest ELF if not skipped
if [[ "${ARM64_MATRIX_SKIP_BUILD:-0}" != "1" ]]; then
	bash scripts/build-arm64.sh
fi
if [[ ! -f build-arm64/lettuce-arm64.elf ]]; then
	echo "ERROR: Missing shared ARM64 ELF: build-arm64/lettuce-arm64.elf" >&2
	exit 1
fi
guest_elf_sha256="$(sha256sum build-arm64/lettuce-arm64.elf | awk '{print $1}')"
echo "$guest_elf_sha256" > "$output_dir/guest-elf-sha256.txt"

# 4. Execute Guest with Early Termination on Completion Markers
qemu_args=(
	-accel tcg
	-M virt
	-cpu max
	-m 128M
	-nographic
	-monitor none
	-serial stdio
	-kernel build-arm64/lettuce-arm64.elf
)

printf 'QEMU_CONFIG,accel=tcg,machine=virt,cpu=max,memory=128M,image=build-arm64/lettuce-arm64.elf\n' > "$run_log"

set +e
"$qemu_bin" "${qemu_args[@]}" >> "$run_log" 2>&1 &
qemu_pid=$!

# Background watchdog/completion monitor
(
	start_time=$(date +%s)
	while kill -0 "$qemu_pid" 2>/dev/null; do
		if grep -q -E "EXECUTION RUNTIME FOUNDATION PASS|ARM64_EXECUTION_RUNTIME_FOUNDATION_FAIL" "$run_log" 2>/dev/null; then
			sleep 0.5
			kill -TERM "$qemu_pid" 2>/dev/null || true
			break
		fi
		now=$(date +%s)
		elapsed=$((now - start_time))
		if [[ $elapsed -ge $run_timeout ]]; then
			kill -TERM "$qemu_pid" 2>/dev/null || true
			break
		fi
		sleep 0.2
	done
) &
monitor_pid=$!

wait "$qemu_pid" 2>/dev/null || true
kill "$monitor_pid" 2>/dev/null || true
wait "$monitor_pid" 2>/dev/null || true
set -e

tr -d '\r' < "$run_log" > "$normalized_log"

# 5. Parse Logs and Validate Test Suite via Python
python3 - <<PYEOF
import csv
import sys

normalized_log = "${normalized_log}"
bench_csv_path = "${output_dir}/arm64-cross-host-benchmarks.csv"
test_csv_path = "${output_dir}/arm64-cross-host-tests.csv"
env_csv_path = "${output_dir}/arm64-cross-host-environments.csv"

env = """${environment}"""
host_os = """${host_os}"""
host_arch = """${host_arch}"""
host_cpu = """${host_cpu}"""
qemu_version = """${qemu_version}"""
compiler = """${compiler}"""
compiler_version = """${compiler_version}"""
linker_version = """${linker_version}"""
commit_sha = """${commit_sha}"""
guest_elf_sha256 = """${guest_elf_sha256}"""
qemu_command = "${qemu_bin} -accel tcg -M virt -cpu max -m 128M -nographic -monitor none -serial stdio -kernel build-arm64/lettuce-arm64.elf"
build_flags = "-ffreestanding -fno-stack-protector -fno-pic -fno-asynchronous-unwind-tables -mgeneral-regs-only -O2 -Wall -Wextra -Werror --target=aarch64-none-elf -fuse-ld=lld -nostdlib"

with open(normalized_log, "r", encoding="utf-8", errors="replace") as f:
    log_lines = f.readlines()

tests = []
seen_test_ids = set()
duplicates = []
benchmarks = []
has_pass_marker = False
has_all_25_marker = False
has_fail_summary_marker = False

for raw_line in log_lines:
    line = raw_line.strip()
    if "EXECUTION RUNTIME FOUNDATION PASS" in line:
        has_pass_marker = True
    if "All 25 ARM64 Execution/Runtime Foundation Tests Passed!" in line:
        has_all_25_marker = True
    if "ARM64_EXECUTION_RUNTIME_FOUNDATION_FAIL" in line:
        has_fail_summary_marker = True

    if line.startswith("TEST,"):
        parts = line.split(",")
        if len(parts) >= 3:
            tid = parts[1].strip()
            res = parts[2].strip()
            if tid in seen_test_ids:
                duplicates.append(tid)
            seen_test_ids.add(tid)
            tests.append((tid, res))

    elif line.startswith("BENCH,"):
        parts = line.split(",")
        if len(parts) >= 8:
            # BENCH,case,p50,p95,p99,mean,min,max
            benchmarks.append({
                "case": parts[1].strip(),
                "p50": parts[2].strip(),
                "p95": parts[3].strip(),
                "p99": parts[4].strip(),
                "mean": parts[5].strip(),
                "min": parts[6].strip(),
                "max": parts[7].strip()
            })

# Write benchmarks CSV
bench_headers = [
    "environment", "host_os", "host_arch", "host_cpu", "execution_backend",
    "qemu_version", "qemu_machine", "qemu_cpu", "benchmark", "metric",
    "value", "unit", "iterations", "commit_sha"
]
with open(bench_csv_path, "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(bench_headers)
    for b in benchmarks:
        for metric in ["p50", "p95", "p99", "mean", "min", "max"]:
            writer.writerow([
                env, host_os, host_arch, host_cpu, "tcg",
                qemu_version, "virt", "max", b["case"], metric,
                b[metric], "generic-counter-ticks", "", commit_sha
            ])

# Write tests CSV
test_headers = [
    "environment", "host_os", "host_arch", "host_cpu", "execution_backend",
    "qemu_version", "qemu_machine", "qemu_cpu", "test_id", "result", "commit_sha"
]
with open(test_csv_path, "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(test_headers)
    for tid, res in tests:
        writer.writerow([
            env, host_os, host_arch, host_cpu, "tcg",
            qemu_version, "virt", "max", tid, res, commit_sha
        ])

# Write environments CSV
env_headers = [
    "environment", "host_os", "host_arch", "host_cpu", "execution_backend",
    "qemu_version", "qemu_machine", "qemu_cpu", "qemu_memory", "qemu_command",
    "compiler", "compiler_version", "linker", "build_flags", "commit_sha", "guest_elf_sha256"
]
with open(env_csv_path, "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(env_headers)
    writer.writerow([
        env, host_os, host_arch, host_cpu, "tcg",
        qemu_version, "virt", "max", "128M", qemu_command,
        compiler, compiler_version, linker_version, build_flags, commit_sha, guest_elf_sha256
    ])

# Strict Validation
expected_test_ids = [str(i) for i in range(1, 26)]
actual_test_dict = {tid: res for tid, res in tests}
missing_ids = [tid for tid in expected_test_ids if tid not in actual_test_dict]
failed_ids = [tid for tid, res in actual_test_dict.items() if res != "PASS"]
pass_count = sum(1 for _, res in tests if res == "PASS")
fail_count = sum(1 for _, res in tests if res != "PASS")
total_count = len(tests)

errors = []
if duplicates:
    errors.append(f"Duplicate test records encountered: {duplicates}")
if missing_ids:
    errors.append(f"Missing expected test numbers: {missing_ids}")
if failed_ids:
    errors.append(f"Failed tests detected: {failed_ids}")
if total_count != 25:
    errors.append(f"Expected exactly 25 test records, but found {total_count}")
if not has_pass_marker or not has_all_25_marker:
    errors.append("Missing canonical 25-test execution pass marker in log")
if has_fail_summary_marker:
    errors.append("Log contains ARM64_EXECUTION_RUNTIME_FOUNDATION_FAIL marker")

if errors:
    sys.stderr.write(f"\n============================================================\n")
    sys.stderr.write(f"ERROR: ARM64 Test Suite Validation Failed on {env}\n")
    sys.stderr.write(f"Summary: {total_count} total, {pass_count} PASS, {fail_count} FAIL\n")
    for err in errors:
        sys.stderr.write(f"  - {err}\n")
    sys.stderr.write(f"============================================================\n\n")
    sys.exit(1)

print(f"[✓] Test suite verified on {env}: 25/25 PASS (all unique tests 1..25 passed)")
print(f"Captured ARM64 QEMU-TCG cross-host evidence in ${output_dir}")
PYEOF
