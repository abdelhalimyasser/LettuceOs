#!/usr/bin/env bash

set -euo pipefail

cmake -S . -B build
cmake --build build
./build/capability_unit
./build/capability_security
./build/same_layer_unit
./build/cross_layer_unit
./build/elevator_unit
./build/memory_unit
./build/context_nested_unit
./build/dynamic_array_unit
./build/task_scheduler_unit
./build/posix_unit
