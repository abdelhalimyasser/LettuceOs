#!/usr/bin/env bash

set -euo pipefail

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON

cmake --build build --parallel

ctest \
    --test-dir build \
    --output-on-failure
