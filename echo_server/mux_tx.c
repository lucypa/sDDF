#include "shared_ringbuffer.h"
#include "util.h"

uintptr_t tx_avail_drv;
uintptr_t tx_used_drv;

uintptr_t tx_avail_cli;
uintptr_t tx_used_cli;

uintptr_t shared_dma_vaddr;
uintptr_t uart_base;

#define CLIENT_CH 0
#define NUM_CLIENTS 1
#define DRIVER_CH 1

typedef struct state {
    /* Pointers to shared buffers */
    ring_handle_t tx_ring_drv;
    ring_handle_t tx_ring_clients[NUM_CLIENTS];
} state_t;

state_t state;

/*
Loop over all used tx buffers in client queues and enqueue to driver.
TODO: Put client prioritisation in here.
*/
bool process_tx_ready(void)
{
    bool done_work = false;
    bool was_empty = ring_empty(state.tx_ring_drv.used_ring);

    // @ivanv: should check that driver TX ring has room
    while(!ring_empty(state.tx_ring_clients[0].used_ring)) {
        uintptr_t addr;
        unsigned int len;
        void *cookie;

        int err = dequeue_used(&state.tx_ring_clients[0], &addr, &len, &cookie);
        if (err) {
            print("process_tx_ready dequeue used failed.\n");
            break;
        }
        err = enqueue_used(&state.tx_ring_drv, addr, len, cookie);
        if (err) {
            print("MUX TX|ERROR: Failed to enqueue to used driver TX ring\n");
        }
        done_work = true;
    }

    if (was_empty && done_work) {
        // have_signal = true;
        // signal_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        // signal = (BASE_OUTPUT_NOTIFICATION_CAP + DRIVER_CH);
        sel4cp_notify(DRIVER_CH);
    }

    return done_work;
}

// Return available tx buffers to clients.
// TODO: We need a way of knowing which client
// this buffer originated from.
bool process_tx_complete(void)
{
    bool was_empty = ring_empty(state.tx_ring_clients[0].avail_ring);
    bool enqueued = false;
    while (!ring_empty(state.tx_ring_drv.avail_ring) && !ring_full(state.tx_ring_clients[0].avail_ring)) {
        uintptr_t addr;
        unsigned int len;
        void *cookie;
        int err = dequeue_avail(&state.tx_ring_drv, &addr, &len, &cookie);
        assert(!err);
        err = enqueue_avail(&state.tx_ring_clients[0], addr, len, cookie);
        assert(!err);
        enqueued = true;
    }

    if (enqueued && was_empty) {
        print("MUX TX notifying client\n");
        sel4cp_notify(CLIENT_CH);
    }

    return enqueued;
}

void notified(sel4cp_channel ch)
{
    static unsigned counter = 0;
    if (++counter % 0x10000U == 0) {
        print("MUX TX (BEFORE): client[0].avail ");
        puthex64(ring_size(state.tx_ring_clients[0].avail_ring));
        print("\n client[0].used ");
        puthex64(ring_size(state.tx_ring_clients[0].used_ring));
        print("\n driver.avail ");
        puthex64(ring_size(state.tx_ring_drv.avail_ring));
        print("\n driver.used ");
        puthex64(ring_size(state.tx_ring_drv.used_ring));
        print("\n\n");
    }
    bool complete_done_work = process_tx_complete();
    bool ready_done_work = process_tx_ready();
    if (!ready_done_work && !complete_done_work) {
        // print("MUX TX| no work done!\n");
    }
    if (counter % 0x10000U == 0) {
        print("MUX TX (AFTER): client[0].avail ");
        puthex64(ring_size(state.tx_ring_clients[0].avail_ring));
        print("\n client[0].used ");
        puthex64(ring_size(state.tx_ring_clients[0].used_ring));
        print("\n driver.avail ");
        puthex64(ring_size(state.tx_ring_drv.avail_ring));
        print("\n driver.used ");
        puthex64(ring_size(state.tx_ring_drv.used_ring));
        print("\n\n");
    }
}

seL4_MessageInfo_t
protected(sel4cp_channel ch, sel4cp_msginfo msginfo)
{
    print("MUX TX: tx_avail_drv ");
    puthex64(ring_size(state.tx_ring_drv.avail_ring));
    print("\n tx_used_drv ");
    puthex64(ring_size(state.tx_ring_drv.used_ring));
    print("\n tx_avail_clients[0] ");
    puthex64(ring_size(state.tx_ring_clients[0].avail_ring));
    print("\n tx_used_clients[0] ");
    puthex64(ring_size(state.tx_ring_clients[0].used_ring));
    print("\n\n");
    return sel4cp_msginfo_new(0, 0);
}

void init(void)
{
    /* Set up shared memory regions */
    // FIX ME: Use the notify function pointer to put the notification in?
    ring_init(&state.tx_ring_drv, (ring_buffer_t *)tx_avail_drv, (ring_buffer_t *)tx_used_drv, NULL, 1);
    ring_init(&state.tx_ring_clients[0], (ring_buffer_t *)tx_avail_cli, (ring_buffer_t *)tx_used_cli, NULL, 0);

    print("MUX: initialised\n");

    return;
}
