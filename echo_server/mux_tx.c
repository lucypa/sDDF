#include "shared_ringbuffer.h"
#include "util.h"

uintptr_t tx_free_drv;
uintptr_t tx_used_drv;
uintptr_t tx_free_cli;
uintptr_t tx_used_cli;
uintptr_t tx_free_arp;
uintptr_t tx_used_arp;

uintptr_t shared_dma_vaddr_cli;
uintptr_t shared_dma_paddr_cli;
uintptr_t shared_dma_vaddr_arp;
uintptr_t shared_dma_paddr_arp;
uintptr_t uart_base;

#define CLIENT_CH 0
#define ARP 1
#define NUM_CLIENTS 2
#define DRIVER_CH 2
#define DMA_SIZE 0x200000

typedef struct state {
    /* Pointers to shared buffers */
    ring_handle_t tx_ring_drv;
    ring_handle_t tx_ring_clients[NUM_CLIENTS];
} state_t;

state_t state;

static uintptr_t
get_phys_addr(uintptr_t virtual)
{
    int offset;
    uintptr_t base;
    if (virtual >= shared_dma_vaddr_cli && virtual < shared_dma_vaddr_cli + DMA_SIZE) {
        offset = virtual - shared_dma_vaddr_cli;
        base = shared_dma_paddr_cli;
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
    if (phys >= shared_dma_paddr_cli && phys < shared_dma_paddr_cli + DMA_SIZE) {
        offset = phys - shared_dma_paddr_cli;
        base = shared_dma_vaddr_cli;
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
    if (addr >= shared_dma_vaddr_cli && addr < shared_dma_vaddr_cli + DMA_SIZE) {
        return CLIENT_CH;
    } else if (addr >= shared_dma_vaddr_arp && addr < shared_dma_vaddr_arp + DMA_SIZE) {
        return ARP;
    }
    print("MUX TX|ERROR: Buffer out of range\n");
    assert(0);
}

/*
Loop over all used tx buffers in client queues and enqueue to driver.
TODO: Put client prioritisation in here.
*/
void process_tx_ready(void)
{
    uint64_t original_size = ring_size(state.tx_ring_drv.used_ring);
    uint64_t enqueued = 0;

    for (int client = 0; client < NUM_CLIENTS; client++) {
        while (!ring_empty(state.tx_ring_clients[client].used_ring) && !ring_full(state.tx_ring_drv.used_ring)) {
            uintptr_t addr;
            unsigned int len;
            void *cookie;

            int err = dequeue_used(&state.tx_ring_clients[client], &addr, &len, &cookie);
            assert(!err);
            uintptr_t phys = get_phys_addr(addr);
            assert(phys);
            err = enqueue_used(&state.tx_ring_drv, phys, len, cookie);
            assert(!err);

            enqueued += 1;
        }
    }

    if ((original_size == 0 || original_size + enqueued != ring_size(state.tx_ring_drv.used_ring)) && enqueued != 0) {
        sel4cp_notify_delayed(DRIVER_CH);
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

    while (!ring_empty(state.tx_ring_drv.free_ring)) {
        uintptr_t addr;
        unsigned int len;
        void *cookie;
        int err = dequeue_free(&state.tx_ring_drv, &addr, &len, &cookie);
        assert(!err);
        uintptr_t virt = get_virt_addr(addr);
        assert(virt);

        int client = get_client(virt);
        int was_empty = ring_empty(state.tx_ring_clients[client].free_ring);

        err = enqueue_free(&state.tx_ring_clients[client], virt, len, cookie);
        assert(!err);

        if (was_empty) {
            notify_clients[client] = true;
        }
    }

    /* Loop over bitmap and see who we need to notify. */
    for (int client = 0; client < NUM_CLIENTS; client++) {
        if (notify_clients[client]) {
            sel4cp_notify(client);
        }
    }
}

void notified(sel4cp_channel ch)
{
    if (ch == CLIENT_CH || ch == ARP || ch == DRIVER_CH ) {
        process_tx_complete();
        process_tx_ready();
    } else {
       print("MUX TX|ERROR: unexpected notification from channel: ");
       puthex64(ch);
       print("\n");
       assert(0);
    }
}

void init(void)
{
    /* Set up shared memory regions */
    // FIX ME: Use the notify function pointer to put the notification in?
    ring_init(&state.tx_ring_drv, (ring_buffer_t *)tx_free_drv, (ring_buffer_t *)tx_used_drv, NULL, 1);
    ring_init(&state.tx_ring_clients[0], (ring_buffer_t *)tx_free_cli, (ring_buffer_t *)tx_used_cli, NULL, 0);
    ring_init(&state.tx_ring_clients[1], (ring_buffer_t *)tx_free_arp, (ring_buffer_t *)tx_used_arp, NULL, 0);

    print("MUX: initialised\n");

    return;
}
