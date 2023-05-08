/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "shared_ringbuffer.h"

void ring_init(ring_handle_t *ring, ring_buffer_t *free, ring_buffer_t *used, notify_fn notify, int buffer_init)
{
    ring->free_ring = free;
    ring->used_ring = used;
    ring->notify = notify;

    if (buffer_init) {
        ring->free_ring->write_idx = 0;
        ring->free_ring->read_idx = 0;
        ring->used_ring->write_idx = 0;
        ring->used_ring->read_idx = 0;
    }
}
