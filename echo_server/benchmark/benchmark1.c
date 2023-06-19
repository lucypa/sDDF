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
#define NOTIFY_START 3
#define NOTIFY_STOP 4
#define PD_MUX_RX_ID    2
#define PD_COPY_0_ID    4

uintptr_t uart_base;

#ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
static void
sel4cp_benchmark_start(void)
{
    seL4_BenchmarkResetThreadUtilisation(TCB_CAP);
    seL4_BenchmarkResetThreadUtilisation(BASE_TCB_CAP + PD_MUX_RX_ID);
    seL4_BenchmarkResetThreadUtilisation(BASE_TCB_CAP + PD_COPY_0_ID);
    seL4_BenchmarkResetLog();
}

static void
print_benchmark_details(uint64_t pd_id, uint64_t kernel_util, uint64_t kernel_entries, uint64_t number_schedules, uint64_t total_util)
{
    print("Utilisation details for PD: ");
    switch (pd_id) {
        case PD_MUX_RX_ID: print("RX MUX"); break;
        case PD_COPY_0_ID: print("COPY"); break;
        default: print("CORE 1 TOTALS");
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
            #ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
            sel4cp_benchmark_start();
            #endif
            sel4cp_notify(NOTIFY_START);
            break;
        case STOP:
            #ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
            sel4cp_benchmark_stop(&total, &idle, &kernel, &entries);
            print_benchmark_details(TCB_CAP, kernel, entries, idle, total);
            
            sel4cp_benchmark_stop_tcb(PD_MUX_RX_ID, &total, &number_schedules, &kernel, &entries);
            print_benchmark_details(PD_MUX_RX_ID, kernel, entries, number_schedules, total);

            sel4cp_benchmark_stop_tcb(PD_COPY_0_ID, &total, &number_schedules, &kernel, &entries);
            print_benchmark_details(PD_COPY_0_ID, kernel, entries, number_schedules, total);
            #endif
            THREAD_MEMORY_RELEASE();
            sel4cp_notify(NOTIFY_STOP);
            break;
        default:
            print("Bench thread notified on unexpected channel\n");
    }
}

void init(void)
{
    // nothing to do. 
}