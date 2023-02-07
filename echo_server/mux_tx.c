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

static bool has_received_pp = false;
static bool has_notified_client_since_pp = false;

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
void process_tx_ready(void)
{
    uint64_t original_size = ring_size(state.tx_ring_drv.used_ring);
    uint64_t enqueued = 0;

    // @ivanv: should check that driver TX ring has room
    while (!ring_empty(state.tx_ring_clients[0].used_ring)) {
        uintptr_t addr;
        unsigned int len;
        void *cookie;

        int err = dequeue_used(&state.tx_ring_clients[0], &addr, &len, &cookie);
        assert(!err);
        err = enqueue_used(&state.tx_ring_drv, addr, len, cookie);
        assert(!err);

        enqueued += 1;
    }

    if ((original_size == 0 || original_size + enqueued != ring_size(state.tx_ring_drv.used_ring)) && enqueued != 0) {
        // have_signal = true;
        // signal_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        // signal = (BASE_OUTPUT_NOTIFICATION_CAP + DRIVER_CH);
        sel4cp_notify_delayed(DRIVER_CH);
        // sel4cp_notify(DRIVER_CH);
    }
}

/*
 * Take as many TX available buffers from the driver and give them to
 * the client. This will notify the client if we have moved buffers
 * around and the client's TX available ring was already empty.
 */
// TODO: We need a way of knowing which client
// this buffer originated from.
void process_tx_complete(void)
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
        assert(!ring_empty(state.tx_ring_clients[0].avail_ring));
        if (has_received_pp) {
            has_notified_client_since_pp = true;
        }
        sel4cp_notify(CLIENT_CH);
    }
}

void notified(sel4cp_channel ch)
{
    process_tx_complete();
    process_tx_ready();
}

seL4_MessageInfo_t
protected(sel4cp_channel ch, sel4cp_msginfo msginfo)
{
    print("MUX TX: tx_avail_drv ");
    puthex64(ring_size(state.tx_ring_drv.avail_ring));
    print("\n tx_used_drv ");
    puthex64(ring_size(state.tx_ring_drv.used_ring));
    print("\n tx_ring_clients[0].avail_ring ");
    puthex64(ring_size(state.tx_ring_clients[0].avail_ring));
    print("\n tx_ring_clients[0].used_ring ");
    puthex64(ring_size(state.tx_ring_clients[0].used_ring));
    print("\n\n");
    has_received_pp = true;
    if (has_received_pp && has_notified_client_since_pp) {
        print("MUXTX| has notified client since PP!\n");
    }
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
