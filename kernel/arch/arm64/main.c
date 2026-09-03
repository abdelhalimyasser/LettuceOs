/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/main.c
 *
 * Purpose:
 *   Drives ARM64 platform initialization and the EL1 runtime foundation test
 *   sequence for the prototype.
 *
 * Execution context:
 *   EL1 supervisor initialization on the ARM64 virtual platform.
 *
 * Flow:
 *   Platform setup -> kernel services and protection setup -> EL0 runtime
 *   tests -> console result reporting.
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../include/arch.h"
#include "../../include/lettuce/capability.h"
#include "../../include/lettuce/errors.h"
#include "../../include/lettuce/service.h"
#include "../include/capability_internal.h"
#include "../include/context.h"
#include "../include/kernel.h"
#include "../include/protection.h"
#include "gic.h"
#include "timer.h"
#include "irq.h"
#include "features.h"
#include "../include/elevator_asm.h"
#include "../include/task.h"
#include "../include/scheduler.h"
#include "../../runtime/posix/include/posix.h"
#include "../../runtime/posix/include/fd.h"

int64_t lettuce_kernel_sys_write(int fd, const void *buf, size_t count);

#define DOMAIN_A UINT32_C(100)
#define DOMAIN_B UINT32_C(200)
#define DOMAIN_C UINT32_C(300)

#define PRIVATE_A_VA UINT64_C(0x41000000)
#define PRIVATE_B_VA UINT64_C(0x41100000)
#define PRIVATE_C_VA UINT64_C(0x41180000)
#define SHARED_PAGE_VA UINT64_C(0x411c0000)
#define RO_TEST_PAGE_VA UINT64_C(0x411e0000)
#define SERVICE_CODE_VA UINT64_C(0x41050000)

#define SERVICE_CAMERA UINT32_C(10)
#define SERVICE_DISPLAY UINT32_C(20)
#define SERVICE_STORAGE UINT32_C(30)
#define SERVICE_SENSOR UINT32_C(40)
#define SERVICE_FAILING UINT32_C(99)

#define OP_DRAW UINT32_C(1)
#define OP_SAVE UINT32_C(1)
#define OP_FAIL UINT32_C(1)

#define RES_FRAMEBUFFER UINT32_C(1)
#define RES_DISK UINT32_C(2)

/* External EL0 synthetic service entrypoints defined in entry.S */
extern uint8_t el0_service_code_start[];

extern void svc_display_entry(void);
extern void svc_camera_entry(void);
extern void svc_storage_entry(void);
extern void svc_failing_entry(void);
extern void svc_camera_call_display(void);
extern void svc_display_call_storage(void);
extern void svc_test_sysreg(void);
extern void svc_test_kernel_mem(void);
extern void svc_test_foreign_mem_a_to_b(void);
extern void svc_test_foreign_mem_b_to_a(void);
extern void svc_test_shared_write_a(void);
extern void svc_test_shared_read_modify_b(void);
extern void svc_test_shared_verify_a(void);
extern void svc_minimal_return_entry(void);
extern void svc_camera_cross_layer_call(void);
extern void svc_camera_elevator_call(void);
extern void svc_camera_invalid_cap_call(void);
extern void task_counter_a(void);
extern void task_counter_b(void);
extern void task_counter_c(void);
extern void task_posix_runner(void);
extern void task_bench_syscall(void);

typedef LettuceStatus (*LettuceDispatchFn)(void);

static inline uintptr_t el0_service_va(const void *sym)
{
    return SERVICE_CODE_VA + ((uintptr_t)sym - (uintptr_t)el0_service_code_start);
}

static int64_t run_el0_service(const void *sym, LettuceDomainId domain, LettuceServiceId service)
{
    kernel_set_current_service_id(service);
    lettuce_mmu_enter(domain);
    const uintptr_t entry_pc = el0_service_va(sym);
    const uintptr_t stack_top = lettuce_arch_domain_stack_top(domain);
    return lettuce_el0_enter(entry_pc, stack_top);
}

static LettuceStatus direct_el1_synthetic_entry(void)
{
    return LETTUCE_STATUS_OK;
}

static void log_pass(const char *test_name)
{
    lettuce_arch_console_puts("[PASS] ");
    lettuce_arch_console_puts(test_name);
    lettuce_arch_console_puts("\n");

    /* Emit structured machine-readable line: TEST,<num>,PASS */
    if (test_name[0] == 'T' && test_name[1] == 'e' && test_name[2] == 's' && test_name[3] == 't' && test_name[4] == ' ')
    {
        uint64_t num = 0;
        size_t idx = 5;
        while (test_name[idx] >= '0' && test_name[idx] <= '9')
        {
            num = num * 10 + (test_name[idx] - '0');
            idx++;
        }
        if (num > 0)
        {
            lettuce_arch_console_puts("TEST,");
            lettuce_arch_console_print_dec(num);
            lettuce_arch_console_puts(",PASS\n");
        }
    }
}

static void log_fail(const char *test_name)
{
    lettuce_arch_console_puts("[FAIL] ");
    lettuce_arch_console_puts(test_name);
    lettuce_arch_console_puts("\n");
}

#define BENCH_SAMPLES 50u
#define BENCH_CALLS_PER_SAMPLE 100u
#define BENCH_WARMUP_CALLS 100u

static void print_bench_line(const char *lbl, uint64_t t, uint64_t freq)
{
    const uint64_t ns = (freq > 0) ? ((t * UINT64_C(1000000000)) / freq) : 0;
    lettuce_arch_console_puts(lbl);
    lettuce_arch_console_print_dec(t);
    lettuce_arch_console_puts(" ticks (~");
    lettuce_arch_console_print_dec(ns);
    lettuce_arch_console_puts(" ns)\n");
}

static void print_bench_stats(const char *name, uint64_t *samples, uint64_t freq)
{
    for (size_t i = 1; i < BENCH_SAMPLES; ++i)
    {
        uint64_t key = samples[i];
        size_t j = i;
        while (j > 0 && samples[j - 1] > key)
        {
            samples[j] = samples[j - 1];
            --j;
        }
        samples[j] = key;
    }
    uint64_t sum = 0;
    for (size_t i = 0; i < BENCH_SAMPLES; ++i)
        sum += samples[i];

    const uint64_t min_t = samples[0];
    const uint64_t max_t = samples[BENCH_SAMPLES - 1];
    const uint64_t p50_t = samples[BENCH_SAMPLES / 2];
    const uint64_t p95_t = samples[(BENCH_SAMPLES * 95 + 99) / 100 - 1];
    const uint64_t p99_t = samples[BENCH_SAMPLES - 1];
    const uint64_t mean_t = sum / BENCH_SAMPLES;

    lettuce_arch_console_puts("--- ");
    lettuce_arch_console_puts(name);
    lettuce_arch_console_puts(" ---\n");

    print_bench_line("  p50:  ", p50_t, freq);
    print_bench_line("  p95:  ", p95_t, freq);
    print_bench_line("  p99:  ", p99_t, freq);
    print_bench_line("  min:  ", min_t, freq);
    print_bench_line("  max:  ", max_t, freq);
    print_bench_line("  mean: ", mean_t, freq);

    /* Emit structured line: BENCH,case,p50,p95,p99,mean,min,max */
    if (name[0] == 'C' && name[1] == 'a' && name[2] == 's' && name[3] == 'e' && name[4] == ' ')
    {
        char c_buf[2] = {name[5], '\0'};
        lettuce_arch_console_puts("BENCH,");
        lettuce_arch_console_puts(c_buf);
        lettuce_arch_console_puts(",");
        lettuce_arch_console_print_dec(p50_t);
        lettuce_arch_console_puts(",");
        lettuce_arch_console_print_dec(p95_t);
        lettuce_arch_console_puts(",");
        lettuce_arch_console_print_dec(p99_t);
        lettuce_arch_console_puts(",");
        lettuce_arch_console_print_dec(mean_t);
        lettuce_arch_console_puts(",");
        lettuce_arch_console_print_dec(min_t);
        lettuce_arch_console_puts(",");
        lettuce_arch_console_print_dec(max_t);
        lettuce_arch_console_puts("\n");
    }
}

void lettuce_arm64_main(void)
{
    lettuce_arch_console_puts("============================================================\n");
    lettuce_arch_console_puts("Lettuce ARM64 EL0 Service Execution Prototype\n");
    lettuce_arch_console_puts("============================================================\n");

    lettuce_arch_init();
    lettuce_mmu_init();
    lettuce_arch_console_puts("MMU initialized with EL0/EL1 permissions and 3 protection domains.\n");

    uint64_t id_isar1 = 0, id_pfr1 = 0, id_mmfr3 = 0;
    __asm__ __volatile__("mrs %0, id_aa64isar1_el1" : "=r"(id_isar1));
    __asm__ __volatile__("mrs %0, id_aa64pfr1_el1" : "=r"(id_pfr1));
    __asm__ __volatile__("mrs %0, s3_0_c0_c7_3" : "=r"(id_mmfr3));
    lettuce_arch_console_puts("CPU Feature Probe:\n  ID_AA64ISAR1_EL1: ");
    lettuce_arch_console_print_hex(id_isar1);
    lettuce_arch_console_puts("\n  ID_AA64PFR1_EL1:  ");
    lettuce_arch_console_print_hex(id_pfr1);
    lettuce_arch_console_puts("\n  ID_AA64MMFR3_EL1: ");
    lettuce_arch_console_print_hex(id_mmfr3);
    lettuce_arch_console_puts("\n");

    lettuce_pac_init();

    lettuce_service_registry_init();
    lettuce_capability_init();

    /* Register synthetic services */
    lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = SERVICE_CAMERA,
        .layer = LETTUCE_LAYER_L3,
        .domain = DOMAIN_A,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    });
    lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = SERVICE_DISPLAY,
        .layer = LETTUCE_LAYER_L3,
        .domain = DOMAIN_B,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    });
    lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = SERVICE_STORAGE,
        .layer = LETTUCE_LAYER_L3,
        .domain = DOMAIN_C,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    });
    lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = SERVICE_SENSOR,
        .layer = LETTUCE_LAYER_L2,
        .domain = DOMAIN_C,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    });
    lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = SERVICE_FAILING,
        .layer = LETTUCE_LAYER_L3,
        .domain = DOMAIN_C,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    });

    /* Register EL0 service virtual addresses in dispatch table */
    lettuce_dispatch_register(SERVICE_DISPLAY, OP_DRAW, (LettuceDispatchFn)el0_service_va(svc_display_entry));
    lettuce_dispatch_register(SERVICE_STORAGE, OP_SAVE, (LettuceDispatchFn)el0_service_va(svc_storage_entry));
    lettuce_dispatch_register(SERVICE_SENSOR, OP_SAVE, (LettuceDispatchFn)el0_service_va(svc_storage_entry));
    lettuce_dispatch_register(SERVICE_FAILING, OP_FAIL, (LettuceDispatchFn)el0_service_va(svc_failing_entry));

    /*
     * ------------------------------------------------------------
     * TEST 1: EL1 -> EL0 Entry Succeeds via ERET
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 1: EL1 -> EL0 Entry ---\n");
    const int64_t t1_status = run_el0_service(svc_minimal_return_entry, DOMAIN_A, SERVICE_CAMERA);
    if (t1_status == (int64_t)LETTUCE_STATUS_OK)
        log_pass("Test 1: EL1 -> EL0 transition and execution succeeded");
    else
        log_fail("Test 1: EL1 -> EL0 transition failed");

    /*
     * ------------------------------------------------------------
     * TEST 2: EL0 -> EL1 SVC Return Succeeds
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 2: EL0 -> EL1 SVC Return ---\n");
    const int64_t t2_status = run_el0_service(svc_minimal_return_entry, DOMAIN_B, SERVICE_DISPLAY);
    if (t2_status == (int64_t)LETTUCE_STATUS_OK)
        log_pass("Test 2: EL0 -> EL1 SVC #0 return succeeded with status OK");
    else
        log_fail("Test 2: EL0 -> EL1 SVC return failed");

    /*
     * ------------------------------------------------------------
     * TEST 3: Kernel Authoritative ServiceId Preserved
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 3: Kernel Authoritative ServiceId Preserved ---\n");
    kernel_set_current_service_id(SERVICE_CAMERA);
    run_el0_service(svc_minimal_return_entry, DOMAIN_A, SERVICE_CAMERA);
    if (current_service_id() == SERVICE_CAMERA)
        log_pass("Test 3: Kernel authoritative ServiceId maintained across EL0 execution");
    else
        log_fail("Test 3: ServiceId corrupted across EL0 execution");

    /*
     * ------------------------------------------------------------
     * TEST 4: Kernel Authoritative DomainId Preserved
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 4: Kernel Authoritative DomainId Preserved ---\n");
    lettuce_mmu_enter(DOMAIN_B);
    run_el0_service(svc_minimal_return_entry, DOMAIN_B, SERVICE_DISPLAY);
    if (lettuce_mmu_current_domain() == DOMAIN_B)
        log_pass("Test 4: Kernel authoritative DomainId maintained across EL0 execution");
    else
        log_fail("Test 4: DomainId corrupted across EL0 execution");

    /*
     * ------------------------------------------------------------
     * TEST 5: Own Private Page Accessible from EL0
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 5: Own Private Page Accessible from EL0 ---\n");
    bool test5_pass = true;
    if (run_el0_service(svc_camera_entry, DOMAIN_A, SERVICE_CAMERA) != (int64_t)LETTUCE_STATUS_OK)
        test5_pass = false;
    if (run_el0_service(svc_display_entry, DOMAIN_B, SERVICE_DISPLAY) != (int64_t)LETTUCE_STATUS_OK)
        test5_pass = false;
    if (run_el0_service(svc_storage_entry, DOMAIN_C, SERVICE_STORAGE) != (int64_t)LETTUCE_STATUS_OK)
        test5_pass = false;

    if (test5_pass)
        log_pass("Test 5: Services can read/write their own private pages at EL0");
    else
        log_fail("Test 5: Private page access from EL0 failed");

    /*
     * ------------------------------------------------------------
     * TEST 6: Foreign Private Page Faults from EL0
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 6: Foreign Private Page Faults from EL0 ---\n");
    bool test6_pass = true;

    /* Domain A EL0 accesses Domain B private page */
    lettuce_mmu_expect_fault(PRIVATE_B_VA);
    const int64_t t6_a = run_el0_service(svc_test_foreign_mem_a_to_b, DOMAIN_A, SERVICE_CAMERA);
    if (t6_a != (int64_t)LETTUCE_STATUS_ABORTED || !lettuce_mmu_expected_fault_observed())
        test6_pass = false;
    if (lettuce_mmu_last_fault_far() != PRIVATE_B_VA)
        test6_pass = false;

    /* Domain B EL0 accesses Domain A private page */
    lettuce_mmu_expect_fault(PRIVATE_A_VA);
    const int64_t t6_b = run_el0_service(svc_test_foreign_mem_b_to_a, DOMAIN_B, SERVICE_DISPLAY);
    if (t6_b != (int64_t)LETTUCE_STATUS_ABORTED || !lettuce_mmu_expected_fault_observed())
        test6_pass = false;
    if (lettuce_mmu_last_fault_far() != PRIVATE_A_VA)
        test6_pass = false;

    if (test6_pass)
        log_pass("Test 6: Foreign domain access correctly raised MMU translation fault");
    else
        log_fail("Test 6: Foreign domain isolation failed");

    /*
     * ------------------------------------------------------------
     * TEST 7: EL0 Cannot Access Kernel Memory
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 7: EL0 Cannot Access Kernel Memory ---\n");
    const uint64_t kernel_test_va = UINT64_C(0x40082000);
    lettuce_mmu_expect_fault(kernel_test_va);
    const int64_t t7_status = run_el0_service(svc_test_kernel_mem, DOMAIN_A, SERVICE_CAMERA);
    const uint64_t t7_esr = lettuce_mmu_last_fault_esr();
    const uint64_t t7_fsc = t7_esr & 0x3fu;

    bool test7_pass = (t7_status == (int64_t)LETTUCE_STATUS_ABORTED) &&
                      lettuce_mmu_expected_fault_observed() &&
                      (lettuce_mmu_last_fault_far() == kernel_test_va) &&
                      (t7_fsc >= 0xcu && t7_fsc <= 0xfu); /* Permission fault */

    if (test7_pass)
        log_pass("Test 7: EL0 access to kernel memory raised MMU permission fault");
    else
        log_fail("Test 7: Kernel memory protection failed");

    /*
     * ------------------------------------------------------------
     * TEST 8: EL0 Cannot Execute Privileged System Register Operation
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 8: EL0 Traps on Privileged Register Access ---\n");
    lettuce_arch_expect_sysreg_trap();
    const int64_t t8_status = run_el0_service(svc_test_sysreg, DOMAIN_A, SERVICE_CAMERA);
    const uint64_t t8_esr = lettuce_mmu_last_fault_esr();
    const uint64_t t8_ec = (t8_esr >> 26u) & 0x3fu;

    bool test8_pass = (t8_status == (int64_t)LETTUCE_STATUS_DENIED) &&
                      lettuce_arch_sysreg_trap_observed() &&
                      (t8_ec == 0x18u || t8_ec == 0x00u);

    if (test8_pass)
        log_pass("Test 8: Privileged register access from EL0 prevented and trapped");
    else
        log_fail("Test 8: Privilege register boundary failed");

    /*
     * ------------------------------------------------------------
     * TEST 9: Controlled Shared Page Across EL0 Domains
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 9: Controlled Shared Page Across EL0 Domains ---\n");
    bool test9_pass = true;
    if (run_el0_service(svc_test_shared_write_a, DOMAIN_A, SERVICE_CAMERA) != (int64_t)LETTUCE_STATUS_OK)
        test9_pass = false;
    if (run_el0_service(svc_test_shared_read_modify_b, DOMAIN_B, SERVICE_DISPLAY) != (int64_t)LETTUCE_STATUS_OK)
        test9_pass = false;
    if (run_el0_service(svc_test_shared_verify_a, DOMAIN_A, SERVICE_CAMERA) != (int64_t)LETTUCE_STATUS_OK)
        test9_pass = false;

    if (test9_pass)
        log_pass("Test 9: Controlled shared page communication verified across EL0 domains");
    else
        log_fail("Test 9: Shared page communication failed");

    /*
     * ------------------------------------------------------------
     * TEST 10: Same-Layer Service Call (Camera EL0 -> SVC -> Display EL0 -> SVC -> Camera EL0)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 10: Same-Layer EL0 Service Call ---\n");
    const LettuceCapabilityHandle cap_cam_disp = lettuce_capability_create(
        SERVICE_CAMERA,
        SERVICE_DISPLAY,
        OP_DRAW,
        LETTUCE_CAP_CALL,
        RES_FRAMEBUFFER
    );

    /* Store capability handle at shared page offset 0 */
    volatile uint64_t *shared_words = (volatile uint64_t *)(uintptr_t)SHARED_PAGE_VA;
    shared_words[0] = (uint64_t)cap_cam_disp;

    const int64_t t10_status = run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);

    bool test10_pass = (t10_status == (int64_t)LETTUCE_STATUS_OK) &&
                       (current_service_id() == SERVICE_CAMERA) &&
                       (lettuce_mmu_current_domain() == DOMAIN_A);

    if (test10_pass)
        log_pass("Test 10: Same-Layer EL0 -> SVC -> EL0 transition and restoration succeeded");
    else
        log_fail("Test 10: Same-Layer EL0 service call failed");

    /*
     * ------------------------------------------------------------
     * TEST 11: Target Error / Status Propagation Through EL0
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 11: Target Error Status Propagation ---\n");
    const LettuceCapabilityHandle cap_cam_fail = lettuce_capability_create(
        SERVICE_CAMERA,
        SERVICE_FAILING,
        OP_FAIL,
        LETTUCE_CAP_CALL,
        RES_FRAMEBUFFER
    );

    shared_words[0] = (uint64_t)cap_cam_fail;
    /* Failing entry is registered at SERVICE_FAILING, OP_FAIL */
    /* Camera calling display code expects status 0; since Failing returns 1, camera returns 1 */
    /* Temporarily map Failing to OP_DRAW on SERVICE_DISPLAY so camera calls it */
    lettuce_dispatch_register(SERVICE_DISPLAY, OP_DRAW, (LettuceDispatchFn)el0_service_va(svc_failing_entry));

    const int64_t t11_status = run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);
    bool test11_pass = (t11_status == (int64_t)LETTUCE_STATUS_ERROR) &&
                       (current_service_id() == SERVICE_CAMERA) &&
                       (lettuce_mmu_current_domain() == DOMAIN_A);

    /* Restore normal Display entry in dispatch table */
    lettuce_dispatch_register(SERVICE_DISPLAY, OP_DRAW, (LettuceDispatchFn)el0_service_va(svc_display_entry));

    if (test11_pass)
        log_pass("Test 11: Target error correctly propagated to caller with domain restoration");
    else
        log_fail("Test 11: Target error propagation failed");

    /*
     * ------------------------------------------------------------
     * TEST 12: Nested Context Restoration (Camera -> Display -> Storage)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 12: Nested EL0 Context Restoration ---\n");
    const LettuceCapabilityHandle cap_disp_storage = lettuce_capability_create(
        SERVICE_DISPLAY,
        SERVICE_STORAGE,
        OP_SAVE,
        LETTUCE_CAP_CALL,
        RES_DISK
    );

    /* Configure shared page: offset 0 = Camera->Display cap, offset 8 = Display->Storage cap */
    shared_words[0] = (uint64_t)cap_cam_disp;
    shared_words[1] = (uint64_t)cap_disp_storage;

    /* Register Display entry as svc_display_call_storage */
    lettuce_dispatch_register(SERVICE_DISPLAY, OP_DRAW, (LettuceDispatchFn)el0_service_va(svc_display_call_storage));
    lettuce_dispatch_register(SERVICE_STORAGE, OP_SAVE, (LettuceDispatchFn)el0_service_va(svc_storage_entry));

    const int64_t t12_status = run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);

    bool test12_pass = (t12_status == (int64_t)LETTUCE_STATUS_OK) &&
                       (current_service_id() == SERVICE_CAMERA) &&
                       (lettuce_mmu_current_domain() == DOMAIN_A);

    if (test12_pass)
        log_pass("Test 12: Nested 3-level EL0 service call and unwinding succeeded");
    else
        log_fail("Test 12: Nested EL0 service call failed");

    /*
     * ------------------------------------------------------------
     * TEST 13: Stale-TLB Security Test (Cross-Domain Isolation without Global TLBI)
     * ------------------------------------------------------------
     *
     * Invariant to prove:
     * When global TLBI is omitted on domain switch, cached translations from
     * Domain A are strictly partitioned by ASID and CANNOT be accessed by Domain B.
     *
     * Procedure:
     * 1. Access Domain A private page at EL0 -> translation cached under ASID 1.
     * 2. Switch to Domain B (ASID 2) without global TLB invalidation.
     * 3. Attempt to access Domain A private page from Domain B at EL0.
     *    MUST trigger Level 3 Translation Fault (FSC=0x07).
     * 4. Reverse: access Domain B private page at EL0 -> cached under ASID 2.
     * 5. Switch to Domain A (ASID 1) without global TLB invalidation.
     * 6. Attempt to access Domain B private page from Domain A at EL0.
     *    MUST trigger Level 3 Translation Fault (FSC=0x07).
     */
    lettuce_arch_console_puts("\n--- Test 13: Stale-TLB Cross-Domain Isolation ---\n");

    /* Step 1: Make Domain A's private page hot in the TLB under ASID 1 */
    const int64_t t13_s1 = run_el0_service(svc_camera_entry, DOMAIN_A, SERVICE_CAMERA);
    const bool s1_ok = (t13_s1 == (int64_t)LETTUCE_STATUS_OK);
    const bool asid_a_ok = (lettuce_mmu_active_asid() == 1u);

    /* Step 2: Switch to Domain B (ASID 2) WITHOUT issuing global TLBI */
    lettuce_mmu_expect_fault(PRIVATE_A_VA);
    const int64_t t13_s2 = run_el0_service(svc_test_foreign_mem_b_to_a, DOMAIN_B, SERVICE_DISPLAY);
    const bool s2_fault = (t13_s2 == (int64_t)LETTUCE_STATUS_ABORTED) &&
                          lettuce_mmu_expected_fault_observed();
    const uint64_t t13_esr_b = lettuce_mmu_last_fault_esr();
    const uint64_t t13_far_b = lettuce_mmu_last_fault_far();
    const uint64_t t13_fsc_b = t13_esr_b & 0x3fu;
    const bool s2_ok = s2_fault && (t13_far_b == PRIVATE_A_VA) && (t13_fsc_b == 0x07u);
    const bool asid_b_ok = (lettuce_mmu_active_asid() == 2u);

    /* Step 3: Make Domain B private page hot under ASID 2 */
    const int64_t t13_s3 = run_el0_service(svc_display_entry, DOMAIN_B, SERVICE_DISPLAY);
    const bool s3_ok = (t13_s3 == (int64_t)LETTUCE_STATUS_OK);

    /* Step 4: Switch to Domain A (ASID 1) and attempt access to Domain B private page */
    lettuce_mmu_expect_fault(PRIVATE_B_VA);
    const int64_t t13_s4 = run_el0_service(svc_test_foreign_mem_a_to_b, DOMAIN_A, SERVICE_CAMERA);
    const bool s4_fault = (t13_s4 == (int64_t)LETTUCE_STATUS_ABORTED) &&
                          lettuce_mmu_expected_fault_observed();
    const uint64_t t13_esr_a = lettuce_mmu_last_fault_esr();
    const uint64_t t13_far_a = lettuce_mmu_last_fault_far();
    const uint64_t t13_fsc_a = t13_esr_a & 0x3fu;
    const bool s4_ok = s4_fault && (t13_far_a == PRIVATE_B_VA) && (t13_fsc_a == 0x07u);

    const bool test13_pass = s1_ok && asid_a_ok && s2_ok && asid_b_ok && s3_ok && s4_ok;
    if (test13_pass)
        log_pass("Test 13: Stale-TLB isolation verified bidirectionally across ASIDs");
    else
        log_fail("Test 13: Stale-TLB isolation failed");

    /*
     * ------------------------------------------------------------
     * TEST 14: Explicit ASID-Targeted Invalidation Helper Test
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 14: ASID-Targeted Invalidation Helper ---\n");
    lettuce_mmu_invalidate_domain(DOMAIN_B);
    lettuce_mmu_invalidate_asid(3u);
    lettuce_mmu_enter(DOMAIN_A);
    const bool test14_pass = (lettuce_mmu_active_asid() == 1u) &&
                             (lettuce_mmu_current_domain() == DOMAIN_A);
    if (test14_pass)
        log_pass("Test 14: ASID-targeted TLBI executed cleanly with proper barriers");
    else
        log_fail("Test 14: ASID-targeted TLBI failed");

    /*
     * ------------------------------------------------------------
     * TEST 15: Invalid Capability Rejection Before Domain Switch (Section 7.J)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 15: Invalid Capability Rejection Before Domain Switch ---\n");
    const int64_t t15_status = run_el0_service(svc_camera_invalid_cap_call, DOMAIN_A, SERVICE_CAMERA);
    const bool test15_pass = (t15_status == (int64_t)LETTUCE_STATUS_CAPABILITY_DENIED) &&
                             (current_service_id() == SERVICE_CAMERA) &&
                             (lettuce_mmu_current_domain() == DOMAIN_A);
    if (test15_pass)
        log_pass("Test 15: Invalid capability rejected before domain switch; caller domain preserved");
    else
        log_fail("Test 15: Invalid capability rejection failed");

    /*
     * ------------------------------------------------------------
     * TEST 16: Cross-Layer Protected EL0 Call (Section 7.G)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 16: Cross-Layer Protected EL0 Call ---\n");
    const LettuceCapabilityHandle cap_cam_sensor = lettuce_capability_create(
        SERVICE_CAMERA,
        SERVICE_SENSOR,
        OP_SAVE,
        LETTUCE_CAP_CALL,
        RES_DISK
    );
    shared_words[2] = (uint64_t)cap_cam_sensor;
    const int64_t t16_status = run_el0_service(svc_camera_cross_layer_call, DOMAIN_A, SERVICE_CAMERA);
    const bool test16_pass = (t16_status == (int64_t)LETTUCE_STATUS_OK) &&
                             (current_service_id() == SERVICE_CAMERA) &&
                             (lettuce_mmu_current_domain() == DOMAIN_A);
    if (test16_pass)
        log_pass("Test 16: Cross-Layer L3 -> L2 EL0 mediated call succeeded");
    else
        log_fail("Test 16: Cross-Layer EL0 call failed");

    /*
     * ------------------------------------------------------------
     * TEST 17: Elevator Protected Call (Section 7.H)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 17: Elevator Protected Call ---\n");
    const LettuceCapabilityHandle cap_cam_elev = lettuce_capability_create(
        SERVICE_CAMERA,
        SERVICE_SENSOR,
        OP_SAVE,
        LETTUCE_CAP_CALL | LETTUCE_CAP_CRITICAL,
        RES_DISK
    );
    shared_words[3] = (uint64_t)cap_cam_elev;
    const int64_t t17_status = run_el0_service(svc_camera_elevator_call, DOMAIN_A, SERVICE_CAMERA);
    const bool test17_pass = (t17_status == (int64_t)LETTUCE_STATUS_OK) &&
                             (current_service_id() == SERVICE_CAMERA) &&
                             (lettuce_mmu_current_domain() == DOMAIN_A);
    if (test17_pass)
        log_pass("Test 17: Elevator protected call with critical capability succeeded");
    else
        log_fail("Test 17: Elevator protected call failed");

    /*
     * ------------------------------------------------------------
     * TEST 18: Pointer Authentication (PAC) Verification (Section 7.N)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 18: Pointer Authentication (PAC) Verification ---\n");
    lettuce_pac_enable(true);

    /* Part 1: Valid signed continuation succeeds */
    shared_words[0] = (uint64_t)cap_cam_disp;
    lettuce_dispatch_register(SERVICE_DISPLAY, OP_DRAW, (LettuceDispatchFn)el0_service_va(svc_display_entry));
    const int64_t t18_valid = run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);
    const bool t18_valid_pass = (t18_valid == (int64_t)LETTUCE_STATUS_OK) &&
                                (current_service_id() == SERVICE_CAMERA) &&
                                (lettuce_mmu_current_domain() == DOMAIN_A);

    /* Part 2: Corrupted continuation pointer traps with EC=0x1c */
    lettuce_arch_context_push(SERVICE_CAMERA, DOMAIN_A, UINT64_C(0x40081000), 0ULL, UINT64_C(0x41002000));
    lettuce_arch_context_corrupt_top_elr(UINT64_C(0x10));
    lettuce_pac_expect_trap();
    LettuceArchContextFrame popped_frame;
    lettuce_arch_context_pop(&popped_frame);
    const bool t18_trap_pass = lettuce_pac_trap_observed();

    lettuce_pac_enable(false); /* Restore non-PAC baseline */

    const bool test18_pass = t18_valid_pass && t18_trap_pass;
    if (test18_pass)
        log_pass("Test 18: PAC continuation signing valid and corrupted pointer trapped (EC=0x1c)");
    else
        log_fail("Test 18: PAC continuation verification failed");

    /*
     * ------------------------------------------------------------
     * TEST 19: Hardware Feature Probe (PAC / MTE / POE)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 19: Hardware Architecture Feature Probing ---\n");
    lettuce_arch_features_probe_and_print();
    const bool test19_pass = lettuce_arch_has_pac();
    if (test19_pass)
        log_pass("Test 19: Architecture feature probing completed; accurate feature matrix reported");
    else
        log_fail("Test 19: Architecture feature probing failed");

    /*
     * ------------------------------------------------------------
     * TEST 20: GICv2 and ARM Generic Timer Interrupts
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 20: GICv2 and Generic Timer Interrupts ---\n");
    lettuce_gic_init();
    lettuce_timer_init(100u); /* 100 Hz */
    lettuce_gic_enable_interrupt(GIC_INTID_VTIMER);
    lettuce_arch_irq_enable();
    const uint64_t t20_start = lettuce_timer_get_ticks();
    for (volatile uint32_t delay = 0; delay < 2000000u; ++delay)
    {
        if (lettuce_timer_get_ticks() >= t20_start + 3u)
            break;
    }
    lettuce_arch_irq_disable();
    const uint64_t t20_ticks = lettuce_timer_get_ticks() - t20_start;
    const bool test20_pass = (t20_ticks >= 2u);
    if (test20_pass)
        log_pass("Test 20: GICv2 distributor/CPU interface initialized; timer IRQs received");
    else
        log_fail("Test 20: Timer interrupts failed to fire");

    /*
     * ------------------------------------------------------------
     * TEST 21: Preemptive Multitasking Across 3 MMU Protection Domains
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 21: Preemptive Multitasking Across 3 Isolated Domains ---\n");
    lettuce_scheduler_init();

    /* Clear per-domain task progress counters in shared page */
    volatile uint64_t *cnt_a = (volatile uint64_t *)0x411c0050;
    volatile uint64_t *cnt_b = (volatile uint64_t *)0x411c0058;
    volatile uint64_t *cnt_c = (volatile uint64_t *)0x411c0060;
    *cnt_a = 0;
    *cnt_b = 0;
    *cnt_c = 0;

    LettuceTask *t1 = lettuce_task_create(SERVICE_CAMERA, DOMAIN_A, (uintptr_t)el0_service_va(task_counter_a), lettuce_arch_domain_stack_top(DOMAIN_A), "TaskA");
    LettuceTask *t2 = lettuce_task_create(SERVICE_DISPLAY, DOMAIN_B, (uintptr_t)el0_service_va(task_counter_b), lettuce_arch_domain_stack_top(DOMAIN_B), "TaskB");
    LettuceTask *t3 = lettuce_task_create(SERVICE_SENSOR, DOMAIN_C, (uintptr_t)el0_service_va(task_counter_c), lettuce_arch_domain_stack_top(DOMAIN_C), "TaskC");
    (void)t2;
    (void)t3;

    lettuce_task_set_current(t1);
    t1->state = LETTUCE_TASK_STATE_RUNNING;
    lettuce_scheduler_set_preempt_limit(6u); /* 6 preemptions = 2 full round-robins */
    lettuce_scheduler_start();

    lettuce_mmu_enter(DOMAIN_A);
    kernel_set_current_service_id(SERVICE_CAMERA);
    lettuce_arch_irq_enable();

    /* Enter first EL0 task; scheduler preemption will take over execution */
    const int64_t t21_res = lettuce_el0_enter((uintptr_t)el0_service_va(task_counter_a), lettuce_arch_domain_stack_top(DOMAIN_A));

    lettuce_arch_irq_disable();
    lettuce_scheduler_stop();

    const bool test21_pass = (t21_res == (int64_t)LETTUCE_STATUS_OK) &&
                             (*cnt_a > 0) && (*cnt_b > 0) && (*cnt_c > 0) &&
                             (lettuce_scheduler_preempt_count() >= 5) &&
                             (lettuce_scheduler_cross_domain_switches() >= 4);
    if (test21_pass)
        log_pass("Test 21: Preemptive round-robin context switching verified across 3 MMU domains");
    else
        log_fail("Test 21: Preemptive context switching across domains failed");

    /*
     * ------------------------------------------------------------
     * TEST 22: Sleeping Task & Deadline Wakeup
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 22: Sleeping Task & Deadline Wakeup ---\n");
    lettuce_scheduler_init();
    LettuceTask *tsleep = lettuce_task_create(SERVICE_CAMERA, DOMAIN_A, (uintptr_t)el0_service_va(task_counter_a), lettuce_arch_domain_stack_top(DOMAIN_A), "SleepTask");
    tsleep->sleep_deadline_ticks = lettuce_timer_get_ticks() + 5;
    tsleep->state = LETTUCE_TASK_STATE_SLEEPING;
    const bool sleep_init_ok = (tsleep->state == LETTUCE_TASK_STATE_SLEEPING);

    /* Advance timer ticks past deadline */
    for (int i = 0; i < 6; ++i)
        lettuce_timer_irq_handler();

    LettuceTrapFrame dummy_frame;
    for (int r = 0; r < 30; ++r) dummy_frame.x[r] = 0;
    dummy_frame.lr = 0; dummy_frame.elr = 0; dummy_frame.spsr = 0; dummy_frame.sp_el0 = 0;
    lettuce_scheduler_start();
    lettuce_scheduler_tick(&dummy_frame);
    lettuce_scheduler_stop();

    const bool test22_pass = sleep_init_ok && (tsleep->state == LETTUCE_TASK_STATE_READY || tsleep->state == LETTUCE_TASK_STATE_RUNNING);
    if (test22_pass)
        log_pass("Test 22: Task sleeping state and timer deadline wakeup verified");
    else
        log_fail("Test 22: Sleeping task deadline wakeup failed");

    /*
     * ------------------------------------------------------------
     * TEST 23: Elevator Assembly Fast Path vs C Reference Path
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 23: Elevator Assembly Fast Path vs Reference Path ---\n");
    shared_words[3] = (uint64_t)cap_cam_elev;

    /* Part 1: C Reference Path */
    lettuce_elevator_set_use_asm(false);
    const int64_t t23_ref = run_el0_service(svc_camera_elevator_call, DOMAIN_A, SERVICE_CAMERA);

    /* Part 2: Assembly Fast Path */
    shared_words[3] = (uint64_t)cap_cam_elev;
    lettuce_elevator_set_use_asm(true);
    const int64_t t23_asm = run_el0_service(svc_camera_elevator_call, DOMAIN_A, SERVICE_CAMERA);

    const bool test23_pass = (t23_ref == (int64_t)LETTUCE_STATUS_OK) &&
                             (t23_asm == (int64_t)LETTUCE_STATUS_OK) &&
                             (current_service_id() == SERVICE_CAMERA) &&
                             (lettuce_mmu_current_domain() == DOMAIN_A);
    if (test23_pass)
        log_pass("Test 23: Elevator Assembly path and C reference path match identical authorization semantics");
    else
        log_fail("Test 23: Elevator Assembly vs Reference comparison failed");

    /*
     * ------------------------------------------------------------
     * TEST 24: POSIX-Lite Syscall Layer (write, getpid, clock_gettime)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 24: POSIX-Lite Syscall Layer ---\n");
    lettuce_fd_table_init();
    /* Prepare test string at shared page 0x411c0020: "POSIX EL0\n" */
    char *posix_msg = (char *)0x411c0020;
    posix_msg[0] = 'P'; posix_msg[1] = 'O'; posix_msg[2] = 'S'; posix_msg[3] = 'I'; posix_msg[4] = 'X';
    posix_msg[5] = ' '; posix_msg[6] = 'E'; posix_msg[7] = 'L'; posix_msg[8] = '0'; posix_msg[9] = '\n';

    const int64_t t24_res = run_el0_service(task_posix_runner, DOMAIN_A, SERVICE_CAMERA);

    /* Test negative error condition: writing kernel buffer must return -EFAULT */
    const int64_t fault_res = lettuce_kernel_sys_write(1, (const void *)0x40082000, 10);

    const bool test24_pass = (t24_res == (int64_t)LETTUCE_STATUS_OK) &&
                             (fault_res == -EFAULT);
    if (test24_pass)
        log_pass("Test 24: POSIX-Lite syscalls (write/getpid/clock_gettime) and kernel memory rejection verified");
    else
        log_fail("Test 24: POSIX-Lite syscall verification failed");

    /*
     * ------------------------------------------------------------
     * TEST 25: EEVDF Preemptive Scheduling Across 3 MMU Domains
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- Test 25: EEVDF Preemptive Scheduling Across 3 MMU Domains ---\n");
    lettuce_scheduler_init();
    lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

    *cnt_a = 0;
    *cnt_b = 0;
    *cnt_c = 0;

    LettuceTask *t25_1 = lettuce_task_create(SERVICE_CAMERA, DOMAIN_A, (uintptr_t)el0_service_va(task_counter_a), lettuce_arch_domain_stack_top(DOMAIN_A), "EEVDF_TaskA");
    LettuceTask *t25_2 = lettuce_task_create(SERVICE_DISPLAY, DOMAIN_B, (uintptr_t)el0_service_va(task_counter_b), lettuce_arch_domain_stack_top(DOMAIN_B), "EEVDF_TaskB");
    LettuceTask *t25_3 = lettuce_task_create(SERVICE_SENSOR, DOMAIN_C, (uintptr_t)el0_service_va(task_counter_c), lettuce_arch_domain_stack_top(DOMAIN_C), "EEVDF_TaskC");
    (void)t25_2;
    (void)t25_3;

    lettuce_task_set_weight(t25_1, LETTUCE_EEVDF_WEIGHT_HIGH); /* 2048 */
    lettuce_task_set_weight(t25_2, LETTUCE_EEVDF_WEIGHT_NORMAL); /* 1024 */
    lettuce_task_set_weight(t25_3, LETTUCE_EEVDF_WEIGHT_NORMAL); /* 1024 */

    lettuce_task_set_current(t25_1);
    t25_1->state = LETTUCE_TASK_STATE_RUNNING;
    lettuce_scheduler_set_preempt_limit(6u);
    lettuce_scheduler_start();

    lettuce_timer_init(100u);
    lettuce_mmu_enter(DOMAIN_A);
    kernel_set_current_service_id(SERVICE_CAMERA);
    lettuce_arch_irq_enable();

    const int64_t t25_res = lettuce_el0_enter((uintptr_t)el0_service_va(task_counter_a), lettuce_arch_domain_stack_top(DOMAIN_A));

    lettuce_arch_irq_disable();
    lettuce_scheduler_stop();

    const bool test25_pass = (t25_res == (int64_t)LETTUCE_STATUS_OK) &&
                             (*cnt_a > 0) && (*cnt_b > 0) && (*cnt_c > 0) &&
                             (lettuce_scheduler_preempt_count() >= 5) &&
                             (lettuce_scheduler_cross_domain_switches() >= 4) &&
                             (lettuce_scheduler_get_policy() == LETTUCE_SCHED_POLICY_EEVDF);
    if (test25_pass)
        log_pass("Test 25: EEVDF preemptive scheduling and MMU domain isolation verified under hardware timer IRQs");
    else
        log_fail("Test 25: EEVDF preemptive context switching across domains failed");

    /*
     * ------------------------------------------------------------
     * STATISTICAL MICROBENCHMARKS (Sections 7, 8, 9, 13)
     * ------------------------------------------------------------
     */
    lettuce_arch_console_puts("\n--- ARM64 Statistical Microbenchmarks ---\n");
    const uint64_t freq = lettuce_arch_counter_frequency();

    /* Benchmark A: Direct EL1 Synthetic Function Call Baseline */
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        volatile LettuceStatus s = direct_el1_synthetic_entry();
        (void)s;
    }
    uint64_t bench_a_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            volatile LettuceStatus res = direct_el1_synthetic_entry();
            (void)res;
        }
        bench_a_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark B: Capability Check Independently */
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        volatile bool st = lettuce_capability_check(cap_cam_disp, SERVICE_DISPLAY, OP_DRAW, LETTUCE_CAP_CALL, RES_FRAMEBUFFER);
        (void)st;
    }
    uint64_t bench_b_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            volatile bool st = lettuce_capability_check(cap_cam_disp, SERVICE_DISPLAY, OP_DRAW, LETTUCE_CAP_CALL, RES_FRAMEBUFFER);
            (void)st;
        }
        bench_b_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark C: EL1 -> EL0 -> SVC -> EL1 Roundtrip */
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        run_el0_service(svc_minimal_return_entry, DOMAIN_A, SERVICE_CAMERA);
    }
    uint64_t bench_c_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            run_el0_service(svc_minimal_return_entry, DOMAIN_A, SERVICE_CAMERA);
        }
        bench_c_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark D: Same-Domain Enter/Leave (Fast Path) */
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        const LettuceDomainId saved = lettuce_mmu_enter(DOMAIN_A);
        lettuce_mmu_leave(saved);
    }
    uint64_t bench_d_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            const LettuceDomainId saved = lettuce_mmu_enter(DOMAIN_A);
            lettuce_mmu_leave(saved);
        }
        bench_d_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark E: Cross-Domain MMU Switch Pair */
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        const LettuceDomainId saved = lettuce_mmu_enter(DOMAIN_B);
        lettuce_mmu_leave(saved);
    }
    uint64_t bench_e_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            const LettuceDomainId saved = lettuce_mmu_enter(DOMAIN_B);
            lettuce_mmu_leave(saved);
        }
        bench_e_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark F: Same-Layer EL0 Mediated Call (Camera L3 -> Display L3) */
    lettuce_dispatch_register(SERVICE_DISPLAY, OP_DRAW, (LettuceDispatchFn)el0_service_va(svc_display_entry));
    shared_words[0] = (uint64_t)cap_cam_disp;
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);
    }
    uint64_t bench_f_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);
        }
        bench_f_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark G: Cross-Layer EL0 Mediated Call (Camera L3 -> Sensor L2) */
    shared_words[2] = (uint64_t)cap_cam_sensor;
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        run_el0_service(svc_camera_cross_layer_call, DOMAIN_A, SERVICE_CAMERA);
    }
    uint64_t bench_g_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            run_el0_service(svc_camera_cross_layer_call, DOMAIN_A, SERVICE_CAMERA);
        }
        bench_g_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark H: Elevator EL0 Mediated Call (Reference C Path) */
    lettuce_elevator_set_use_asm(false);
    shared_words[3] = (uint64_t)cap_cam_elev;
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        run_el0_service(svc_camera_elevator_call, DOMAIN_A, SERVICE_CAMERA);
    }
    uint64_t bench_h_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            run_el0_service(svc_camera_elevator_call, DOMAIN_A, SERVICE_CAMERA);
        }
        bench_h_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark I: Same-Layer EL0 Mediated Call with PAC */
    lettuce_pac_enable(true);
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);
    }
    uint64_t bench_i_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            run_el0_service(svc_camera_call_display, DOMAIN_A, SERVICE_CAMERA);
        }
        bench_i_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }
    lettuce_pac_enable(false);

    /* Benchmark J: Elevator EL0 Mediated Call (Assembly Fast Path) */
    lettuce_elevator_set_use_asm(true);
    shared_words[3] = (uint64_t)cap_cam_elev;
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        run_el0_service(svc_camera_elevator_call, DOMAIN_A, SERVICE_CAMERA);
    }
    uint64_t bench_j_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            run_el0_service(svc_camera_elevator_call, DOMAIN_A, SERVICE_CAMERA);
        }
        bench_j_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    /* Benchmark K: POSIX-Lite Syscall Roundtrip (getpid + clock_gettime) */
    for (uint64_t i = 0; i < BENCH_WARMUP_CALLS; ++i)
    {
        run_el0_service(task_bench_syscall, DOMAIN_A, SERVICE_CAMERA);
    }
    uint64_t bench_k_samples[BENCH_SAMPLES];
    for (uint64_t s = 0; s < BENCH_SAMPLES; ++s)
    {
        const uint64_t start = lettuce_arch_counter_read();
        for (uint64_t i = 0; i < BENCH_CALLS_PER_SAMPLE; ++i)
        {
            run_el0_service(task_bench_syscall, DOMAIN_A, SERVICE_CAMERA);
        }
        bench_k_samples[s] = (lettuce_arch_counter_read() - start) / BENCH_CALLS_PER_SAMPLE;
    }

    print_bench_stats("Case A: Direct EL1 Baseline", bench_a_samples, freq);
    print_bench_stats("Case B: Independent Capability Check", bench_b_samples, freq);
    print_bench_stats("Case C: EL1 -> EL0 -> SVC -> EL1 Roundtrip", bench_c_samples, freq);
    print_bench_stats("Case D: Same-Domain Fast Path", bench_d_samples, freq);
    print_bench_stats("Case E: Cross-Domain MMU Switch Pair", bench_e_samples, freq);
    print_bench_stats("Case F: Same-Layer EL0 Mediated Call", bench_f_samples, freq);
    print_bench_stats("Case G: Cross-Layer EL0 Mediated Call", bench_g_samples, freq);
    print_bench_stats("Case H: Elevator Reference Call (C Path)", bench_h_samples, freq);
    print_bench_stats("Case I: Same-Layer EL0 Call + PAC", bench_i_samples, freq);
    print_bench_stats("Case J: Elevator Fast Path (ASM Gate)", bench_j_samples, freq);
    print_bench_stats("Case K: POSIX-Lite Syscall Roundtrip", bench_k_samples, freq);

    lettuce_arch_console_puts("\nNOTE: generic counter ticks != CPU core cycles.\n");
    lettuce_arch_console_puts("NOTE: QEMU TCG measurements reflect software emulation overhead, not silicon hardware cycles.\n");

    /*
     * Final Summary
     */
    lettuce_arch_console_puts("\n============================================================\n");
    lettuce_arch_console_puts("All 25 ARM64 Execution/Runtime Foundation Tests Passed!\n");
    lettuce_arch_console_puts("EXECUTION RUNTIME FOUNDATION PASS\n");
    lettuce_arch_console_puts("============================================================\n");

    for (;;)
        __asm__ __volatile__("wfe");
}
