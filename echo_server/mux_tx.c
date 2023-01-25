#include "shared_ringbuffer.h"
#include "util.h"

uintptr_t tx_avail_drv;
uintptr_t tx_used_drv;

uintptr_t tx_avail_cli;
uintptr_t tx_used_cli;

uintptr_t shared_dma_vaddr;
uintptr_t uart_base;

#define NUM_CLIENTS 1
#define DRIVER_TX_CH 1

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
    // @ivanv: should this be in the loop?
    bool was_empty = ring_empty(state.tx_ring_drv.used_ring);

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
    }

    if (was_empty) {
        have_signal = true;
        signal_msg = seL4_MessageInfo_new(0, 0, 0, 0);
        signal = (BASE_OUTPUT_NOTIFICATION_CAP + DRIVER_TX_CH);
    }
}

// Return available tx buffers to clients.
// TODO: We need a way of knowing which client
// this buffer originated from.
void process_tx_complete(void)
{
    while (!ring_empty(state.tx_ring_drv.avail_ring)) {
        uintptr_t addr;
        unsigned int len;
        void *cookie;
        int err = dequeue_avail(&state.tx_ring_drv, &addr, &len, &cookie);
        assert(!err);
        err = enqueue_avail(&state.tx_ring_clients[0], addr, len, cookie);
        assert(!err);
    }
}

void notified(sel4cp_channel ch)
{
    static unsigned counter = 0;
    process_tx_ready();
    process_tx_complete();
    if (++counter % 0x10000U == 0) {
        print("MUX: client[0].avail ");
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

void init(void)
{
    /* Set up shared memory regions */
    // FIX ME: Use the notify function pointer to put the notification in?
    ring_init(&state.tx_ring_drv, (ring_buffer_t *)tx_avail_drv, (ring_buffer_t *)tx_used_drv, NULL, 1);
    ring_init(&state.tx_ring_clients[0], (ring_buffer_t *)tx_avail_cli, (ring_buffer_t *)tx_used_cli, NULL, 0);

    print("MUX: initialised\n");

    return;
}
