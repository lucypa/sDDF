/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include "eth.h"
#include "shared_ringbuffer.h"
#include "util.h"

#define TX_COUNT 256
#define TX_CH 1
#define ETH_INIT 0

/* Memory regions. These all have to be here to keep compiler happy */
uintptr_t hw_ring_buffer_vaddr;
uintptr_t hw_ring_buffer_paddr;
uintptr_t tx_cookies;
uintptr_t tx_free;
uintptr_t tx_used;
uintptr_t uart_base;

ring_ctx_t *tx = (void *)(uintptr_t)0x5200000;

/* Pointers to shared_ringbuffers */
ring_handle_t tx_ring;
bool initialised = false;

volatile struct enet_regs *eth = (void *)(uintptr_t)0x2000000;

static void update_ring_slot(
    ring_ctx_t *ring,
    unsigned int idx,
    uintptr_t phys,
    uint16_t len,
    uint16_t stat)
{
    volatile struct descriptor *d = &(ring->descr[idx]);
    d->addr = phys;
    d->len = len;

    /* Ensure all writes to the descriptor complete, before we set the flags
     * that makes hardware aware of this slot.
     */
    __sync_synchronize();

    d->stat = stat;
}

static void
raw_tx(volatile struct enet_regs *eth, uintptr_t phys,
                  unsigned int len, void *cookie)
{
    ring_ctx_t *ring = tx;

    unsigned int write = ring->write;
    unsigned int write_new = write;

    uint16_t stat = TXD_READY | TXD_ADDCRC | TXD_LAST;

    unsigned int idx = write_new;
    if (++write_new == TX_COUNT) {
        write_new = 0;
        stat |= WRAP;
    }
    update_ring_slot(ring, idx, phys, len, stat);

    ring->cookies[write] = cookie;

    THREAD_MEMORY_RELEASE();
    ring->write = write_new;

    if (!(eth->tdar & TDAR_TDAR)) {
        eth->tdar = TDAR_TDAR;
    }
}

static void
handle_tx(volatile struct enet_regs *eth)
{
    uintptr_t buffer = 0;
    unsigned int len = 0;
    void *cookie = NULL;

    tx_ring.used_ring->notify_reader = false;

    while (!(hw_ring_full(tx)) && !driver_dequeue(tx_ring.used_ring, &buffer, &len, &cookie)) {
        raw_tx(eth, buffer, len, cookie);
    }

    if (!(hw_ring_full(tx))) {
        tx_ring.used_ring->notify_reader = true;
    } else {
        tx_ring.used_ring->notify_reader = false;
    }
}

seL4_MessageInfo_t
protected(sel4cp_channel ch, sel4cp_msginfo msginfo)
{
    if (initialised) {
        handle_tx(eth);
    }

    return sel4cp_msginfo_new(0, 0);
}

void notified(sel4cp_channel ch)
{
    if (!initialised) {
        if (ch != ETH_INIT) {
            // if the irq side driver hasn't initialised yet and this
            // is a request to send, we need to ignore. 
            return;
        } else {
            // safe to send now.
            tx_ring.used_ring->notify_reader = true;
            initialised = true;
        }
    }

    // we have one job. 
    handle_tx(eth);
}

void init(void)
{
    sel4cp_dbg_puts(sel4cp_name);
    sel4cp_dbg_puts(": elf PD init function running\n");

    ring_init(&tx_ring, (ring_buffer_t *)tx_free, (ring_buffer_t *)tx_used, 0);
}