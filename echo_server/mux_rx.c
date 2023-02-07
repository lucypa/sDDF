#include "shared_ringbuffer.h"
#include "util.h"
#include "netif/ethernet.h"

uintptr_t rx_avail_drv;
uintptr_t rx_used_drv;

uintptr_t rx_avail_cli;
uintptr_t rx_used_cli;

uintptr_t shared_dma_vaddr;
uintptr_t uart_base;

#define NUM_CLIENTS 1

#define COPY_CH 0
/* Channel ID for driver, only used for initialisation. */
#define DRIVER_CH 1

#define ETHER_MTU 1500

typedef struct state {
    /* Pointers to shared buffers */
    ring_handle_t rx_ring_drv;
    ring_handle_t rx_ring_clients[NUM_CLIENTS];
    uint8_t mac_addrs[NUM_CLIENTS][6];
} state_t;

state_t state;
int initialised = 0;
uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint64_t total = 0;
uint64_t dropped = 0;

static void
dump_mac(uint8_t *mac)
{
    for (unsigned i = 0; i < 6; i++) {
        sel4cp_dbg_putc(hexchar((mac[i] >> 4) & 0xf));
        sel4cp_dbg_putc(hexchar(mac[i] & 0xf));
        if (i < 5) {
            sel4cp_dbg_putc(':');
        }
    }
    sel4cp_dbg_putc('\n');
}

int compare_mac(uint8_t *mac1, uint8_t *mac2)
{
    for (int i = 0; i < 6; i++) {
        if (mac1[i] != mac2[i]) {
            return -1;
        }
    }
    return 0;
}

/* Return the client ID if the Mac address is a match. */
int get_client(uintptr_t dma_vaddr) {
    uint8_t dest_addr[6];
    struct eth_hdr *ethhdr = (struct eth_hdr *)dma_vaddr;
    dest_addr[0] = ethhdr->dest.addr[0];
    dest_addr[1] = ethhdr->dest.addr[1];
    dest_addr[2] = ethhdr->dest.addr[2];
    dest_addr[3] = ethhdr->dest.addr[3];
    dest_addr[4] = ethhdr->dest.addr[4];
    dest_addr[5] = ethhdr->dest.addr[5];

    for (int client = 0; client < NUM_CLIENTS; client++) {
        if (compare_mac(dest_addr, state.mac_addrs[client]) == 0) {
            return client;
        } else if (compare_mac(dest_addr, broadcast) == 0) {
            // broadcast packet, send the packet to the first client to handle.
            // This is temporary, eventually we will have a different
            // component to deal with this.
            return 0;
        }
    }
    // @ivanv: we return 0 just for safety, deal with this later.
    return 0;
}

static uint64_t ring_size_before = 0;
static uint64_t ring_size_after = 0;

/*
Loop over driver and insert all used rx buffers to appropriate client queues.
*/
bool process_rx_complete(void)
{
    bool done_work = false;
    // print("MUX| rx complete\n");
    int notify_clients[NUM_CLIENTS] = {0};
    bool rx_avail_was_empty = ring_empty(state.rx_ring_drv.avail_ring);
    while (!ring_empty(state.rx_ring_drv.used_ring)) {
        // ring_size_after = ring_size(state.rx_ring_drv.used_ring);

        total++;
        uintptr_t addr = 0;
        unsigned int len = 0;
        void *cookie = NULL;

        int err = dequeue_used(&state.rx_ring_drv, &addr, &len, &cookie);
        assert(!err);
        err = seL4_ARM_VSpace_Invalidate_Data(3, addr, addr + ETHER_MTU);
        if (err) {
            print("MUX RX|ERROR: ARM Vspace invalidate failed\n");
            puthex64(err);
            print("\n");
        }

        // Get MAC address and work out which client it is.
        int client = get_client(addr);
        // @ivanv: should check that there is space before enqueueing?
        if (client >= 0) {
            /* enqueue it. */
            int was_empty = ring_empty(state.rx_ring_clients[client].used_ring);
            int err = enqueue_used(&state.rx_ring_clients[client], addr, len, cookie);
            assert(!err);
            if (err) {
                print("Mux_rx enqueue used failed. Ring full\n");
            }
            /* We don't want to signal the copier until we know there is something
               in the used ring and the avail ring is also not empty. */
            assert(!ring_empty(state.rx_ring_clients[client].used_ring));
            if (was_empty) {
                notify_clients[client] = 1;
            }
        } else {
            // no match, not for us, return the buffer to the driver.
            if (addr == 0) {
                print("MUX RX|ERROR: Attempting to add NULL buffer to driver RX ring\n");
            }
            err = enqueue_avail(&state.rx_ring_drv, addr, len, cookie);
            if (err) {
                print("MUX RX|ERROR: Failed to enqueue available to driver RX ring\n");
            }
            dropped++;
        }

        done_work = true;
    }

    /* Loop over bitmap and see who we need to notify. */
    for (int client = 0; client < NUM_CLIENTS; client++) {
        if (notify_clients[client]) {
            // assert(!ring_empty(state.rx_ring_clients[client].avail_ring));
            assert(!ring_empty(state.rx_ring_clients[client].used_ring));
            // print("MUX RX: notify client\n");
            done_work = true;
            sel4cp_notify(client);
        }
    }
    /*
     * Tell the driver if we've added some Rx bufs.
     * Note this will force a context switch, as the driver runs at higher
     * priority than the MUX
     */
    if (dropped && rx_avail_was_empty) {
        // @ivanv: We're potentially notifying again in process_rx_free.
        done_work = true;
        print("MUX RX: added rx avail\n");
        // sel4cp_notify(DRIVER_CH);
        sel4cp_notify_delayed(DRIVER_CH);
    }

    return done_work;
}

static uint64_t num_invoked = 0;
static uint64_t num_enqueued = 0;

// Loop over all client rings and return unused rx buffers to the driver
bool process_rx_free(void)
{
    /* If we have enqueued to the driver's available ring and the available
     * ring was empty, we want to notify the driver. We also only want to
     * notify it only once.
     */
    uint64_t original_size = ring_size(state.rx_ring_drv.avail_ring);
    uint64_t enqueued = 0;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        while (!ring_empty(state.rx_ring_clients[i].avail_ring)) {
            uintptr_t addr;
            unsigned int len;
            void *buffer;
            int err = dequeue_avail(&state.rx_ring_clients[i], &addr, &len, &buffer);
            assert(!err);
            err = enqueue_avail(&state.rx_ring_drv, addr, len, buffer);
            assert(!err);
            enqueued += 1;
        }
    }
    if ((original_size == 0 || original_size + enqueued != ring_size(state.rx_ring_drv.avail_ring)) && enqueued != 0) {
        // print("MUX RX: notify driver in process_rx_free\n");
        // print("MUX RX (before notify): client[0].avail ");
        // puthex64(ring_size(state.rx_ring_clients[0].avail_ring));
        // print("\n client[0].used ");
        // puthex64(ring_size(state.rx_ring_clients[0].used_ring));
        // print("\n driver.avail ");
        // puthex64(ring_size(state.rx_ring_drv.avail_ring));
        // print("\n driver.used ");
        // puthex64(ring_size(state.rx_ring_drv.used_ring));
        // print("\n\n");
        // print("MUX|RX: notifying driver!\n");
        // print("MUX RX: ADDED RX AVAIL\n");
        // print("     num_enqueued: ");
        // puthex64(num_enqueued);
        // print("\n");
        // print("     num_invoked: ");
        // puthex64(num_invoked);
        // print("\n");
        // sel4cp_notify(DRIVER_CH);
        sel4cp_notify_delayed(DRIVER_CH);
    }

    return enqueued;
}

seL4_MessageInfo_t
protected(sel4cp_channel ch, sel4cp_msginfo msginfo)
{
    // if (ch >= NUM_CLIENTS) {
    //     sel4cp_dbg_puts("Received ppc on unexpected channel ");
    //     puthex64(ch);
    //     return sel4cp_msginfo_new(0, 0);
    // }
    // // return the MAC address.
    // uint32_t lower = (state.mac_addrs[ch][0] << 24) |
    //                  (state.mac_addrs[ch][1] << 16) |
    //                  (state.mac_addrs[ch][2] << 8) |
    //                  (state.mac_addrs[ch][3]);
    // uint32_t upper = (state.mac_addrs[ch][4] << 24) | (state.mac_addrs[ch][5] << 16);
    // sel4cp_dbg_puts("Mux rx is sending mac: ");
    // dump_mac(state.mac_addrs[ch]);
    // sel4cp_mr_set(0, lower);
    // sel4cp_mr_set(1, upper);
    // return sel4cp_msginfo_new(0, 2);

    print("MUX RX: rx_avail_drv ");
    puthex64(ring_size(state.rx_ring_drv.avail_ring));
    print("\n rx_used_drv ");
    puthex64(ring_size(state.rx_ring_drv.used_ring));
    print("\n rx_avail_clients[0] ");
    puthex64(ring_size(state.rx_ring_clients[0].avail_ring));
    print("\n rx_used_clients[0] ");
    puthex64(ring_size(state.rx_ring_clients[0].used_ring));
    print("\n\n");
    return sel4cp_msginfo_new(0, 0);
}

void notified(sel4cp_channel ch)
{
    if (initialised) {
        if (ch == COPY_CH) {
            // We should only be notified by the copy component
            // if there is something placed in the available ring.
        } else if (ch == DRIVER_CH) {
            // if (ring_empty(state.rx_ring_drv.used_ring)) {
            //     print("MUX| rx_ring_drv.used_ring size: ");
            //     puthex64(ring_size(state.rx_ring_drv.used_ring));
            //     print("\n");
            // }
            // if (ring_empty(state.rx_ring_drv.used_ring)) {
            //     print("assert went off!\n");
            //     print("write_idx: ");
            //     puthex64(state.rx_ring_drv.used_ring->write_idx);
            //     print(" read_idx: ");
            //     puthex64(state.rx_ring_drv.used_ring->read_idx);
            //     print("\n");
            // }
            // assert(!ring_empty(state.rx_ring_drv.used_ring));
            // ring_size_before = ring_size(satte.rx_ring_drv.used_ring);
        } else {
            print("MUX RX: unexpected notification!\n");
        }
        // static unsigned counter = 0;
        // if (++counter % 0x1000U == 0) {
        //     print("MUX RX (BEFORE): client[0].avail ");
        //     puthex64(ring_size(state.rx_ring_clients[0].avail_ring));
        //     print("\n client[0].used ");
        //     puthex64(ring_size(state.rx_ring_clients[0].used_ring));
        //     print("\n driver.avail ");
        //     puthex64(ring_size(state.rx_ring_drv.avail_ring));
        //     print("\n driver.used ");
        //     puthex64(ring_size(state.rx_ring_drv.used_ring));
        //     print("\n\n");
        // }
        process_rx_complete();
        process_rx_free();
        // if (!complete_done_work && !free_done_work) {
            // print("MUX RX| no work done ");
            // if (ch == COPY_CH) {
            //     print("from copy\n");
            // }
            // if (ch == DRIVER_CH) {
            //     print("from driver\n");
            // }
        // }
        // if (counter % 0x1000U == 0) {
        //     print("MUX RX (AFTER): client[0].avail ");
        //     puthex64(ring_size(state.rx_ring_clients[0].avail_ring));
        //     print("\n client[0].used ");
        //     puthex64(ring_size(state.rx_ring_clients[0].used_ring));
        //     print("\n driver.avail ");
        //     puthex64(ring_size(state.rx_ring_drv.avail_ring));
        //     print("\n driver.used ");
        //     puthex64(ring_size(state.rx_ring_drv.used_ring));
        //     print("\n\n");
        // }
    } else {
        process_rx_free();
        sel4cp_notify(DRIVER_CH);
        initialised = 1;
    }
}

void init(void)
{
    // set up client macs
    /*state.mac_addrs[0][0] = 0x52;
    state.mac_addrs[0][1] = 0x54;
    state.mac_addrs[0][2] = 0x1;
    state.mac_addrs[0][3] = 0;
    state.mac_addrs[0][4] = 0;
    state.mac_addrs[0][5] = 0;*/
    state.mac_addrs[0][0] = 0;
    state.mac_addrs[0][1] = 0x4;
    state.mac_addrs[0][2] = 0x9f;
    state.mac_addrs[0][3] = 0x5;
    state.mac_addrs[0][4] = 0xf8;
    state.mac_addrs[0][5] = 0xcc;

    /* Set up shared memory regions */
    ring_init(&state.rx_ring_drv, (ring_buffer_t *)rx_avail_drv, (ring_buffer_t *)rx_used_drv, NULL, 1);

    // FIX ME: Use the notify function pointer to put the notification in?
    ring_init(&state.rx_ring_clients[0], (ring_buffer_t *)rx_avail_cli, (ring_buffer_t *)rx_used_cli, NULL, 0);

    return;
}

