#include "shared_ringbuffer.h"
#include "util.h"
#include <string.h>

uintptr_t rx_avail_mux;
uintptr_t rx_used_mux;

uintptr_t rx_avail_cli;
uintptr_t rx_used_cli;

uintptr_t shared_dma_vaddr_mux;
uintptr_t shared_dma_vaddr_cli;
uintptr_t uart_base;

#define MUX 0
#define CLIENT 1
#define BUF_SIZE 2048
#define NUM_BUFFERS 512

ring_handle_t rx_ring_mux;
ring_handle_t rx_ring_cli;
int initialised = 0;

void process_rx_complete(void) 
{
    sel4cp_dbg_puts("copy: process_rx_complete\n");
    int was_empty = ring_empty(rx_ring_cli.used_ring);
    while(!ring_empty(rx_ring_mux.used_ring)) {
        uintptr_t m_addr, c_addr;
        unsigned int m_len, c_len;
        void *cookie;
        void *cookie2;

        dequeue_used(&rx_ring_mux, &m_addr, &m_len, &cookie);
        // get an available one from clients queue.
        // TODO: Check the address we dequeue is sane and return
        // it if not. 
        dequeue_avail(&rx_ring_cli, &c_addr, &c_len, &cookie2);
        if (!c_addr || c_addr < shared_dma_vaddr_cli ||
                c_addr > (shared_dma_vaddr_cli + (2048*512))) {
            print("Copy.c got an insane address\n");
        }

        // copy the data over to the clients address space. 
        memcpy(c_addr, m_addr, m_len);

        enqueue_used(&rx_ring_cli, c_addr, m_len, cookie2);
        /* enqueue the old buffer back to dev_rx_ring.avail so the driver can use it again. */
        enqueue_avail(&rx_ring_mux, m_addr, BUF_SIZE, cookie);
    }

    if (was_empty) {
        have_signal = true;
        signal_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        signal = (BASE_OUTPUT_NOTIFICATION_CAP + CLIENT);
    }
}

void notified(sel4cp_channel ch)
{
    if (!initialised) {
        // propogate this down the line to ensure everyone is
        // initliased in correct order.
        sel4cp_notify(MUX);
        initialised = 1;
        return;
    } 
    // we have one job.
    process_rx_complete();
}

void init(void)
{
    /* Set up shared memory regions */
    ring_init(&rx_ring_mux, (ring_buffer_t *)rx_avail_mux, (ring_buffer_t *)rx_used_mux, NULL, 0);
    ring_init(&rx_ring_cli, (ring_buffer_t *)rx_avail_cli, (ring_buffer_t *)rx_used_cli, NULL, 0);
    
    /* Enqueue available buffers for the mux to access */
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        uintptr_t addr = shared_dma_vaddr_mux + (BUF_SIZE * i);
        enqueue_avail(&rx_ring_mux, addr, BUF_SIZE, NULL);
    }

    return;
}