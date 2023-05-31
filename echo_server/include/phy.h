#include "odroidc2.h"
#include "util.h"
#include <stdint.h>

#define PHY_RTL8211x_FORCE_MASTER BIT(1)
#define PHY_RTL8211E_PINE64_GIGABIT_FIX BIT(2)

#define PHY_AUTONEGOTIATE_TIMEOUT 5000

/* RTL8211x 1000BASE-T Control Register */
#define MIIM_RTL8211x_CTRL1000T_MSCE BIT(12);
#define MIIM_RTL8211x_CTRL1000T_MASTER BIT(11);

/* RTL8211x PHY Status Register */
#define MIIM_RTL8211x_PHY_STATUS       0x11
#define MIIM_RTL8211x_PHYSTAT_SPEED    0xc000
#define MIIM_RTL8211x_PHYSTAT_GBIT     0x8000
#define MIIM_RTL8211x_PHYSTAT_100      0x4000
#define MIIM_RTL8211x_PHYSTAT_DUPLEX   0x2000
#define MIIM_RTL8211x_PHYSTAT_SPDDONE  0x0800
#define MIIM_RTL8211x_PHYSTAT_LINK     0x0400

/* RTL8211x PHY Interrupt Enable Register */
#define MIIM_RTL8211x_PHY_INER         0x12
#define MIIM_RTL8211x_PHY_INTR_ENA     0x9f01
#define MIIM_RTL8211x_PHY_INTR_DIS     0x0000

/* RTL8211F PHY Status Register */
#define MIIM_RTL8211F_PHY_STATUS       0x1a
#define MIIM_RTL8211F_AUTONEG_ENABLE   0x1000
#define MIIM_RTL8211F_PHYSTAT_SPEED    0x0030
#define MIIM_RTL8211F_PHYSTAT_GBIT     0x0020
#define MIIM_RTL8211F_PHYSTAT_100      0x0010
#define MIIM_RTL8211F_PHYSTAT_DUPLEX   0x0008
#define MIIM_RTL8211F_PHYSTAT_SPDDONE  0x0800
#define MIIM_RTL8211F_PHYSTAT_LINK     0x0004

#define MIIM_RTL8211F_PAGE_SELECT      0x1f
#define MIIM_RTL8211F_TX_DELAY      0x100
#define MIIM_RTL8211F_LCR       0x10

#define MDIO_DEVAD_NONE         (-1)
#define BMSR_LSTATUS        0x0004  /* Link status             */

#define BMCR_RESET      0x8000  /* Reset the DP83840           */
#define MII_BMCR        0x00    /* Basic mode control register */
#define MII_BMSR        0x01    /* Basic mode status register  */
#define MII_PHYSID1     0x02    /* PHYS ID 1               */
#define MII_PHYSID2     0x03    /* PHYS ID 2  */

#define MII_BUSY        (1 << 0)
#define MII_WRITE       (1 << 1)

#define MII_CLKRANGE_150_250M   (0x10)
#define MIIADDRSHIFT    (11)
#define MIIREGSHIFT     (6)
#define MII_REGMSK      (0x1F << 6)
#define MII_ADDRMSK     (0x1F << 11)

int get_phy_addr(struct eth_mac_regs *eth_mac);
void phy_reset(struct eth_mac_regs *eth_mac, int phy_addr);
void phy_config(struct eth_mac_regs *eth_mac, int phy_addr);
int update_link(struct eth_mac_regs *eth_mac, int phy_addr);
uint32_t phy_startup(struct eth_mac_regs *eth_mac, int phy_addr);