# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)

project(research LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/kernel/include)

add_library(lettuce_kernel STATIC
    kernel/main/capability.c
    kernel/main/context.c
    kernel/main/dispatch.c
    kernel/main/kernel.c
    kernel/main/protection.c
    ipc/same_layer/gate.c
    ipc/same_layer/validate.c
    ipc/cross_layer/call.c
    ipc/cross_layer/message.c
    ipc/elevator/elevator.c
    ipc/elevator/policy.c
    memory/allocator/fixed.c
    memory/shared/buffer.c
    runtime/c/same_layer_call.c
    runtime/c/cross_layer_call.c
    runtime/c/elevator_call.c
)

target_include_directories(lettuce_kernel
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/kernel/include
)

add_executable(capability_unit tests/unit/capability_unit.c)
target_link_libraries(capability_unit PRIVATE lettuce_kernel)

add_executable(same_layer_unit tests/unit/same_layer_unit.c)
target_link_libraries(same_layer_unit PRIVATE lettuce_kernel)

add_executable(cross_layer_unit tests/unit/cross_layer_unit.c)
target_link_libraries(cross_layer_unit PRIVATE lettuce_kernel)

add_executable(elevator_unit tests/unit/elevator_unit.c)
target_link_libraries(elevator_unit PRIVATE lettuce_kernel)

add_executable(memory_unit tests/unit/memory_unit.c)
target_link_libraries(memory_unit PRIVATE lettuce_kernel)

add_executable(capability_security tests/security/capability_security.c)
target_link_libraries(capability_security PRIVATE lettuce_kernel)

add_executable(direct_call_bench benchmarks/direct_call/direct_call_bench.c)
add_executable(same_layer_bench benchmarks/same_layer/same_layer_bench.c)
target_link_libraries(same_layer_bench PRIVATE lettuce_kernel)
