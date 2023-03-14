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

#include "logbuffer_eth.h"

#define TX_COUNT 256

#define TX_CH 1

/* Memory regions. These all have to be here to keep compiler happy */
uintptr_t hw_ring_buffer_vaddr;
uintptr_t hw_ring_buffer_paddr;
uintptr_t shared_dma_vaddr_rx;
uintptr_t shared_dma_paddr_rx;
uintptr_t shared_dma_vaddr_tx;
uintptr_t shared_dma_paddr_tx;
uintptr_t tx_cookies;
uintptr_t tx_avail;
uintptr_t tx_used;
uintptr_t uart_base;

struct descriptor {
    uint16_t len;
    uint16_t stat;
    uint32_t addr;
};

typedef struct {
    unsigned int cnt;
    unsigned int remain;
    unsigned int tail;
    unsigned int head;
    volatile struct descriptor *descr;
    uintptr_t phys;
    void **cookies;
    unsigned int tx_lengths[TX_COUNT];
} ring_ctx_t;

ring_ctx_t *tx = (void *)(uintptr_t)0x5400000;

/* Pointers to shared_ringbuffers */
ring_handle_t tx_ring;

volatile struct enet_regs *eth = (void *)(uintptr_t)0x2000000;

static uintptr_t
get_phys_addr(uintptr_t virtual, int tx)
{
    uint64_t offset;
    uintptr_t phys;
    if (tx) {
        offset = virtual - shared_dma_vaddr_tx;
    } else {
        offset = virtual - shared_dma_vaddr_rx;
    }

    if (offset < 0) {
        print("get_phys_addr: offset < 0");
        return 0;
    }

    if (tx) {
        phys = shared_dma_paddr_tx + offset;
    } else {
        phys = shared_dma_paddr_rx + offset;
    }

    return phys;
}

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
raw_tx(volatile struct enet_regs *eth, unsigned int num, uintptr_t *phys,
                  unsigned int *len, void *cookie)
{
    ring_ctx_t *ring = tx;

    /* Ensure we have room */
    if (ring->remain < num) {
        /* not enough room, try to complete some and check again */
        return;
    }

    __sync_synchronize();

    unsigned int tail = ring->tail;
    unsigned int tail_new = tail;

    unsigned int i = num;
    while (i-- > 0) {
        uint16_t stat = TXD_READY;
        if (0 == i) {
            stat |= TXD_ADDCRC | TXD_LAST;
        }

        unsigned int idx = tail_new;
        if (++tail_new == TX_COUNT) {
            tail_new = 0;
            stat |= WRAP;
        }
        update_ring_slot(ring, idx, *phys++, *len++, stat);
    }

    ring->cookies[tail] = cookie;
    ring->tx_lengths[tail] = num;
    __sync_synchronize();
    ring->tail = tail_new;
    /* There is a race condition here if add/remove is not synchronized. */
    ring->remain -= num;

    if (!(eth->tdar & TDAR_TDAR)) {
        eth->tdar = TDAR_TDAR;
    }
}

static void
handle_tx(volatile struct enet_regs *eth, sel4cp_channel ch)
{
    uintptr_t buffer = 0;
    unsigned int len = 0;
    void *cookie = NULL;
    uint64_t num = 0;

    // We need to put in an empty condition here.
    if ((tx->remain > 1) && !driver_dequeue(tx_ring.used_ring, &buffer, &len, &cookie)) {
        uintptr_t phys = get_phys_addr(buffer, 1);
        raw_tx(eth, 1, &phys, &len, cookie);
        num++;
    }

    new_log_buffer_entry_tx_used(num, ch, tx->remain, 0,
                                    ring_size(tx_ring.avail_ring),
                                    ring_size(tx_ring.used_ring));
}

void notified(sel4cp_channel ch)
{
    switch(ch) {
        case TX_CH:
            handle_tx(eth, TX_CH);
            break;
        default:
            sel4cp_dbg_puts("eth2 driver: received notification on unexpected channel\n");
            break;
    }
}

void init(void)
{
    sel4cp_dbg_puts(sel4cp_name);
    sel4cp_dbg_puts(": elf PD init function running\n");

    ring_init(&tx_ring, (ring_buffer_t *)tx_avail, (ring_buffer_t *)tx_used, NULL, 0);
}
