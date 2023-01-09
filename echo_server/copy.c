#include "shared_ringbuffer.h"
#include "util.h"

uintptr_t rx_avail_mux;
uintptr_t rx_used_mux;

uintptr_t rx_avail_cli;
uintptr_t rx_used_cli;

uintptr_t shared_dma_vaddr_mux;
uintptr_t shared_dma_vaddr_cli;
uintptr_t uart_base;

#define CLIENT 0

ring_handle_t rx_ring_mux;
ring_handle_t rx_ring_cli;

void process_rx_complete(void) 
{
    int was_empty = ring_empty(rx_ring_cli.used_ring);
    while(!ring_empty(rx_ring_mux.used_ring)) {
        uintptr_t m_addr, c_addr;
        unsigned int m_len, c_len;
        void *cookie;
        void *cookie2;

        dequeue_used(&rx_ring_mux, &m_addr, &m_len, &cookie);
        // get an available one from clients queue.
        dequeue_avail(&rx_ring_cli, &c_addr, &c_len, &cookie2);
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
    // we have one job.
    process_rx_complete();
}

void init(void)
{
    /* Set up shared memory regions */
    ring_init(&rx_ring_mux, (ring_buffer_t *)rx_avail_drv, (ring_buffer_t *)rx_used_drv, NULL, 0);
    ring_init(&rx_ring_cli, (ring_buffer_t *)rx_avail_cli, (ring_buffer_t *)rx_used_cli, NULL, 0);
    
    return;
}