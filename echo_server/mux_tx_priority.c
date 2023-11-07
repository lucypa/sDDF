#include "shared_ringbuffer.h"
#include "util.h"

uintptr_t tx_free_drv;
uintptr_t tx_used_drv;
uintptr_t tx_free_cli0;
uintptr_t tx_used_cli0;
uintptr_t tx_free_cli1;
uintptr_t tx_used_cli1;
uintptr_t tx_free_arp;
uintptr_t tx_used_arp;

uintptr_t shared_dma_vaddr_cli0;
uintptr_t shared_dma_paddr_cli0;
uintptr_t shared_dma_vaddr_cli1;
uintptr_t shared_dma_paddr_cli1;
uintptr_t shared_dma_vaddr_arp;
uintptr_t shared_dma_paddr_arp;
uintptr_t uart_base;

#define CLIENT_0 0
#define CLIENT_1 1
#define ARP 2
#define NUM_CLIENTS 3
#define DRIVER_SEND 5
#define DRIVER_RECV 5
#define NUM_BUFFERS 512
#define BUF_SIZE 2048
#define DMA_SIZE 0x200000

typedef struct state {
    /* Pointers to shared buffers */
    ring_handle_t tx_ring_drv;
    ring_handle_t tx_ring_clients[NUM_CLIENTS];
    int client_priority_order[NUM_CLIENTS];
} state_t;

state_t state;

static uintptr_t
get_phys_addr(uintptr_t virtual)
{
    int offset;
    uintptr_t base;
    if (virtual >= shared_dma_vaddr_cli0 && virtual < shared_dma_vaddr_cli0 + DMA_SIZE) {
        offset = virtual - shared_dma_vaddr_cli0;
        base = shared_dma_paddr_cli0;
    } else if (virtual >= shared_dma_vaddr_cli1 && virtual < shared_dma_vaddr_cli1 + DMA_SIZE) {
        offset = virtual - shared_dma_vaddr_cli1;
        base = shared_dma_paddr_cli1;
    } else if (virtual >= shared_dma_vaddr_arp && virtual < shared_dma_vaddr_arp + DMA_SIZE) {
        offset = virtual - shared_dma_vaddr_arp;
        base = shared_dma_paddr_arp;
    } else {
        print("MUX TX|ERROR: get_phys_addr: invalid virtual address\n");
        return 0;
    }

    return base + offset;
}

static uintptr_t
get_virt_addr(uintptr_t phys)
{
    int offset;
    uintptr_t base; 
    if (phys >= shared_dma_paddr_cli0 && phys < shared_dma_paddr_cli0 + DMA_SIZE) {
        offset = phys - shared_dma_paddr_cli0;
        base = shared_dma_vaddr_cli0;
    } else if (phys >= shared_dma_paddr_cli1 && phys < shared_dma_paddr_cli1 + DMA_SIZE) {
        offset = phys - shared_dma_paddr_cli1;
        base = shared_dma_vaddr_cli1;
    } else if (phys >= shared_dma_paddr_arp && phys < shared_dma_paddr_arp + DMA_SIZE) {
        offset = phys - shared_dma_paddr_arp;
        base = shared_dma_vaddr_arp;
    } else {
        print("MUX TX|ERROR: get_virt_addr: invalid physical address\n");
        return 0;
    }

    return base + offset;
}

static int
get_client(uintptr_t addr)
{
    if (addr >= shared_dma_vaddr_cli0 && addr < shared_dma_vaddr_cli0 + DMA_SIZE) {
        return CLIENT_0;
    } else if (addr >= shared_dma_vaddr_cli1 && addr < shared_dma_vaddr_cli1 + DMA_SIZE) {
        return CLIENT_1; 
    }else if (addr >= shared_dma_vaddr_arp && addr < shared_dma_vaddr_arp + DMA_SIZE) {
        return ARP;
    }
    print("MUX TX|ERROR: Buffer out of range\n");
    assert(0);
}

/*
 * Loop over all used tx buffers in client queues and enqueue to driver.
 */
void process_tx_ready(void)
{
    uint64_t enqueued = 0;
    uint64_t old_enqueued = enqueued;
    int err;
    int client;

    /* 
     * Loop through clients in their priority ordering.
     * We need to check the drivers used ring here because
     * the queue could be full so we don't run a higher prio
     * component but once we get to a lower prio component
     * the driver could've interrupted in the mean time
     * and emptied it's queue. This would mean the lower
     * priority component might end up taking precendence
     * which we don't want.
     */
    while(!ring_full(state.tx_ring_drv.used_ring)) {
        old_enqueued = enqueued;

        for (int prio = 0; prio < NUM_CLIENTS && !ring_full(state.tx_ring_drv.used_ring); prio++) {
            client = state.client_priority_order[prio];
            // Process all queued packets for this client. 
            while (!ring_empty(state.tx_ring_clients[client].used_ring) && !ring_full(state.tx_ring_drv.used_ring)) {
                uintptr_t addr;
                unsigned int len;
                void *cookie;
                uintptr_t phys;

                err = dequeue_used(&state.tx_ring_clients[client], &addr, &len, &cookie);
                assert(!err);
                phys = get_phys_addr(addr);
                assert(phys);
                err = enqueue_used(&state.tx_ring_drv, phys, len, cookie);
                assert(!err);

                enqueued += 1;
            }

            state.tx_ring_clients[client].used_ring->notify_reader = true;
        }

        // we haven't processed any packets since last loop, exit.
        if (old_enqueued == enqueued) break;
    }

    if (state.tx_ring_drv.used_ring->notify_reader && enqueued) {
        state.tx_ring_drv.used_ring->notify_reader = false;
        sel4cp_notify_delayed(DRIVER_SEND);
    }
}

/*
 * Take as many TX free buffers from the driver and give them to
 * the respective clients. This will notify the clients if we have moved buffers
 * around and the client's TX free ring was empty.
 */
void process_tx_complete(void)
{
    // bitmap stores whether which clients need notifying.
    bool notify_clients[NUM_CLIENTS] = {false};
    while (true) {
        while (!ring_empty(state.tx_ring_drv.free_ring)) {
            uintptr_t addr;
            unsigned int len;
            void *cookie;
            int err = dequeue_free(&state.tx_ring_drv, &addr, &len, &cookie);
            assert(!err);
            uintptr_t virt = get_virt_addr(addr);
            assert(virt);

            int client = get_client(virt);
            err = enqueue_free(&state.tx_ring_clients[client], virt, len, cookie);
            assert(!err);

            if (state.tx_ring_clients[client].free_ring->notify_reader) {
                notify_clients[client] = true;
            }
        }

        state.tx_ring_drv.free_ring->notify_reader = true;

        THREAD_MEMORY_FENCE();

        if (ring_empty(state.tx_ring_drv.free_ring)) break;

        state.tx_ring_drv.free_ring->notify_reader = false;
    }

    /* Loop over bitmap and see who we need to notify. */
    for (int client = 0; client < NUM_CLIENTS; client++) {
        if (notify_clients[client]) {
            state.tx_ring_clients[client].free_ring->notify_reader = false;
            sel4cp_notify(client);
        }
    }
}

void notified(sel4cp_channel ch)
{
    process_tx_complete();
    process_tx_ready();
}


void init(void)
{
    /* Set up shared memory regions */
    ring_init(&state.tx_ring_drv, (ring_buffer_t *)tx_free_drv, (ring_buffer_t *)tx_used_drv, 1, NUM_BUFFERS, NUM_BUFFERS);
    ring_init(&state.tx_ring_clients[0], (ring_buffer_t *)tx_free_cli0, (ring_buffer_t *)tx_used_cli0, 1, NUM_BUFFERS, NUM_BUFFERS);
    ring_init(&state.tx_ring_clients[1], (ring_buffer_t *)tx_free_cli1, (ring_buffer_t *)tx_used_cli1, 1, NUM_BUFFERS, NUM_BUFFERS);
    ring_init(&state.tx_ring_clients[2], (ring_buffer_t *)tx_free_arp, (ring_buffer_t *)tx_used_arp, 1, NUM_BUFFERS, NUM_BUFFERS);

    /* Enqueue free transmit buffers to all clients. */
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        uintptr_t addr = shared_dma_vaddr_cli0 + (BUF_SIZE * i);
        int err = enqueue_free(&state.tx_ring_clients[0], addr, BUF_SIZE, NULL);
        assert(!err);
    }

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        uintptr_t addr = shared_dma_vaddr_cli1 + (BUF_SIZE * i);
        int err = enqueue_free(&state.tx_ring_clients[1], addr, BUF_SIZE, NULL);
        assert(!err);
    }

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        uintptr_t addr = shared_dma_vaddr_arp + (BUF_SIZE * i);
        int err = enqueue_free(&state.tx_ring_clients[2], addr, BUF_SIZE, NULL);
        assert(!err);
    }

    // We are higher priority than the clients, so we always need to be notified
    // when a used buffer becomes available to be sent. 
    state.tx_ring_clients[0].used_ring->notify_reader = true;
    state.tx_ring_clients[1].used_ring->notify_reader = true;
    state.tx_ring_clients[2].used_ring->notify_reader = true;
    // state.tx_ring_drv.used_ring->notify_reader = true;
    state.tx_ring_drv.free_ring->notify_reader = true;

    state.client_priority_order[0] = CLIENT_0;
    state.client_priority_order[1] = CLIENT_1;
    state.client_priority_order[2] = ARP;

    return;
}
