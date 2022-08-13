/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <autoconf.h>
#include <sel4cp.h>

#include "lwip/dhcp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"

void gpt_init(void);
u32_t sys_now(void);
void irq(sel4cp_channel ch);
