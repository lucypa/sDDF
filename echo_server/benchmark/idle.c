/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <sel4/benchmark_track_types.h>
#include "sel4bench.h"
#include "fence.h"
#include "bench.h"
#include "util.h"

#define INIT 3
#define MAGIC_CYCLES 150
#define ULONG_MAX 0xfffffffffffffffful
#define UINT_MAX 0xfffffffful

uintptr_t cyclecounters_vaddr;
uintptr_t instructionCount_vaddr;

struct bench *b = (void *)(uintptr_t)0x5010000;

struct instr *inst = (void *)(uintptr_t)0x3000000;

void count_idle(void)
{
    b->prev = sel4bench_get_cycle_count();
    b->ccount = 0;
    b->overflows = 0;

    /*uint64_t instr_prev = (uint64_t)sel4bench_get_counter(4);
    inst->instr_overflows = 0;
    inst->instr_idle_count = 0;
    uint64_t instr_count = 0;*/

    while (1) {

        b->ts = (uint64_t)sel4bench_get_cycle_count();
        uint64_t diff;
        //uint64_t instr_diff;

        //instr_count = (uint64_t)sel4bench_get_counter(4); 

        /* Handle overflow: This thread needs to run at least 2 times
           within any ULONG_MAX cycles period to detect overflows */
        if (b->ts < b->prev) {
            diff = ULONG_MAX - b->prev + b->ts + 1;
            b->overflows++;
        } else {
            diff = b->ts - b->prev;
        }

        /*if (sel4bench_private_read_overflow() & BIT(4)) {
            instr_diff = UINT_MAX - instr_prev + instr_count + 1;
            inst->instr_overflows++;
        } else if (inst->instr_idle_count == 0) {
            sel4bench_private_read_overflow();
            instr_diff = instr_count;
        } else {
            instr_diff = instr_count - instr_prev;
        }*/

        if (diff < MAGIC_CYCLES) {
            COMPILER_MEMORY_FENCE();
            //inst->instr_idle_count += instr_diff;
            b->ccount += diff;
            COMPILER_MEMORY_FENCE();
        }

        b->prev = b->ts;
        //instr_prev = instr_count;
    }
}

void notified(sel4cp_channel ch)
{
    switch(ch) {
        case INIT: 
            // init is complete so we can start counting.
            count_idle();
            break;
        default:
            sel4cp_dbg_puts("Idle thread notified on unexpected channel\n");
    }
}

void init(void)
{
    /* Nothing to set up as benchmark.c initialises the sel4bench library for us. */
    return;
}