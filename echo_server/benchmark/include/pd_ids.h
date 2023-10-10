#include "util.h"

#define PD_ETH_ID       1
#define PD_MUX_RX       2
#define PD_ARP          3
#define PD_COPY0        4
#define PD_CLIENT0      5
#define CORE_TOTALS     6
#define PD_TIMER        7
#define PD_MUX_TX       8
#define PD_ETH2         9
#define PD_COPY1        11
#define PD_CLIENT1      10

void print_pd_name(int pd_id) {
    switch (pd_id) {
        case PD_ETH_ID: print("ETH DRIVER"); break;
        case PD_MUX_RX: print("MUX RX"); break;
        case PD_MUX_TX: print("MUX TX"); break;
        case PD_ARP: print("ARP"); break;
        case PD_COPY0: print("COPY0"); break;
        case PD_CLIENT0: print("CLIENT0"); break;
        case PD_TIMER: print("TIMER"); break;
        case PD_ETH2: print("ETH CLI DRIVER"); break;
        case PD_COPY1: print("COPY1"); break;
        case PD_CLIENT1: print("CLIENT1"); break;
        default: print("CORE TOTALS");
    }
}
