#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

cmake -S . -B build
cmake --build build
./build/capability_unit
./build/capability_security
./build/same_layer_unit
./build/cross_layer_unit
./build/elevator_unit
./build/memory_unit
