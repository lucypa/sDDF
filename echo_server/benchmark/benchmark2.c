/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <sel4/benchmark_track_types.h>
#include <sel4/benchmark_utilisation_types.h>
#include "sel4bench.h"
#include "fence.h"
#include "bench.h"
#include "util.h"
#include "utilisation_benchmark.h"

#define START 1
#define STOP 2

#define PD_MUX_TX_ID    3
#define PD_ETH_CLI_ID   2
uintptr_t uart_base;
ccnt_t counter_values[8];
counter_bitfield_t benchmark_bf;

char *counter_names[] = {
    "L1 i-cache misses",
    "L1 d-cache misses",
    "L1 i-tlb misses",
    "L1 d-tlb misses",
    "Instructions",
    "Branch mispredictions",
};

event_id_t benchmarking_events[] = {
    SEL4BENCH_EVENT_CACHE_L1I_MISS,
    SEL4BENCH_EVENT_CACHE_L1D_MISS,
    SEL4BENCH_EVENT_TLB_L1I_MISS,
    SEL4BENCH_EVENT_TLB_L1D_MISS,
    SEL4BENCH_EVENT_EXECUTE_INSTRUCTION,
    SEL4BENCH_EVENT_BRANCH_MISPREDICT,
};

#ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
static void
sel4cp_benchmark_start(void)
{
    seL4_BenchmarkResetThreadUtilisation(TCB_CAP);
    seL4_BenchmarkResetThreadUtilisation(BASE_TCB_CAP + PD_MUX_TX_ID);
    seL4_BenchmarkResetThreadUtilisation(BASE_TCB_CAP + PD_ETH_CLI_ID);
    seL4_BenchmarkResetLog();
}

static void
print_benchmark_details(uint64_t pd_id, uint64_t kernel_util, uint64_t kernel_entries, uint64_t number_schedules, uint64_t total_util)
{
    print("Utilisation details for PD: ");
    switch (pd_id) {
        case PD_MUX_TX_ID: print("TX MUX"); break;
        case PD_ETH_CLI_ID: print("ETH CLIENT"); break;
        default: print("CORE 2 TOTALS");
    }
    print(" (");
    puthex64(pd_id);
    print(")\n");
    print("KernelUtilisation");
    print(": ");
    puthex64(kernel_util);
    print("\n");
    print("KernelEntries");
    print(": ");
    puthex64(kernel_entries);
    print("\n");
    print("NumberSchedules: ");
    puthex64(number_schedules);
    print("\n");
    print("TotalUtilisation: ");
    puthex64(total_util);
    print("\n");
}
#endif

void notified(sel4cp_channel ch)
{
    uint64_t total;
    uint64_t kernel;
    uint64_t entries;
    uint64_t idle;
    uint64_t number_schedules;
    switch(ch) {
        case START:
            sel4bench_reset_counters();
            THREAD_MEMORY_RELEASE();
            sel4bench_start_counters(benchmark_bf);

            #ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
            sel4cp_benchmark_start();
            #endif

            break;
        case STOP:
            sel4bench_get_counters(benchmark_bf, &counter_values[0]);
            sel4bench_stop_counters(benchmark_bf);

            print("{CORE 2: \n");
            for (int i = 0; i < ARRAY_SIZE(benchmarking_events); i++) {
                print(counter_names[i]);
                print(": ");
                puthex64(counter_values[i]);
                print("\n");
            }
            print("}\n");

            #ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
            sel4cp_benchmark_stop(&total, &idle, &kernel, &entries);
            print_benchmark_details(TCB_CAP, kernel, entries, idle, total);
            
            sel4cp_benchmark_stop_tcb(PD_MUX_TX_ID, &total, &number_schedules, &kernel, &entries);
            print_benchmark_details(PD_MUX_TX_ID, kernel, entries, number_schedules, total);

            sel4cp_benchmark_stop_tcb(PD_ETH_CLI_ID, &total, &number_schedules, &kernel, &entries);
            print_benchmark_details(PD_ETH_CLI_ID, kernel, entries, number_schedules, total);
            #endif

            break;
        default:
            print("Bench thread notified on unexpected channel\n");
    }
}

void init(void)
{
    sel4bench_init();
    seL4_Word n_counters = sel4bench_get_num_counters();

    counter_bitfield_t mask = 0;

    for (seL4_Word i = 0; i < n_counters; i++) {
        seL4_Word counter = i;
        if (counter >= ARRAY_SIZE(benchmarking_events)) {
            break;
        }
        sel4bench_set_count_event(i, benchmarking_events[counter]);
        mask |= BIT(i);
    }

    sel4bench_reset_counters();
    sel4bench_start_counters(mask);

    benchmark_bf = mask;
}