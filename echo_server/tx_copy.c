#include "shared_ringbuffer.h"
#include "util.h"
#include "fence.h"
#include "cache.h"
#include <string.h>
#include <stdbool.h>

uintptr_t tx_free_mux;
uintptr_t tx_used_mux;

uintptr_t tx_free_cli;
uintptr_t tx_used_cli;

uintptr_t shared_dma_vaddr_mux;
uintptr_t shared_dma_vaddr_cli;
uintptr_t uart_base;

#define MUX_TX_CH 0
#define CLIENT_CH 1

#define BUF_SIZE 2048
#define NUM_BUFFERS 512
#define SHARED_DMA_SIZE (BUF_SIZE * NUM_BUFFERS)

ring_handle_t tx_ring_mux;
ring_handle_t tx_ring_cli;

void process_tx_ready(void)
{
    uint64_t enqueued = 0;
    // We only want to copy buffers if all the dequeues and enqueues will be successful
    while(true) {
        while (!ring_empty(tx_ring_cli.used_ring) &&
                !ring_empty(tx_ring_mux.free_ring) &&
                !ring_full(tx_ring_cli.free_ring) &&
                !ring_full(tx_ring_mux.used_ring)) {

            uintptr_t d_addr, s_addr = 0;
            unsigned int d_len, s_len = 0;
            void *cookie = NULL;
            void *cookie2 = NULL;
            int err;

            err = dequeue_used(&tx_ring_cli, &s_addr, &s_len, &cookie);
            assert(!err);

            if (!s_addr || 
                s_addr < shared_dma_vaddr_cli ||
                s_addr >= (shared_dma_vaddr_cli + SHARED_DMA_SIZE) ||
                s_len >= BUF_SIZE) {
                print("COPY TX|ERROR: client enqueued a strange address or length!\n");
                continue;
            }
            // check the packet header is ok. 

            // get a free one from tx mux rings queue.
            err = dequeue_free(&tx_ring_mux, &d_addr, &d_len, &cookie2);
            assert(!err);

            // copy the data over to the clients address space.
            memcpy((void *)d_addr, (void *)s_addr, s_len);

            /* Now that we've copied the data, clean the cache at the new address */
            cleanCache(d_addr, d_addr + s_len);

            /* and enqueue the buffer to the mux's used ring. */
            err = enqueue_used(&tx_ring_mux, d_addr, s_len, cookie2);
            assert(!err);
            /* enqueue the old buffer back to client so the client can use it again. */
            err = enqueue_free(&tx_ring_cli, s_addr, BUF_SIZE, cookie);
            assert(!err);

            enqueued += 1;
        }

        tx_ring_cli.used_ring->notify_reader = true;
        tx_ring_mux.free_ring->notify_reader = true;

        THREAD_MEMORY_FENCE();

        if (ring_empty(tx_ring_cli.used_ring) || ring_empty(tx_ring_mux.free_ring)) break;

        tx_ring_cli.used_ring->notify_reader = false;
        tx_ring_mux.free_ring->notify_reader = false;
    }


    if (tx_ring_mux.used_ring->notify_reader && enqueued) {
        tx_ring_mux.used_ring->notify_reader = false;
        sel4cp_notify(MUX_TX_CH);
    }

    /* We want to inform the client that more free buffers are available */
    if (enqueued && tx_ring_cli.free_ring->notify_reader) {
        tx_ring_cli.free_ring->notify_reader = false;
        sel4cp_notify_delayed(CLIENT_CH);
    }
}

void notified(sel4cp_channel ch)
{
    /* We have one job. */
    process_tx_ready();
}

void init(void)
{
    /* Set up shared memory regions */
    ring_init(&tx_ring_mux, (ring_buffer_t *)tx_free_mux, (ring_buffer_t *)tx_used_mux, 0, NUM_BUFFERS, NUM_BUFFERS);
    ring_init(&tx_ring_cli, (ring_buffer_t *)tx_free_cli, (ring_buffer_t *)tx_used_cli, 1, NUM_BUFFERS, NUM_BUFFERS);
    // ensure we get notified when a packet comes in
    tx_ring_cli.used_ring->notify_reader = true;

    /* Enqueue buffers for the client to use */
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        uintptr_t addr = shared_dma_vaddr_cli + (BUF_SIZE * i);
        int err = enqueue_free(&tx_ring_cli, addr, BUF_SIZE, NULL);
        assert(!err);
        _unused(err);
    }

    return;
}
