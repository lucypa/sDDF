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

uintptr_t debug_mapping;

#define MUX_RX_CH 0
#define CLIENT_CH 1

#define BUF_SIZE 2048
#define NUM_BUFFERS 512
#define SHARED_DMA_SIZE (BUF_SIZE * NUM_BUFFERS)

ring_handle_t rx_ring_mux;
ring_handle_t rx_ring_cli;
int initialised = 0;

static int count = 0;

bool process_rx_complete(void)
{
    // print("copy: process_rx_complete\n");
    bool done_work = false;
    bool mux_was_full = ring_full(rx_ring_mux.used_ring);
    bool mux_avail_was_empty = ring_empty(rx_ring_mux.avail_ring);
    int was_empty = ring_empty(rx_ring_cli.used_ring);
    /* We only want to copy buffers if meet both conditions:
       1. There are buffers from the mux to copy to.
       2. There are buffers from the client to copy.
    */
    int processed = 0;
    // if (!(!ring_empty(rx_ring_mux.used_ring) &&
    //         !ring_empty(rx_ring_cli.avail_ring) &&
    //         !ring_full(rx_ring_mux.avail_ring) &&
    //         !ring_full(rx_ring_cli.used_ring))) {

    //     if (ring_empty(rx_ring_mux.used_ring)) {
    //         print("COPY| cond 1 failed");
    //     }
    //     if (ring_empty(rx_ring_cli.avail_ring)) {
    //         print("COPY| cond 2 failed");
    //     }
    //     if (ring_full(rx_ring_mux.avail_ring)) {
    //         print("COPY| cond 3 failed");
    //     }
    //     if (ring_full(rx_ring_cli.used_ring)) {
    //         print("COPY| cond 4 failed");
    //     }
    // }
    while (!ring_empty(rx_ring_mux.used_ring) &&
            !ring_empty(rx_ring_cli.avail_ring) &&
            !ring_full(rx_ring_mux.avail_ring) &&
            !ring_full(rx_ring_cli.used_ring)) {
        uintptr_t m_addr, c_addr = 0;
        unsigned int m_len, c_len = 0;
        void *cookie = NULL;
        void *cookie2 = NULL;
        int err;

        err = dequeue_used(&rx_ring_mux, &m_addr, &m_len, &cookie);
        assert(!err);
        // get an available one from clients queue.
        // TODO: Check the address we dequeue is sane and return
        // it if not.
        err = dequeue_avail(&rx_ring_cli, &c_addr, &c_len, &cookie2);
        assert(!err);
        if (!c_addr ||
                c_addr < shared_dma_vaddr_cli ||
                c_addr >= shared_dma_vaddr_cli + SHARED_DMA_SIZE)
        {
            print("COPY|ERROR: Received an insane address: ");
            puthex64(c_addr);
            print(". Address should be between ");
            puthex64(shared_dma_vaddr_cli);
            print(" and ");
            puthex64(shared_dma_vaddr_cli + SHARED_DMA_SIZE);
            print("\n");
        }

        if (c_len < m_len) {
            print("COPY|ERROR: client buffer length is less than mux buffer length.\n");
            print("client length: ");
            puthex64(c_len);
            print(" mux length: ");
            puthex64(m_len);
            print("\n");
        }
        // copy the data over to the clients address space.
        memcpy((void *)c_addr, (void *)m_addr, m_len);

        /* Now that we've copied the data, enqueue the buffer to the client's used ring. */
        err = enqueue_used(&rx_ring_cli, c_addr, m_len, cookie2);
        assert(!err);
        /* enqueue the old buffer back to dev_rx_ring.avail so the driver can use it again. */
        err = enqueue_avail(&rx_ring_mux, m_addr, BUF_SIZE, cookie);
        assert(!err);
        // @ivanv: this assert was going off, which is unexpected
        // assert(!ring_empty(rx_ring_mux.avail_ring));

        done_work = true;

        processed += 1;
    }

    count += 1;

    // if ((mux_was_full || mux_avail_was_empty) && done_work) {
    //     if (count % 100 == 0) {
    //         // print("COPY has addition mux\n");
    //         count = 0;
    //     }
    //     // assert(!ring_empty(rx_ring_mux.avail_ring));
    //     sel4cp_notify(MUX_RX_CH);
    // }

        // @ivanv: this should be an sel4cp syscall rather than dealing with these globals in user code
        // have_signal = true;
        // signal_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        // signal = (BASE_OUTPUT_NOTIFICATION_CAP + CLIENT_CH);
        // print("COPY| notifying lwip\n");
    if (!ring_empty(rx_ring_cli.used_ring)) {
        sel4cp_notify(CLIENT_CH);
    }

    if ((mux_was_full || mux_avail_was_empty) && done_work) {
        // assert(!ring_empty(rx_ring_mux.avail_ring));
        sel4cp_notify_delayed(MUX_RX_CH);
    }

    // print("COPY| processed ");
    // puthex64(processed);
    // print("\n");
    return done_work;
}

seL4_MessageInfo_t
protected(sel4cp_channel ch, sel4cp_msginfo msginfo)
{
    print("COPY: rx_avail_mux ");
    puthex64(ring_size(rx_ring_mux.avail_ring));
    print("\n rx_used_mux ");
    puthex64(ring_size(rx_ring_mux.used_ring));
    print("\n rx_avail_cli ");
    puthex64(ring_size(rx_ring_cli.avail_ring));
    print("\n rx_used_cli ");
    puthex64(ring_size(rx_ring_cli.used_ring));
    print("\n\n");
    return sel4cp_msginfo_new(0, 0);
}

void notified(sel4cp_channel ch)
{
    if (!initialised) {
        // propogate this down the line to ensure everyone is
        // initliased in correct order.
        sel4cp_notify(MUX_RX_CH);
        initialised = 1;
        // print("COPY: init finished\n");
        return;
    }
    // we have one job.
    if (ch == CLIENT_CH) {
        // assert(!ring_empty(rx_ring_cli.avail_ring));
    } else if (ch == MUX_RX_CH) {
        // assert(!ring_empty(rx_ring_mux.used_ring));
        // if (!ring_empty(rx_ring_mux.used_ring)) {
        //     print("COPY| size of rx_ring_cli.used_ring: ");
        //     puthex64(ring_size(rx_ring_cli.used_ring));
        //     print()
        // }
    } else {
        print("COPY| unexpected notification!\n");
        assert(0);
    }
    process_rx_complete();
}

void init(void)
{
    // print("COPY: init started\n");
    /* Set up shared memory regions */
    // @ivanv: Having asynchronous initialisation is extremely fragile, we need to find a better way.
    ring_init(&rx_ring_mux, (ring_buffer_t *)rx_avail_mux, (ring_buffer_t *)rx_used_mux, NULL, 1);
    ring_init(&rx_ring_cli, (ring_buffer_t *)rx_avail_cli, (ring_buffer_t *)rx_used_cli, NULL, 0);

    /* Enqueue available buffers for the mux to access */
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        uintptr_t addr = shared_dma_vaddr_mux + (BUF_SIZE * i);
        int err = enqueue_avail(&rx_ring_mux, addr, BUF_SIZE, NULL);
        if (err) {
            print("COPY INIT: i is: ");
            puthex64(i);
            print("\n");
        }
        assert(!err);
    }

    return;
}
