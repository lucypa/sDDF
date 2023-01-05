#include "timer.h"
#include "echo.h"

uintptr_t gpt_regs;

#define BIT(n) (1ul<<(n))

#define TIMER_BASE      0xc1100000
#define TIMER_MAP_BASE  0xc1109000

#define TIMER_REG_START   0x940    // TIMER_MUX

#define TIMER_A_INPUT_CLK 0
#define TIMER_E_INPUT_CLK 8
#define TIMER_A_EN      BIT(16)
#define TIMER_A_MODE    BIT(12)

#define TIMESTAMP_TIMEBASE_SYSTEM   0b000
#define TIMESTAMP_TIMEBASE_1_US     0b001
#define TIMESTAMP_TIMEBASE_10_US    0b010
#define TIMESTAMP_TIMEBASE_100_US   0b011
#define TIMESTAMP_TIMEBASE_1_MS     0b100

#define TIMEOUT_TIMEBASE_1_US   0b00
#define TIMEOUT_TIMEBASE_10_US  0b01
#define TIMEOUT_TIMEBASE_100_US 0b10
#define TIMEOUT_TIMEBASE_1_MS   0b11

struct timer_regs {
    uint32_t mux;
    uint32_t timer_a;
    uint32_t timer_b;
    uint32_t timer_c;
    uint32_t timer_d;
    uint32_t unused[13];
    uint32_t timer_e;
    uint32_t timer_e_hi;
    uint32_t mux1;
    uint32_t timer_f;
    uint32_t timer_g;
    uint32_t timer_h;
    uint32_t timer_i;
};

volatile struct timer_regs *regs;

#define LWIP_TICK_MS 10
#define NS_IN_MS 1000000ULL

int timers_initialised = 0;

static uint64_t get_ticks(void)
{
    uint64_t initial_high = regs->timer_e_hi;
    uint64_t low = regs->timer_e;
    uint64_t high = regs->timer_e_hi;
    if (high != initial_high) {
        low = regs->timer_e;
    }

    uint64_t ticks = (high << 32) | low;
    return ticks;
}

void irq(sel4cp_channel ch)
{
    regs->mux &= ~TIMER_A_EN;
    regs->mux &= ~TIMER_A_MODE; //|= TIMER_A_MODE;
    regs->timer_a = LWIP_TICK_MS; // * NS_IN_MS;
    regs->mux |= TIMER_A_EN;

    sys_check_timeouts();
}

u32_t sys_now(void)
{
    if (!timers_initialised) {
        /* lwip_init() will call this when initialising its own timers,
         * but the timer is not set up at this point so just return 0 */
        return 0;
    } else {
        uint64_t time_now = get_ticks();
        return time_now / NS_IN_MS;
    }
}

void gpt_init(void)
{
    regs = (void *)(gpt_regs + TIMER_REG_START);

    regs->mux = TIMER_A_EN | (TIMESTAMP_TIMEBASE_1_US << TIMER_E_INPUT_CLK) |
                       (TIMEOUT_TIMEBASE_1_MS << TIMER_A_INPUT_CLK);

    regs->timer_e = 0;

    // set a time out
    regs->mux &= ~TIMER_A_MODE; //|= TIMER_A_MODE;
    regs->timer_a = LWIP_TICK_MS; // * NS_IN_MS;
    regs->mux |= TIMER_A_EN;

    timers_initialised = 1;
    sel4cp_irq_ack(1);
    puthex64(get_ticks());
}