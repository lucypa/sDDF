#include "sel4bench.h"
#include "bench.h"
#include "util.h"

uintptr_t log_buffer;
int log_head = 0;

#define LOG_BUFFER_SIZE 10

struct entry *log = (void *)(uintptr_t)0x6000000;

struct entry {
    uint64_t cycle_count;
    uint32_t packets_processed;
    uint32_t notification;
    uint32_t left_free_queue;
    uint32_t left_used_queue;
    uint32_t right_free_queue;
    uint32_t right_used_queue;
};

void
new_log_buffer_entry(uint32_t packets, uint32_t ntfn, 
                            uint32_t left_free_queue, uint32_t left_used_queue,
                            uint32_t right_free_queue, uint32_t right_used_queue) 
{
    struct entry *entry = &log[log_head];
    log_head = (log_head + 1) % LOG_BUFFER_SIZE;

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
