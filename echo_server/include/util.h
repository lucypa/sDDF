/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define UART_REG(x) ((volatile uint32_t *)(UART_BASE + (x)))
#define UART_BASE 0x5000000 //0x30890000 in hardware on imx8mm. 
#define STAT 0x98
#define TRANSMIT 0x40
#define STAT_TDRE (1 << 14)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#ifdef __GNUC__
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (!!(x))
#define unlikely(x) (!!(x))
#endif

/*static void
putC(uint8_t ch)
{
    while (!(*UART_REG(STAT) & STAT_TDRE)) { }
    *UART_REG(TRANSMIT) = ch;
}*/

#define UART_WFIFO  0x0
#define UART_STATUS 0xC
#define UART_TX_FULL (1 << 21)

#define REG(x) ((volatile uint32_t *)(0x5000000 + 0x4c0 + (x)))

static void
putC(uint8_t ch)
{
    while ((*REG(UART_STATUS) & UART_TX_FULL));
    *REG(UART_WFIFO) = ch;
}

static void
print(const char *s)
{
    while (*s) {
        putC(*s);
        s++;
    }
}

static char
hexchar(unsigned int v)
{
    return v < 10 ? '0' + v : ('a' - 10) + v;
}

static void
puthex64(uint64_t val)
{
    char buffer[16 + 3];
    buffer[0] = '0';
    buffer[1] = 'x';
    buffer[16 + 3 - 1] = 0;
    for (unsigned i = 16 + 1; i > 1; i--) {
        buffer[i] = hexchar(val & 0xf);
        val >>= 4;
    }
    sel4cp_dbg_puts(buffer);
}
