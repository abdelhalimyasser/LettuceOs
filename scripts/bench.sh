#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

cmake -S . -B build
cmake --build build
./build/direct_call_bench
./build/same_layer_bench
./build/cross_layer_bench
./build/elevator_bench
./build/capability_bench
