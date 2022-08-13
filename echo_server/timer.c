/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* 
 * This is a basic single client timer driver intended purely for 
 * use by the lwIP stack for TCP. 
 */

#include "timer.h"
#include "echo.h"

uintptr_t gpt_regs;

static volatile uint32_t *gpt;

static uint32_t overflow_count;

#define CR 0
#define PR 1
#define SR 2
#define IR 3
#define OCR1 4
#define OCR2 5
#define OCR3 6
#define ICR1 7
#define ICR2 8
#define CNT 9

#define LWIP_TICK_MS 10
#define NS_IN_MS 1000000ULL

int timers_initialised = 0;

static uint64_t get_ticks(void) {
    /* FIXME: If an overflow interrupt happens in the middle here we are in trouble */
    uint64_t overflow = overflow_count;
    uint32_t sr1 = gpt[SR];
    uint32_t cnt = gpt[CNT];
    uint32_t sr2 = gpt[SR];
    if ((sr2 & (1 << 5)) && (!(sr1 & (1 << 5)))) {
        /* rolled-over during - 64-bit time must be the overflow */
        cnt = gpt[CNT];
        overflow++;
    }
    return (overflow << 32) | cnt;
}

u32_t sys_now(void)
{
    if (!timers_initialised) {
        /* lwip_init() will call this when initialising its own timers,
         * but the timer is not set up at this point so just return 0 */
        return 0;
    } else {
        uint64_t time_now = get_ticks();
        return time_now / NS_IN_MS;
    }
}

void irq(sel4cp_channel ch)
{
    uint32_t sr = gpt[SR];
    gpt[SR] = sr;

    if (sr & (1 << 5)) {
        overflow_count++;
    }

    if (sr & 1) {
        gpt[IR] &= ~1;
        uint64_t abs_timeout = get_ticks() + (LWIP_TICK_MS * NS_IN_MS);
        gpt[OCR1] = abs_timeout;
        gpt[IR] |= 1;
        sys_check_timeouts();
    }
}

void gpt_init(void)
{
    gpt = (volatile uint32_t *) gpt_regs;

        uint32_t cr = (
        (1 << 9) | // Free run mode
        (1 << 6) | // Peripheral clocks
        (1) // Enable
    );

    gpt[CR] = cr;

    gpt[IR] = ( 
        (1 << 5) // rollover interrupt
    );

    // set a timer! 
    uint64_t abs_timeout = get_ticks() + (LWIP_TICK_MS * NS_IN_MS);
    gpt[OCR1] = abs_timeout;
    gpt[IR] |= 1;

    timers_initialised = 1;
}