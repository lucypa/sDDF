#include <stdint.h>
#include <sel4cp.h>
#include "util.h"
#define GET_TIME 0
#define SET_TIMEOUT 1
#define TIMER_CH 1 

uintptr_t copy_log;
uintptr_t eth_log;
uintptr_t rx_log;
uintptr_t tx_log;
uintptr_t client_log;
uintptr_t uart_base;

struct entry {
    uint64_t cycle_count;
    uint32_t packets_processed;
    uint32_t notification;
    uint32_t left_free_queue;
    uint32_t left_used_queue;
    uint32_t right_free_queue;
    uint32_t right_used_queue;
};

struct entry *client = (void *)(uintptr_t)0x2004000;
struct entry *eth= (void *)(uintptr_t)0x2001000;
struct entry *rx = (void *)(uintptr_t)0x2002000;
struct entry *tx = (void *)(uintptr_t)0x2003000;
struct entry *copy = (void *)(uintptr_t)0x2000000;

void notified(sel4cp_channel ch) {
    // dump the log buffers
    for (int i = 0; i < 10; i++) {
        struct entry *entry = &client[i];
        if (entry->cycle_count == 0) continue;
        puthex64(entry->cycle_count);
        print(": client processed ");
        put8(entry->packets_processed);
        print(" packets notified by ");
        put8(entry->notification);
        print(" lfq: ");
        puthex64(entry->left_free_queue);
        print(" luq: ");
        puthex64(entry->left_used_queue);
        print(" rfq: ");
        puthex64(entry->right_free_queue);
        print(" ruq: ");
        puthex64(entry->right_used_queue);
        print("\n");
    }

    for (int i = 0; i < 10; i++) {
        struct entry *entry = &eth[i];
        if (entry->cycle_count == 0) continue;
        puthex64(entry->cycle_count);
        print(": driver processed ");
        put8(entry->packets_processed);
        print(" packets notified by ");
        put8(entry->notification);
        print(" lfq: ");
        puthex64(entry->left_free_queue);
        print(" luq: ");
        puthex64(entry->left_used_queue);
        print(" rfq: ");
        puthex64(entry->right_free_queue);
        print(" ruq: ");
        puthex64(entry->right_used_queue);
        print("\n");
    }

    for (int i = 0; i < 10; i++) {
        struct entry *entry = &rx[i];
        if (entry->cycle_count == 0) continue;
        puthex64(entry->cycle_count);
        print(": rx mux processed ");
        put8(entry->packets_processed);
        print(" packets notified by ");
        put8(entry->notification);
        print(" lfq: ");
        puthex64(entry->left_free_queue);
        print(" luq: ");
        puthex64(entry->left_used_queue);
        print(" rfq: ");
        puthex64(entry->right_free_queue);
        print(" ruq: ");
        puthex64(entry->right_used_queue);
        print("\n");
    }

    for (int i = 0; i < 10; i++) {
        struct entry *entry = &tx[i];
        if (entry->cycle_count == 0) continue;
        puthex64(entry->cycle_count);
        print(": tx mux processed ");
        put8(entry->packets_processed);
        print(" packets notified by ");
        put8(entry->notification);
        print(" lfq: ");
        puthex64(entry->left_free_queue);
        print(" luq: ");
        puthex64(entry->left_used_queue);
        print(" rfq: ");
        puthex64(entry->right_free_queue);
        print(" ruq: ");
        puthex64(entry->right_used_queue);
        print("\n");
    }

    for (int i = 0; i < 10; i++) {
        struct entry *entry = &copy[i];
        if (entry->cycle_count == 0) continue;
        puthex64(entry->cycle_count);
        print(": copy processed ");
        puthex64(entry->packets_processed);
        print(" packets notified by ");
        put8(entry->notification);
        print(" lfq: ");
        puthex64(entry->left_free_queue);
        print(" luq: ");
        puthex64(entry->left_used_queue);
        print(" rfq: ");
        puthex64(entry->right_free_queue);
        print(" ruq: ");
        puthex64(entry->right_used_queue);
        print("\n");
    }

    /*uint64_t timeout = 10000000;
    sel4cp_mr_set(0, timeout);
    sel4cp_ppcall(TIMER_CH, sel4cp_msginfo_new(SET_TIMEOUT, 1));*/
}

void init(void)
{
    /*uint64_t timeout = 60000000;
    sel4cp_mr_set(0, timeout);
    sel4cp_ppcall(TIMER_CH, sel4cp_msginfo_new(SET_TIMEOUT, 1));*/
}