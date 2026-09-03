/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/mmu.c
 *
 * Purpose:
 *   Implements ARM64 page-table setup and protection-domain entry through
 *   TTBR0_EL1 roots carrying ASID tags.
 *
 * Execution context:
 *   EL1 MMU initialization and protected domain-transition paths.
 *
 * Key invariants:
 *   - User mappings are non-global and supervisor mappings retain EL1-only use.
 *   - TTBR0_EL1 writes observe required dsb ish and isb ordering.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../include/arch.h"

/*
 * ============================================================================
 * ARM64 MMU CONFIGURATION & CONSTANTS
 * ============================================================================
 *
 * Translation Granule: 4KB
 * Virtual Address Space: 39-bit (T0SZ = 25, covers 0x0000000000 - 0x7fffffffff, 512 GB)
 * Translation Root: Level 1 Table (512 entries, each entry represents 1 GB)
 * Intermediate Level: Level 2 Table (512 entries, each entry represents 2 MB)
 * Leaf Level: Level 3 Table (512 entries, each entry represents 4 KB)
 *
 * Address Space Layout:
 * - 0x00000000 - 0x3fffffff (1 GB): Devices (L1 entry 0 -> shared kernel_l2_device)
 *   - 0x09000000: PL011 UART (2MB block at L2 entry 72, Device-nGnRnE, PXN, UXN)
 * - 0x40000000 - 0x7fffffff (1 GB): RAM & Services (L1 entry 1 -> domain-specific L2)
 *   - 0x40000000 - 0x401fffff: Common Kernel Image (2MB block at L2 entry 0, Normal WB/WA)
 *     Contains _start, kernel text, exception vectors, data, bss, stack, and page tables.
 *     Mapped identically in all domains so kernel transitions never unmap kernel code.
 *   - 0x41000000 - 0x411fffff: Domain Memory Region (2MB region at L2 entry 8 -> L3 table)
 *     - 0x41000000: Domain A Private Page (L3 entry 0, RW in Domain A only)
 *     - 0x41100000: Domain B Private Page (L3 entry 256, RW in Domain B only)
 *     - 0x41180000: Domain C Private Page (L3 entry 384, RW in Domain C only)
 *     - 0x411c0000: Shared Buffer Page    (L3 entry 448, RW in Domains A, B, and C)
 *     - 0x411e0000: Domain A Read-Only Page (L3 entry 480, RO in Domain A for perm fault test)
 */

#define PAGE_SIZE               UINT64_C(4096)
#define TABLE_ENTRIES           512u

#define DOMAIN_A                UINT32_C(100)
#define DOMAIN_B                UINT32_C(200)
#define DOMAIN_C                UINT32_C(300)

#define GIC_PA                  UINT64_C(0x08000000)
#define UART_PA                 UINT64_C(0x09000000)
#define KERNEL_RAM_PA           UINT64_C(0x40000000)

#define PRIVATE_A_VA            UINT64_C(0x41000000)
#define PRIVATE_A_PA            UINT64_C(0x41000000)

#define PRIVATE_B_VA            UINT64_C(0x41100000)
#define PRIVATE_B_PA            ((uintptr_t)domain_b_private_page)

#define PRIVATE_C_VA            UINT64_C(0x41180000)
#define PRIVATE_C_PA            ((uintptr_t)domain_c_private_page)

#define SHARED_PAGE_VA          UINT64_C(0x411c0000)
#define SHARED_PAGE_PA          ((uintptr_t)shared_buffer_page)

#define RO_TEST_PAGE_VA         UINT64_C(0x411e0000)
#define RO_TEST_PAGE_PA         ((uintptr_t)ro_test_page)

#define SERVICE_CODE_VA         UINT64_C(0x41050000)

#define DOMAIN_A_STACK_VA       UINT64_C(0x41001000)
#define DOMAIN_A_STACK_TOP      UINT64_C(0x41002000)

#define DOMAIN_B_STACK_VA       UINT64_C(0x41101000)
#define DOMAIN_B_STACK_TOP      UINT64_C(0x41102000)

#define DOMAIN_C_STACK_VA       UINT64_C(0x41181000)
#define DOMAIN_C_STACK_TOP      UINT64_C(0x41182000)

/*
 * Descriptor Bitfields (ARMv8-A DDI 0487):
 */
#define DESC_VALID              UINT64_C(1)
#define DESC_TABLE              UINT64_C(2)        /* bit 1 = 1 for table in L1/L2 */
#define DESC_BLOCK              UINT64_C(0)        /* bit 1 = 0 for block in L1/L2 */
#define DESC_PAGE               UINT64_C(2)        /* bit 1 = 1 for page in L3 */

#define DESC_ATTR_NORMAL        (UINT64_C(0) << 2) /* AttrIndx = 0 (Normal WB/WA) */
#define DESC_ATTR_DEVICE        (UINT64_C(1) << 2) /* AttrIndx = 1 (Device-nGnRnE) */

#define DESC_AP_EL1_RW          (UINT64_C(0) << 6) /* Read/Write at EL1, No EL0 Access */
#define DESC_AP_EL1_RO          (UINT64_C(2) << 6) /* Read-Only at EL1, No EL0 Access */
#define DESC_AP_EL0_RW          (UINT64_C(1) << 6) /* Read/Write at EL1 and EL0 */
#define DESC_AP_EL0_RO          (UINT64_C(3) << 6) /* Read-Only at EL1 and EL0 */

#define DESC_SH_NON             (UINT64_C(0) << 8)
#define DESC_SH_OUTER           (UINT64_C(2) << 8)
#define DESC_SH_INNER           (UINT64_C(3) << 8)

#define DESC_AF                 (UINT64_C(1) << 10) /* Access Flag (1 = accessed) */
#define DESC_NG                 (UINT64_C(1) << 11) /* Non-Global (tagged with ASID) */

#define DESC_PXN                (UINT64_C(1) << 53) /* Privileged Execute Never */
#define DESC_UXN                (UINT64_C(1) << 54) /* Unprivileged Execute Never */

/*
 * ============================================================================
 * STATIC TRANSLATION TABLE STORAGE (Fixed Arrays, No Dynamic Allocator)
 * ============================================================================
 */
static uint64_t kernel_l2_device[TABLE_ENTRIES] __attribute__((aligned(4096)));

static uint64_t domain_a_l1[TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t domain_a_l2[TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t domain_a_l3[TABLE_ENTRIES] __attribute__((aligned(4096)));

static uint64_t domain_b_l1[TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t domain_b_l2[TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t domain_b_l3[TABLE_ENTRIES] __attribute__((aligned(4096)));

static uint64_t domain_c_l1[TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t domain_c_l2[TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t domain_c_l3[TABLE_ENTRIES] __attribute__((aligned(4096)));

/*
 * Physical page backing for domain-private memory, stacks, shared memory, and test pages.
 */
static uint8_t domain_a_private_page[PAGE_SIZE] __attribute__((aligned(4096)));
static uint8_t domain_a_stack_page[PAGE_SIZE] __attribute__((aligned(4096)));

static uint8_t domain_b_private_page[PAGE_SIZE] __attribute__((aligned(4096)));
static uint8_t domain_b_stack_page[PAGE_SIZE] __attribute__((aligned(4096)));

static uint8_t domain_c_private_page[PAGE_SIZE] __attribute__((aligned(4096)));
static uint8_t domain_c_stack_page[PAGE_SIZE] __attribute__((aligned(4096)));

static uint8_t shared_buffer_page[PAGE_SIZE] __attribute__((aligned(4096)));
static uint8_t ro_test_page[PAGE_SIZE] __attribute__((aligned(4096)));

extern uint8_t el0_service_code_start[];

/*
 * ============================================================================
 * ARM64 ARCHITECTURE PRIVATE DOMAIN OBJECT
 * ============================================================================
 */
typedef struct LettuceArchDomain {
    uint64_t ttbr0_val;       /* Precomputed ((asid << 48) | (l1_root & 0x0000FFFFFFFFF000ULL)) */
    uint64_t *l1_root;        /* 8 bytes */
    uintptr_t private_va;     /* 8 bytes */
    uintptr_t private_pa;     /* 8 bytes */
    uintptr_t stack_va;       /* 8 bytes */
    uintptr_t stack_pa;       /* 8 bytes */
    uintptr_t stack_top;      /* 8 bytes */
    const char *name;         /* 8 bytes */
    LettuceDomainId id;       /* 4 bytes */
    uint16_t asid;            /* 2 bytes */
    uint16_t reserved;        /* 2 bytes explicit padding */
} LettuceArchDomain;

_Static_assert(sizeof(LettuceArchDomain) == 72, "LettuceArchDomain must remain exactly 72 bytes.");
_Static_assert(offsetof(LettuceArchDomain, ttbr0_val) == 0, "ttbr0_val must be at offset 0.");

static LettuceArchDomain arch_domains[] = {
    {
        .ttbr0_val = 0,
        .l1_root = domain_a_l1,
        .private_va = PRIVATE_A_VA,
        .private_pa = (uintptr_t)domain_a_private_page,
        .stack_va = DOMAIN_A_STACK_VA,
        .stack_pa = (uintptr_t)domain_a_stack_page,
        .stack_top = DOMAIN_A_STACK_TOP,
        .name = "Domain A (Camera L3)",
        .id = DOMAIN_A,
        .asid = 1u,
        .reserved = 0
    },
    {
        .ttbr0_val = 0,
        .l1_root = domain_b_l1,
        .private_va = PRIVATE_B_VA,
        .private_pa = (uintptr_t)domain_b_private_page,
        .stack_va = DOMAIN_B_STACK_VA,
        .stack_pa = (uintptr_t)domain_b_stack_page,
        .stack_top = DOMAIN_B_STACK_TOP,
        .name = "Domain B (Display L3)",
        .id = DOMAIN_B,
        .asid = 2u,
        .reserved = 0
    },
    {
        .ttbr0_val = 0,
        .l1_root = domain_c_l1,
        .private_va = PRIVATE_C_VA,
        .private_pa = (uintptr_t)domain_c_private_page,
        .stack_va = DOMAIN_C_STACK_VA,
        .stack_pa = (uintptr_t)domain_c_stack_page,
        .stack_top = DOMAIN_C_STACK_TOP,
        .name = "Domain C (Storage L2)",
        .id = DOMAIN_C,
        .asid = 3u,
        .reserved = 0
    }
};

#define ARCH_DOMAIN_COUNT (sizeof(arch_domains) / sizeof(arch_domains[0]))

static LettuceDomainId current_domain = LETTUCE_DOMAIN_ID_INVALID;

/* Expected-fault mechanism state */
static volatile bool g_expecting_fault;
static volatile bool g_fault_observed;
static volatile uint64_t g_expected_far;
static volatile uint64_t g_last_fault_far;
static volatile uint64_t g_last_fault_esr;
static volatile uint64_t g_last_fault_elr;

/* Sysreg trap observation state */
static volatile bool g_expecting_sysreg_trap;
static volatile bool g_sysreg_trap_observed;

uintptr_t lettuce_arch_domain_stack_top(LettuceDomainId domain)
{
    if (domain == DOMAIN_A)
        return DOMAIN_A_STACK_TOP;
    if (domain == DOMAIN_B)
        return DOMAIN_B_STACK_TOP;
    if (domain == DOMAIN_C)
        return DOMAIN_C_STACK_TOP;
    return 0;
}

static inline const LettuceArchDomain *arch_domain_lookup(LettuceDomainId id)
{
    switch (id)
    {
        case DOMAIN_A: return &arch_domains[0];
        case DOMAIN_B: return &arch_domains[1];
        case DOMAIN_C: return &arch_domains[2];
        default: return NULL;
    }
}

uint64_t lettuce_arch_domain_ttbr0_val(LettuceDomainId domain)
{
    const LettuceArchDomain *arch_dom = arch_domain_lookup(domain);
    if (arch_dom != NULL)
        return arch_dom->ttbr0_val;
    return 0;
}

/*
 * Helper functions to construct translation table descriptors.
 */
static inline uint64_t table_descriptor(const uint64_t *table)
{
    return ((uint64_t)(uintptr_t)table & ~UINT64_C(0xfff)) | DESC_VALID | DESC_TABLE;
}

static inline uint64_t device_block_descriptor(uint64_t physical)
{
    return (physical & ~UINT64_C(0x1fffff)) | DESC_VALID | DESC_BLOCK | DESC_AF |
           DESC_SH_NON | DESC_ATTR_DEVICE | DESC_UXN | DESC_PXN | DESC_AP_EL1_RW;
}

static inline uint64_t kernel_ram_block_descriptor(uint64_t physical)
{
    return (physical & ~UINT64_C(0x1fffff)) | DESC_VALID | DESC_BLOCK | DESC_AF |
           DESC_SH_INNER | DESC_ATTR_NORMAL | DESC_AP_EL1_RW | DESC_UXN;
}

static inline uint64_t page_descriptor(uint64_t physical, uint64_t attributes)
{
    return (physical & ~UINT64_C(0xfff)) | DESC_VALID | DESC_PAGE | DESC_AF |
           DESC_SH_INNER | attributes;
}

/*
 * Build the translation tables for a protection domain.
 */
static void init_domain_tables(
    uint64_t *l1,
    uint64_t *l2,
    uint64_t *l3,
    uintptr_t private_va,
    uintptr_t private_pa,
    uintptr_t stack_va,
    uintptr_t stack_pa,
    bool include_ro_test_page)
{
    for (uint32_t i = 0; i < TABLE_ENTRIES; ++i)
    {
        l1[i] = 0;
        l2[i] = 0;
        l3[i] = 0;
    }

    /*
     * L1 Translation Table:
     * - Entry 0 (0x00000000 - 0x3fffffff, 1GB): Maps peripheral devices (UART) via kernel_l2_device.
     * - Entry 1 (0x40000000 - 0x7fffffff, 1GB): Maps RAM and service memory via domain's L2 table.
     */
    l1[0] = table_descriptor(kernel_l2_device);
    l1[1] = table_descriptor(l2);

    /*
     * L2 Translation Table (Upper 1GB: 0x40000000 - 0x7fffffff):
     * - Entry 0 (0x40000000 - 0x401fffff, 2MB): Common kernel memory mapping.
     *   DESC_AP_EL1_RW | DESC_UXN: Accessible ONLY at EL1. Inaccessible to EL0 (no read/write/exec).
     * - Entry 8 (0x41000000 - 0x411fffff, 2MB): Domain service memory region.
     *   Points to domain-specific L3 table (4KB page mappings).
     */
    l2[0] = kernel_ram_block_descriptor(KERNEL_RAM_PA);
    l2[(PRIVATE_A_VA >> 21u) & 0x1ffu] = table_descriptor(l3);

    /*
     * L3 Translation Table (4KB Granule):
     * - Map this domain's private page with EL0 Read/Write, UXN, and Non-Global (DESC_NG).
     *   Tagged with the active domain's ASID so TLB entries are partitioned by ASID.
     */
    const uint32_t private_idx = (private_va >> 12u) & 0x1ffu;
    l3[private_idx] = page_descriptor(private_pa, DESC_ATTR_NORMAL | DESC_AP_EL0_RW | DESC_UXN | DESC_NG);

    /*
     * - Map this domain's private stack page with EL0 Read/Write, UXN, and Non-Global (DESC_NG).
     */
    const uint32_t stack_idx = (stack_va >> 12u) & 0x1ffu;
    l3[stack_idx] = page_descriptor(stack_pa, DESC_ATTR_NORMAL | DESC_AP_EL0_RW | DESC_UXN | DESC_NG);

    /*
     * - Map service code page at SERVICE_CODE_VA (0x41050000) with EL0 Read-Only, Executable (PXN, UXN=0, DESC_NG).
     */
    const uint32_t service_code_idx = (SERVICE_CODE_VA >> 12u) & 0x1ffu;
    l3[service_code_idx] = page_descriptor((uintptr_t)el0_service_code_start, DESC_ATTR_NORMAL | DESC_AP_EL0_RO | DESC_PXN | DESC_NG);

    /*
     * - Map shared buffer page (EL0 Read/Write, UXN, DESC_NG).
     */
    const uint32_t shared_idx = (SHARED_PAGE_VA >> 12u) & 0x1ffu;
    l3[shared_idx] = page_descriptor(SHARED_PAGE_PA, DESC_ATTR_NORMAL | DESC_AP_EL0_RW | DESC_UXN | DESC_NG);

    /*
     * - Optionally map a Read-Only test page for permission-fault testing (DESC_NG).
     */
    if (include_ro_test_page)
    {
        const uint32_t ro_idx = (RO_TEST_PAGE_VA >> 12u) & 0x1ffu;
        l3[ro_idx] = page_descriptor(RO_TEST_PAGE_PA, DESC_ATTR_NORMAL | DESC_AP_EL0_RO | DESC_UXN | DESC_NG);
    }
}

/*
 * Architectural ASID-aware TTBR update for static protection domain switching.
 *
 * SAFETY INVARIANT & ARCHITECTURAL MODEL:
 * 1. Each protection domain is assigned a unique, immutable 16-bit ASID:
 *    - Domain A (Camera L3)  = ASID 1
 *    - Domain B (Display L3) = ASID 2
 *    - Domain C (Storage L3) = ASID 3
 * 2. All domain-private translation descriptors are marked Non-Global (DESC_NG, bit 11 = 1),
 *    causing cached TLB entries to be tagged with the active ASID in hardware.
 * 3. Kernel RAM and UART blocks are marked Global (nG = 0), allowing shared supervisor access.
 * 4. Translation tables are static and immutable after initialization.
 * 5. ASIDs are never reused or recycled across domain lifecycles.
 *
 * Because translation entries are tagged with their owning domain's ASID, cached entries
 * from Domain A will NOT match lookups performed while Domain B (ASID 2) is active.
 * Hardware automatically ignores entries with mismatched ASIDs during address translation.
 *
 * Therefore, steady-state switching between static domains does NOT require global TLB
 * invalidation (tlbi vmalle1). The transition requires only:
 * 1. DSB ISH: Ensure all prior memory accesses and page-table writes complete.
 * 2. MSR TTBR0_ELx: Atomically install the new root table base and ASID.
 * 3. ISB: Synchronize instruction fetch and execution pipeline so subsequent
 *    operations use the new translation context.
 */
static inline void write_ttbr(uint64_t ttbr_val)
{
    if (lettuce_arch_current_el() == 2u)
    {
        __asm__ __volatile__(
            "dsb ish\n\t"
            "msr ttbr0_el2, %0\n\t"
            "isb"
            :
            : "r"(ttbr_val)
            : "memory"
        );
    }
    else
    {
        __asm__ __volatile__(
            "dsb ish\n\t"
            "msr ttbr0_el1, %0\n\t"
            "isb"
            :
            : "r"(ttbr_val)
            : "memory"
        );
    }
}

/*
 * Explicit ASID-targeted TLB invalidation.
 *
 * Used when translations for an ASID are modified or when an ASID is recycled.
 * Invalidates only the TLB entries matching the specified ASID across the
 * Inner Shareable domain, leaving cached entries of other domains undisturbed.
 */
void lettuce_mmu_invalidate_asid(uint16_t asid)
{
    const uint64_t asid_arg = (uint64_t)asid << 48;
    if (lettuce_arch_current_el() == 2u)
    {
        __asm__ __volatile__(
            "dsb ishst\n\t"
            "tlbi alle2is\n\t"
            "dsb ish\n\t"
            "isb"
            ::: "memory"
        );
    }
    else
    {
        __asm__ __volatile__(
            "dsb ishst\n\t"
            "tlbi aside1is, %0\n\t"
            "dsb ish\n\t"
            "isb"
            :
            : "r"(asid_arg)
            : "memory"
        );
    }
}

void lettuce_mmu_invalidate_domain(LettuceDomainId domain)
{
    const LettuceArchDomain *arch_dom = arch_domain_lookup(domain);
    if (arch_dom != NULL)
    {
        lettuce_mmu_invalidate_asid(arch_dom->asid);
    }
}

void lettuce_mmu_invalidate_all(void)
{
    if (lettuce_arch_current_el() == 2u)
    {
        __asm__ __volatile__(
            "dsb ishst\n\t"
            "tlbi alle2is\n\t"
            "dsb ish\n\t"
            "isb"
            ::: "memory"
        );
    }
    else
    {
        __asm__ __volatile__(
            "dsb ishst\n\t"
            "tlbi vmalle1is\n\t"
            "dsb ish\n\t"
            "isb"
            ::: "memory"
        );
    }
}

uint16_t lettuce_mmu_active_asid(void)
{
    uint64_t ttbr0;
    if (lettuce_arch_current_el() == 2u)
        __asm__ __volatile__("mrs %0, ttbr0_el2" : "=r"(ttbr0));
    else
        __asm__ __volatile__("mrs %0, ttbr0_el1" : "=r"(ttbr0));
    return (uint16_t)(ttbr0 >> 48);
}

uint64_t lettuce_mmu_active_ttbr0(void)
{
    uint64_t ttbr0;
    if (lettuce_arch_current_el() == 2u)
        __asm__ __volatile__("mrs %0, ttbr0_el2" : "=r"(ttbr0));
    else
        __asm__ __volatile__("mrs %0, ttbr0_el1" : "=r"(ttbr0));
    return ttbr0;
}

void lettuce_mmu_init(void)
{
    /* Initialize shared kernel device L2 table (UART at 0x09000000) */
    for (uint32_t i = 0; i < TABLE_ENTRIES; ++i)
        kernel_l2_device[i] = 0;
    kernel_l2_device[(GIC_PA >> 21u) & 0x1ffu] = device_block_descriptor(GIC_PA);
    kernel_l2_device[(UART_PA >> 21u) & 0x1ffu] = device_block_descriptor(UART_PA);

    /* Build domain tables */
    init_domain_tables(domain_a_l1, domain_a_l2, domain_a_l3, PRIVATE_A_VA, PRIVATE_A_PA, DOMAIN_A_STACK_VA, (uintptr_t)domain_a_stack_page, true);
    init_domain_tables(domain_b_l1, domain_b_l2, domain_b_l3, PRIVATE_B_VA, PRIVATE_B_PA, DOMAIN_B_STACK_VA, (uintptr_t)domain_b_stack_page, false);
    init_domain_tables(domain_c_l1, domain_c_l2, domain_c_l3, PRIVATE_C_VA, PRIVATE_C_PA, DOMAIN_C_STACK_VA, (uintptr_t)domain_c_stack_page, false);

    /* Zero physical pages before enabling translation */
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); ++i)
    {
        ((volatile uint64_t *)domain_a_private_page)[i] = 0;
        ((volatile uint64_t *)domain_a_stack_page)[i] = 0;
        ((volatile uint64_t *)domain_b_private_page)[i] = 0;
        ((volatile uint64_t *)domain_b_stack_page)[i] = 0;
        ((volatile uint64_t *)domain_c_private_page)[i] = 0;
        ((volatile uint64_t *)domain_c_stack_page)[i] = 0;
        ((volatile uint64_t *)shared_buffer_page)[i] = 0;
        ((volatile uint64_t *)ro_test_page)[i] = 0;
    }

    /*
     * MAIR (Memory Attribute Indirection Register):
     * Attr0 (bits 7:0)   = 0xFF -> Normal Memory, Inner & Outer Write-Back Read/Write Allocate
     * Attr1 (bits 15:8)  = 0x00 -> Device-nGnRnE Memory (strict device ordering)
     */
    const uint64_t mair = UINT64_C(0xff) | (UINT64_C(0x00) << 8);

    /*
     * TCR (Translation Control Register):
     * T0SZ = 25 (39-bit VA space for TTBR0, Level 1 root)
     * IRGN0 = 01 (Inner Write-Back Read-Allocate Write-Allocate Cacheable)
     * ORGN0 = 01 (Outer Write-Back Read-Allocate Write-Allocate Cacheable)
     * SH0 = 11 (Inner Shareable)
     * TG0 = 00 (4KB translation granule)
     * EPD1 = 1 (Disable TTBR1 walks at EL1)
     * IPS = 010 (40-bit Intermediate Physical Address size, 1TB)
     * AS = 1 (16-bit ASID)
     */
    const uint64_t tcr_el1 = (UINT64_C(25) << 0) |
                             (UINT64_C(1) << 8) |
                             (UINT64_C(1) << 10) |
                             (UINT64_C(3) << 12) |
                             (UINT64_C(0) << 14) |
                             (UINT64_C(1) << 23) |
                             (UINT64_C(2) << 32) |
                             (UINT64_C(1) << 36);

    const uint64_t tcr_el2 = (UINT64_C(25) << 0) |
                             (UINT64_C(1) << 8) |
                             (UINT64_C(1) << 10) |
                             (UINT64_C(3) << 12) |
                             (UINT64_C(0) << 14) |
                             (UINT64_C(2) << 16) |
                             (UINT64_C(1) << 23) |
                             (UINT64_C(1) << 31);

    if (lettuce_arch_current_el() == 2u)
    {
        __asm__ __volatile__(
            "msr mair_el2, %0\n\t"
            "msr tcr_el2, %1\n\t"
            "isb"
            :
            : "r"(mair), "r"(tcr_el2)
            : "memory"
        );
    }
    else
    {
        __asm__ __volatile__(
            "msr mair_el1, %0\n\t"
            "msr tcr_el1, %1\n\t"
            "isb"
            :
            : "r"(mair), "r"(tcr_el1)
            : "memory"
        );
    }

    /* Precompute immutable ttbr0_val for all domains to avoid repeated runtime calculation */
    for (size_t i = 0; i < ARCH_DOMAIN_COUNT; ++i)
    {
        arch_domains[i].ttbr0_val = ((uint64_t)arch_domains[i].asid << 48) |
                                    (((uint64_t)(uintptr_t)arch_domains[i].l1_root) & 0x0000FFFFFFFFF000ULL);
    }

    /* One-time cold boot TLB invalidation to clear any bootloader/reset cache entries */
    lettuce_mmu_invalidate_all();

    /* Install Domain A root table with ASID 1 */
    write_ttbr(arch_domains[0].ttbr0_val);

    /*
     * Enable MMU via SCTLR_ELx.M (bit 0):
     * Read SCTLR, set bit 0 (MMU enable), write SCTLR, execute ISB.
     */
    uint64_t sctlr;
    if (lettuce_arch_current_el() == 2u)
    {
        __asm__ __volatile__("mrs %0, sctlr_el2" : "=r"(sctlr));
        sctlr |= UINT64_C(1);
        __asm__ __volatile__("msr sctlr_el2, %0\n\tisb" : : "r"(sctlr) : "memory");
    }
    else
    {
        __asm__ __volatile__("mrs %0, sctlr_el1" : "=r"(sctlr));
        sctlr |= UINT64_C(1);
        __asm__ __volatile__("msr sctlr_el1, %0\n\tisb" : : "r"(sctlr) : "memory");
    }

    current_domain = DOMAIN_A;
}

LettuceDomainId lettuce_mmu_enter(LettuceDomainId domain)
{
    const LettuceDomainId previous = current_domain;
    if (domain == previous || domain == LETTUCE_DOMAIN_ID_INVALID)
        return previous;

    const LettuceArchDomain *target = arch_domain_lookup(domain);
    if (target == NULL)
        return previous;

    write_ttbr(target->ttbr0_val);
    current_domain = domain;
    return previous;
}

void lettuce_mmu_leave(LettuceDomainId previous_domain)
{
    (void)lettuce_mmu_enter(previous_domain);
}

LettuceDomainId lettuce_mmu_current_domain(void)
{
    return current_domain;
}

void lettuce_mmu_set_current_domain(LettuceDomainId domain)
{
    current_domain = domain;
}

void lettuce_mmu_expect_data_abort(void)
{
    g_expecting_fault = true;
    g_fault_observed = false;
    g_expected_far = 0;
    g_last_fault_far = 0;
    g_last_fault_esr = 0;
    g_last_fault_elr = 0;
}

void lettuce_mmu_expect_fault(uint64_t expected_far)
{
    g_expecting_fault = true;
    g_fault_observed = false;
    g_expected_far = expected_far;
    g_last_fault_far = 0;
    g_last_fault_esr = 0;
    g_last_fault_elr = 0;
}

bool lettuce_mmu_expected_fault_observed(void)
{
    return g_fault_observed;
}

uint64_t lettuce_mmu_last_fault_far(void)
{
    return g_last_fault_far;
}

uint64_t lettuce_mmu_last_fault_esr(void)
{
    return g_last_fault_esr;
}

uint64_t lettuce_mmu_last_fault_elr(void)
{
    return g_last_fault_elr;
}

bool lettuce_mmu_handle_expected_data_abort(void)
{
    if (!g_expecting_fault)
        return false;
    g_expecting_fault = false;
    g_fault_observed = true;
    return true;
}

bool lettuce_mmu_handle_fault(uint64_t esr, uint64_t far, uint64_t elr)
{
    g_last_fault_esr = esr;
    g_last_fault_far = far;
    g_last_fault_elr = elr;

    if (!g_expecting_fault)
        return false;

    g_expecting_fault = false;
    g_fault_observed = true;
    return true;
}

void lettuce_arch_expect_sysreg_trap(void)
{
    g_expecting_sysreg_trap = true;
    g_sysreg_trap_observed = false;
}

bool lettuce_arch_sysreg_trap_observed(void)
{
    return g_sysreg_trap_observed;
}

bool lettuce_arch_handle_sysreg_trap(uint64_t esr, uint64_t elr)
{
    g_last_fault_esr = esr;
    g_last_fault_elr = elr;

    if (!g_expecting_sysreg_trap)
        return false;

    g_expecting_sysreg_trap = false;
    g_sysreg_trap_observed = true;
    return true;
}
