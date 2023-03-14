#include "shared_ringbuffer.h"
#include "util.h"
#include "logbuffer.h"

uintptr_t tx_avail_drv;
uintptr_t tx_used_drv;

uintptr_t tx_avail_cli;
uintptr_t tx_used_cli;

uintptr_t shared_dma_vaddr;
uintptr_t uart_base;

#define CLIENT_CH 0
#define NUM_CLIENTS 1
#define DRIVER_CH 1
#define TX_CH 4
#define TRACE 2
#define TRACE_NTFN 3

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
void process_tx_ready(sel4cp_channel ch)
{
    uint64_t original_size = ring_size(state.tx_ring_drv.used_ring);
    uint32_t enqueued = 0;

    if (!ring_empty(state.tx_ring_clients[0].used_ring) && !ring_full(state.tx_ring_drv.used_ring)) {
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
        sel4cp_notify_delayed(DRIVER_CH);
    }

    new_log_buffer_entry_used(enqueued, ch,
                                ring_size(state.tx_ring_clients[0].avail_ring),
                                ring_size(state.tx_ring_clients[0].used_ring),
                                ring_size(state.tx_ring_drv.avail_ring),
                                ring_size(state.tx_ring_drv.used_ring));
}

/*
 * Take as many TX available buffers from the driver and give them to
 * the client. This will notify the client if we have moved buffers
 * around and the client's TX available ring was already empty.
 */
// TODO: Use the address range to determine which client
// this buffer originated from to return it to the correct one. 
void process_tx_complete(sel4cp_channel ch)
{
    bool was_empty = ring_empty(state.tx_ring_clients[0].avail_ring);
    uint32_t enqueued = 0;
    if (!ring_empty(state.tx_ring_drv.avail_ring) && !ring_full(state.tx_ring_clients[0].avail_ring)) {
        uintptr_t addr;
        unsigned int len;
        void *cookie;
        int err = dequeue_avail(&state.tx_ring_drv, &addr, &len, &cookie);
        assert(!err);
        err = enqueue_avail(&state.tx_ring_clients[0], addr, len, cookie);
        assert(!err);
        enqueued++;
    }

    if (enqueued && was_empty) {
        sel4cp_notify(CLIENT_CH);
    }

    new_log_buffer_entry_free(enqueued, ch,
                                ring_size(state.tx_ring_clients[0].avail_ring),
                                ring_size(state.tx_ring_clients[0].used_ring),
                                ring_size(state.tx_ring_drv.avail_ring),
                                ring_size(state.tx_ring_drv.used_ring));
}

void notified(sel4cp_channel ch)
{
    if (ch == CLIENT_CH) {
        process_tx_ready(ch);
    } else if (ch == DRIVER_CH || ch == TX_CH) {
        process_tx_complete(ch);
    } else if (ch == TRACE) { 
        log_buffer_stop();
        sel4cp_notify(TRACE_NTFN);
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
    ring_init(&state.tx_ring_drv, (ring_buffer_t *)tx_avail_drv, (ring_buffer_t *)tx_used_drv, NULL, 1);
    ring_init(&state.tx_ring_clients[0], (ring_buffer_t *)tx_avail_cli, (ring_buffer_t *)tx_used_cli, NULL, 0);

    notifications[CLIENT_CH] = "lwip";
    notifications[DRIVER_CH] = "Tx Driver";
    notifications[TX_CH] = "Driver";

    print("MUX: initialised\n");

    return;
}
