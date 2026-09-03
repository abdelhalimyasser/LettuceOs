#!/usr/bin/env bash
	
set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

mkdir -p benchmarks/results
{
	echo "# LettuceOs host benchmark run"
	echo "# results are software-prototype measurements, not ARM64 results"
	echo "--- direct_call ---"
	./build/direct_call_bench
	echo "--- same_layer ---"
	./build/same_layer_bench
	echo "--- cross_layer ---"
	./build/cross_layer_bench
	echo "--- elevator ---"
	./build/elevator_bench
	echo "--- capability_check ---"
	./build/capability_bench
	echo "--- scheduler ---"
	./build/scheduler_bench
} | tee benchmarks/results/host-latest.txt
