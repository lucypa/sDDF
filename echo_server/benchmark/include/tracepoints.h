#include "sel4bench.h"
#include "bench.h"
#include "util.h"

#define NUM_TRACES 10

typedef struct trace_point {
    const char *name;
    uint64_t start;
    uint64_t sum;
    uint64_t num_tripped;
} trace_point_t;

static trace_point_t trace_points[NUM_TRACES];
static int num_tp = 0;

#define TRACE_START(num) do { \
        if (num < NUM_TRACES) { \
            ccnt_t val; \
            SEL4BENCH_READ_CCNT(val); \
            trace_points[num].start = val; \
        } \
} while (0)

#define TRACE_END_COUNT(num, count) do { \
        if (num < NUM_TRACES) { \
            ccnt_t val; \
            SEL4BENCH_READ_CCNT(val); \
            trace_points[num].sum += val - trace_points[num].start; \
            trace_points[num].num_tripped += count; \
        } \
} while (0)

#define TRACE_END(num) TRACE_END_COUNT(num, 1)

void trace_start(void) {
    for (int i = 0; i < num_tp; i++) {
        trace_points[i].start = 0;
        trace_points[i].sum = 0;
        trace_points[i].num_tripped = 0;
    }
}

void trace_stop(void) {
    print("traces:");
    print(sel4cp_name);
    print("\n");

    for (int i = 0; i < num_tp; i++) {
        if (trace_points[i].name != NULL) {
            print(trace_points[i].name);
        }
        print("\n");
        print("Num_tripped: ");
        puthex64(trace_points[i].num_tripped);
        print("\nCycles: ");
        puthex64(trace_points[i].sum);
        print("\n");
    }
}

int trace_point_register_name(int tp_id, const char *name)
{
    if (name == NULL) {
        print("Invalid name provided to trace_extra_point_register");
        return -1;
    }

    if (tp_id < 0 || tp_id >= NUM_TRACES) {
        print("Invalid tp ID");
        return -1;
    }

    if (trace_points[tp_id].name != NULL) {
        print("Overwriting an existing trace point");
        return -1;
    }

    trace_points[tp_id].name = name;
    num_tp++;

    return 0;
}
