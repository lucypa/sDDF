#include <sel4/sel4.h>
#include <stdint.h>
#include "cache.h"
#include "util.h"

#define ROUND_DOWN(n, b) (((n) >> (b)) << (b))
#define LINE_START(a) ROUND_DOWN(a, CONFIG_L1_CACHE_LINE_SIZE_BITS)
#define LINE_INDEX(a) (LINE_START(a)>>CONFIG_L1_CACHE_LINE_SIZE_BITS)

static inline void
dsb(void)
{
    asm volatile("dsb sy" ::: "memory");
}

static inline void 
dmb(void)
{
    asm volatile("dmb sy" ::: "memory");
}

static inline void
cleanInvalByVA(unsigned long vaddr)
{
    asm volatile("dc civac, %0" : : "r"(vaddr));
    dsb();
}

static inline void
cleanByVA(unsigned long vaddr)
{
    puthex64(vaddr);
    unsigned long ctr_el0;
    asm volatile( "mrs	%0, ctr_el0" : "=r"(ctr_el0)); // read CTR_EL0
    puthex64(ctr_el0);
    asm volatile("dc cvac, %0" : : "r"(vaddr));
    dmb();
}

static inline void
invalidateByVA(unsigned long vaddr)
{
    asm volatile("dc ivac, %0" : : "r"(vaddr));
    dmb();
}

void
cleanInvalidateCache(unsigned long start, unsigned long end)
{
    unsigned long line;
    unsigned long index;
    /* Clean the L1 range */

    /* Finally clean and invalidate the L1 range. The extra clean is only strictly neccessary
     * in a multiprocessor environment to prevent a write being lost if another core is
     * attempting a store at the same time. As the range should already be clean asking
     * it to clean again should not affect performance */
    for (index = LINE_INDEX(start); index < LINE_INDEX(end) + 1; index++) {
        line = index << CONFIG_L1_CACHE_LINE_SIZE_BITS;
        cleanInvalByVA(line);
    }
}

void
cleanCache(unsigned long start, unsigned long end)
{
    unsigned long line;
    unsigned long index;

    for (index = LINE_INDEX(start); index < LINE_INDEX(end) + 1; index++) {
        line = index << CONFIG_L1_CACHE_LINE_SIZE_BITS;
        cleanByVA(line);
    }
}

void
invalidateCache(unsigned long start, unsigned long end)
{
    unsigned long line;
    unsigned long index;

    if (start != LINE_START(start)) {
        cleanCache(start, start);
    }
    if (end + 1 != LINE_START(end + 1)) {
        line = LINE_START(end);
        cleanCache(line, line);
    }

    /* Now invalidate L1 range */
    for (index = LINE_INDEX(start); index < LINE_INDEX(end) + 1; index++) {
        line = index << CONFIG_L1_CACHE_LINE_SIZE_BITS;
        invalidateByVA(line);
    }
    /* Ensure invalidate completes */
    dsb();
}
