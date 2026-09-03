cmake_minimum_required(VERSION 3.16)

project(research LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -UNDEBUG")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(LETTUCE_BUILD_TYPE_RELEASE=1)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/runtime/posix/include
)

add_library(lettuce_kernel STATIC
    kernel/main/capability.c
    kernel/main/context.c
    kernel/main/dispatch.c
    kernel/main/kernel.c
    kernel/main/protection.c
    kernel/main/task.c
    kernel/scheduler/scheduler.c
    kernel/scheduler/rr.c
    kernel/scheduler/eevdf.c
    kernel/main/host_arch_stubs.c
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
    runtime/posix/src/fd.c
    runtime/posix/src/sys.c
    runtime/posix/src/posix.c
)

target_include_directories(lettuce_kernel
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/kernel/include
        ${CMAKE_SOURCE_DIR}/kernel/scheduler
        ${CMAKE_SOURCE_DIR}/runtime/posix/include
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

add_executable(context_nested_unit tests/unit/context_nested_unit.c)
target_link_libraries(context_nested_unit PRIVATE lettuce_kernel)

add_executable(capability_security tests/security/capability_security.c)
target_link_libraries(capability_security PRIVATE lettuce_kernel)

add_executable(dynamic_array_unit tests/unit/dynamic_array_unit.c shared/dynamic_array/dynamic_array.c)

add_executable(task_scheduler_unit tests/unit/task_scheduler_unit.c)
target_link_libraries(task_scheduler_unit PRIVATE lettuce_kernel)

add_executable(sched_eevdf_unit tests/unit/sched_eevdf_unit.c)
target_link_libraries(sched_eevdf_unit PRIVATE lettuce_kernel)

add_executable(posix_unit tests/unit/posix_unit.c)
target_link_libraries(posix_unit PRIVATE lettuce_kernel)

add_executable(direct_call_bench benchmarks/direct_call/direct_call_bench.c)
target_link_libraries(direct_call_bench PRIVATE m)
add_executable(same_layer_bench benchmarks/same_layer/same_layer_bench.c)
target_link_libraries(same_layer_bench PRIVATE lettuce_kernel m)

add_executable(cross_layer_bench benchmarks/cross_layer/cross_layer_bench.c)
target_link_libraries(cross_layer_bench PRIVATE lettuce_kernel m)

add_executable(elevator_bench benchmarks/elevator/elevator_bench.c)
target_link_libraries(elevator_bench PRIVATE lettuce_kernel m)

add_executable(capability_bench benchmarks/ipc/capability_bench.c)
target_link_libraries(capability_bench PRIVATE lettuce_kernel m)

add_executable(scheduler_bench benchmarks/scheduler/scheduler_bench.c)
target_link_libraries(scheduler_bench PRIVATE lettuce_kernel m)
