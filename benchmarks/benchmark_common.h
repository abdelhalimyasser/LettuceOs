/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: benchmarks/benchmark_common.h
 *
 * Purpose:
 *   Provides the host-side sampling, percentile, affinity, and CSV-reporting
 *   helpers shared by Lettuce microbenchmarks.
 *
 * Design:
 *   Measurements use CLOCK_MONOTONIC_RAW and report nanoseconds; this harness
 *   does not characterize QEMU Generic Counter ticks or physical ARM64 cycles.
 */

#ifndef LETTUCE_BENCHMARK_COMMON_H
#define LETTUCE_BENCHMARK_COMMON_H

#if defined(__linux__)
#define _GNU_SOURCE
#include <sched.h>
#endif

#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LETTUCE_BENCH_SAMPLE_COUNT 100u
#define LETTUCE_BENCH_CALLS_PER_SAMPLE 10000u
#define LETTUCE_BENCH_WARMUP_CALLS 100000u

#if defined(__GNUC__) || defined(__clang__)
#define LETTUCE_BENCH_NOINLINE __attribute__((noinline))
#define LETTUCE_BENCH_UNUSED __attribute__((unused))
#else
#define LETTUCE_BENCH_NOINLINE
#define LETTUCE_BENCH_UNUSED
#endif

static volatile uint64_t lettuce_benchmark_sink;

static LETTUCE_BENCH_NOINLINE LETTUCE_BENCH_UNUSED void lettuce_benchmark_target_work(void)
{
    lettuce_benchmark_sink = (lettuce_benchmark_sink * UINT64_C(0x100000001b3)) ^ UINT64_C(0x9e3779b97f4a7c15);
}

typedef void (*LettuceBenchmarkBatch)(uint64_t calls, void *context);

typedef struct LettuceBenchmarkStats
{
    double mean_ns;
    double standard_deviation_ns;
    uint64_t p50_ns;
    uint64_t p95_ns;
    uint64_t p99_ns;
    uint64_t minimum_ns;
    uint64_t maximum_ns;
    uint64_t timer_overhead_ns;
    bool affinity_requested;
    bool affinity_used;
} LettuceBenchmarkStats;

static uint64_t lettuce_benchmark_now_ns(void)
{
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC_RAW, &timestamp);
    return (uint64_t)timestamp.tv_sec * UINT64_C(1000000000) + (uint64_t)timestamp.tv_nsec;
}

static int lettuce_benchmark_compare_u64(const void *left, const void *right)
{
    const uint64_t left_value = *(const uint64_t *)left;
    const uint64_t right_value = *(const uint64_t *)right;
    return left_value > right_value ? 1 : left_value < right_value ? -1 : 0;
}

static uint64_t lettuce_benchmark_percentile(const uint64_t *sorted, size_t count, size_t percentile)
{
    const size_t index = (count * percentile + 99u) / 100u;
    return sorted[index == 0u ? 0u : index - 1u];
}

static bool lettuce_benchmark_try_affinity(void)
{
#if defined(__linux__)
    const char *requested_cpu = getenv("LETTUCE_BENCH_CPU");
    if (requested_cpu == NULL)
        return false;

    char *end = NULL;
    const unsigned long cpu = strtoul(requested_cpu, &end, 10);
    if (*requested_cpu == '\0' || *end != '\0')
        return false;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return sched_setaffinity(0, sizeof(set), &set) == 0;
#else
    return false;
#endif
}

static LettuceBenchmarkStats lettuce_benchmark_run(LettuceBenchmarkBatch batch, void *context)
{
    const bool affinity_requested = getenv("LETTUCE_BENCH_CPU") != NULL;
    const bool affinity_used = lettuce_benchmark_try_affinity();
    uint64_t samples[LETTUCE_BENCH_SAMPLE_COUNT];

    batch(LETTUCE_BENCH_WARMUP_CALLS, context);

    const uint64_t overhead_start = lettuce_benchmark_now_ns();
    for (size_t i = 0; i < LETTUCE_BENCH_SAMPLE_COUNT; ++i)
    {
        const uint64_t start = lettuce_benchmark_now_ns();
        const uint64_t end = lettuce_benchmark_now_ns();
        (void)start;
        (void)end;
    }
    const uint64_t overhead_elapsed = lettuce_benchmark_now_ns() - overhead_start;
    const uint64_t timer_overhead_ns = overhead_elapsed / LETTUCE_BENCH_SAMPLE_COUNT;

    for (size_t sample = 0; sample < LETTUCE_BENCH_SAMPLE_COUNT; ++sample)
    {
        const uint64_t start = lettuce_benchmark_now_ns();
        batch(LETTUCE_BENCH_CALLS_PER_SAMPLE, context);
        samples[sample] = lettuce_benchmark_now_ns() - start;
    }

    uint64_t sorted[LETTUCE_BENCH_SAMPLE_COUNT];
    memcpy(sorted, samples, sizeof(sorted));
    qsort(sorted, LETTUCE_BENCH_SAMPLE_COUNT, sizeof(sorted[0]), lettuce_benchmark_compare_u64);

    double sum = 0.0;
    for (size_t i = 0; i < LETTUCE_BENCH_SAMPLE_COUNT; ++i)
        sum += (double)samples[i] / (double)LETTUCE_BENCH_CALLS_PER_SAMPLE;
    const double mean = sum / (double)LETTUCE_BENCH_SAMPLE_COUNT;
    double variance = 0.0;
    for (size_t i = 0; i < LETTUCE_BENCH_SAMPLE_COUNT; ++i)
    {
        const double sample_ns = (double)samples[i] / (double)LETTUCE_BENCH_CALLS_PER_SAMPLE;
        const double difference = sample_ns - mean;
        variance += difference * difference;
    }

    LettuceBenchmarkStats stats = {
        .mean_ns = mean,
        .standard_deviation_ns = sqrt(variance / (double)LETTUCE_BENCH_SAMPLE_COUNT),
        .p50_ns = lettuce_benchmark_percentile(sorted, LETTUCE_BENCH_SAMPLE_COUNT, 50u) / LETTUCE_BENCH_CALLS_PER_SAMPLE,
        .p95_ns = lettuce_benchmark_percentile(sorted, LETTUCE_BENCH_SAMPLE_COUNT, 95u) / LETTUCE_BENCH_CALLS_PER_SAMPLE,
        .p99_ns = lettuce_benchmark_percentile(sorted, LETTUCE_BENCH_SAMPLE_COUNT, 99u) / LETTUCE_BENCH_CALLS_PER_SAMPLE,
        .minimum_ns = sorted[0] / LETTUCE_BENCH_CALLS_PER_SAMPLE,
        .maximum_ns = sorted[LETTUCE_BENCH_SAMPLE_COUNT - 1u] / LETTUCE_BENCH_CALLS_PER_SAMPLE,
        .timer_overhead_ns = timer_overhead_ns,
        .affinity_requested = affinity_requested,
        .affinity_used = affinity_used
    };
    return stats;
}

static void lettuce_benchmark_print(const char *name, LettuceBenchmarkStats stats)
{
#if defined(__x86_64__)
    const char *architecture = "x86_64";
#elif defined(__aarch64__)
    const char *architecture = "aarch64";
#else
    const char *architecture = "unknown";
#endif
#if defined(__OPTIMIZE__)
    const char *optimization = "enabled";
#else
    const char *optimization = "disabled";
#endif

    const uint64_t total_calls = (uint64_t)LETTUCE_BENCH_SAMPLE_COUNT * LETTUCE_BENCH_CALLS_PER_SAMPLE;
    printf("benchmark=%s\n", name);
    printf("compiler=%s\n", __VERSION__);
    printf("architecture=%s\n", architecture);
    printf("optimization=%s\n", optimization);
    printf("samples=%u\n", LETTUCE_BENCH_SAMPLE_COUNT);
    printf("calls_per_sample=%u\n", LETTUCE_BENCH_CALLS_PER_SAMPLE);
    printf("warmup_calls=%u\n", LETTUCE_BENCH_WARMUP_CALLS);
    printf("total_calls=%" PRIu64 "\n", total_calls);
    printf("timer=clock_monotonic_raw\n");
    printf("timer_overhead_ns=%" PRIu64 "\n", stats.timer_overhead_ns);
    printf("mean_ns=%.3f\n", stats.mean_ns);
    printf("p50_ns=%" PRIu64 "\n", stats.p50_ns);
    printf("p95_ns=%" PRIu64 "\n", stats.p95_ns);
    printf("p99_ns=%" PRIu64 "\n", stats.p99_ns);
    printf("min_ns=%" PRIu64 "\n", stats.minimum_ns);
    printf("max_ns=%" PRIu64 "\n", stats.maximum_ns);
    printf("stddev_ns=%.3f\n", stats.standard_deviation_ns);
    printf("ops_per_sec_p50=%.3f\n", stats.p50_ns == 0u ? 0.0 : 1e9 / (double)stats.p50_ns);
    printf("affinity_requested=%s\n", stats.affinity_requested ? "true" : "false");
    printf("affinity_used=%s\n", stats.affinity_used ? "true" : "false");
    printf("cycles_per_call=not_collected\n\n");
}

static LETTUCE_BENCH_UNUSED void lettuce_benchmark_print_csv(const char *name, LettuceBenchmarkStats stats)
{
    printf("%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.3f,%" PRIu64 ",%" PRIu64 "\n",
           name, stats.p50_ns, stats.p95_ns, stats.p99_ns,
           stats.mean_ns, stats.minimum_ns, stats.maximum_ns);
}

static inline LETTUCE_BENCH_UNUSED bool lettuce_benchmark_has_csv_flag(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--csv") == 0)
            return true;
    }
    return false;
}

#endif
