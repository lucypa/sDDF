#include "sel4bench.h"
#include "bench.h"
#include "util.h"

// If we just use a single 2MB page we will fit 256 entries. 
#define LOG_BUFFER_SIZE 50

uintptr_t buffer_used;
uintptr_t buffer_free;

int head_used = 0;
int head_free = 0;

struct entry {
    uint64_t cycle_count;
    uint32_t packets_processed;
    uint32_t notification;
    uint32_t left_free_queue;
    uint32_t left_used_queue;
    uint32_t right_free_queue;
    uint32_t right_used_queue;
};

/* The pd needs to initialise this so we know who notified us. */
char *notifications[8];

struct entry *log_buffer_used = (void *)(uintptr_t)0x5200000;
struct entry *log_buffer_free = (void *)(uintptr_t)0x5201000;

void
new_log_buffer_entry_used(uint32_t packets, uint32_t ntfn, 
                            uint32_t left_free_queue, uint32_t left_used_queue,
                            uint32_t right_free_queue, uint32_t right_used_queue) 
{
    struct entry *entry = &log_buffer_used[head_used];
    head_used = (head_used + 1) % LOG_BUFFER_SIZE;

    ccnt_t val;
    SEL4BENCH_READ_CCNT(val);

    *entry = (struct entry) {
        .cycle_count = val,
        .packets_processed = packets,
        .notification = ntfn,
        .left_free_queue = left_free_queue,
        .left_used_queue = left_used_queue,
        .right_free_queue = right_free_queue,
        .right_used_queue = right_used_queue
    };
}

void
new_log_buffer_entry_free(uint32_t packets, uint32_t ntfn,
                            uint32_t left_free_queue, uint32_t left_used_queue,
                            uint32_t right_free_queue, uint32_t right_used_queue) 
{
    struct entry *entry = &log_buffer_free[head_free];
    head_free = (head_free + 1) % LOG_BUFFER_SIZE;

    ccnt_t val;
    SEL4BENCH_READ_CCNT(val);

    *entry = (struct entry) {
        .cycle_count = val,
        .packets_processed = packets,
        .notification = ntfn,
        .left_free_queue = left_free_queue,
        .left_used_queue = left_used_queue,
        .right_free_queue = right_free_queue,
        .right_used_queue = right_used_queue
    };
}

void
log_buffer_stop()
{
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        struct entry *entry = &log_buffer_free[i];
        if (entry->cycle_count == 0) continue;
        puthex64(entry->cycle_count);
        print(": ");
        print(sel4cp_name);
        print(" processed ");
        puthex64(entry->packets_processed);
        print(" free packets notified by ");
        print(notifications[entry->notification]);
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

    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        struct entry *entry = &log_buffer_used[i];
        if (entry->cycle_count == 0) continue;
        puthex64(entry->cycle_count);
        print(": ");
        print(sel4cp_name);
        print(" processed ");
        puthex64(entry->packets_processed);
        print(" used packets notified by ");
        print(notifications[entry->notification]);
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

    // clear all entries.
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        struct entry *entry = &log_buffer_free[i];
        entry->cycle_count = 0;
        entry->packets_processed = 0;
        
    }

    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        struct entry *entry = &log_buffer_used[i];
        entry->cycle_count = 0;
        entry->packets_processed = 0;
    }
}

