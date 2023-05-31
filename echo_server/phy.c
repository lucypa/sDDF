#include "phy.h"

/**
 * get_phy_id - reads the specified addr for its ID.
 * @addr: PHY address on the MII bus
 * @phy_id: where to store the ID retrieved.
 *
 * Description: Reads the ID registers of the PHY at @addr on the
 *   @bus, stores it in @phy_id and returns zero on success.
 */
int
get_phy_id(struct eth_mac_regs *eth_mac, int addr, int devad, uint32_t *phy_id)
{
    int phy_reg;

    /* Grab the bits from PHYIR1, and put them
     * in the upper half */
    phy_reg = phy_read(eth_mac, addr, devad, MII_PHYSID1);

    if (phy_reg < 0) {
        return -1;
    }

    *phy_id = (phy_reg & 0xffff) << 16;

    /* Grab the bits from PHYIR2, and put them in the lower half */
    phy_reg = phy_read(eth_mac, addr, devad, MII_PHYSID2);

    if (phy_reg < 0) {
        return -1;
    }

    *phy_id |= (phy_reg & 0xffff);

    return 0;
}

int
get_phy_addr(struct eth_mac_regs *eth_mac)
{
    uint32_t phy_id = 0xffffffff;
    int mask;

    for (int i = 0; i < 5; i++) {
        mask = 0xffffffff;

        while (mask) {
            int addr = ffs(mask) - 1;
            int r = get_phy_id(eth_mac, addr, i ? i : MDIO_DEVAD_NONE, &phy_id);
            /* If the PHY ID is mostly f's, we didn't find anything */
            if (r == 0 && (phy_id & 0x1fffffff) != 0x1fffffff) {
                print("Phy addr: ");
                puthex64(addr);
                print("\n");
                return addr;
            }
            mask &= ~(1 << addr);
        }
    }
    print("Phy addr: -1");
    return -1;
}
                                    
void
udelay(uint32_t us)
{
    volatile int i;
    for(; us > 0; us--){
        for(i = 0; i < 100; i++){
        }
    }
}

int
phy_read(struct eth_mac_regs *eth_mac, int addr, int devad, int reg)
{
    uint16_t miiaddr;

    miiaddr = ((addr << MIIADDRSHIFT) & MII_ADDRMSK) |
              ((reg << MIIREGSHIFT) & MII_REGMSK);

    eth_mac->miiaddr = miiaddr | MII_CLKRANGE_150_250M | MII_BUSY;

    for (int i = 0; i < 10; i++) {
        if (!(eth_mac->miiaddr & MII_BUSY)) {
            return eth_mac->miidata;
        }
        udelay(100);
    };

    return -1;
}

int
phy_write(struct eth_mac_regs *eth_mac, int addr, int devad, int reg, uint32_t val)
{
    uint16_t miiaddr;
    int ret = -1;

    eth_mac->miidata = val;
    miiaddr = ((addr << MIIADDRSHIFT) & MII_ADDRMSK) |
              ((reg << MIIREGSHIFT) & MII_REGMSK) | MII_WRITE;

    eth_mac->miiaddr = miiaddr | MII_CLKRANGE_150_250M | MII_BUSY;

    for (int i = 0; i < 10; i++) {
        if (!(eth_mac->miiaddr & MII_BUSY)) {
            ret = 0;
            break;
        }
        udelay(100);
    };

    return ret;
}

void
phy_reset(struct eth_mac_regs *eth_mac, int phy_addr)
{
    int reg;
    int timeout = 500;
    int devad = MDIO_DEVAD_NONE;

    if (phy_write(eth_mac, phy_addr, devad, MII_BMCR, BMCR_RESET) < 0) {
        print("PHY reset failed\n");
        return;
    }

    /*
     * Poll the control register for the reset bit to go to 0 (it is
     * auto-clearing).  This should happen within 0.5 seconds per the
     * IEEE spec.
     */
    reg = phy_read(eth_mac, phy_addr, devad, MII_BMCR);
    while ((reg & BMCR_RESET) && timeout--) {
        reg = phy_read(eth_mac, phy_addr, devad, MII_BMCR);

        if (reg < 0) {
            print("PHY status read failed\n");
            return;
        }
        udelay(1000);
    }

    if (reg & BMCR_RESET) {
        print("PHY reset timed out\n");
        return;
    }

    return;
}

void
phy_config(struct eth_mac_regs *eth_mac, int phy_addr)
{
    uint16_t reg;

    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE, MII_BMCR, BMCR_RESET);

    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE,
              MIIM_RTL8211F_PAGE_SELECT, 0xd08);
    reg = phy_read(eth_mac, phy_addr, MDIO_DEVAD_NONE, 0x11);

    /* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
    reg &= ~MIIM_RTL8211F_TX_DELAY;

    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE, 0x11, reg);
    /* restore to default page 0 */
    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE,
              MIIM_RTL8211F_PAGE_SELECT, 0x0);

    /* Set green LED for Link, yellow LED for Active */
    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE,
              MIIM_RTL8211F_PAGE_SELECT, 0xd04);
    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE, 0x10, 0x617f);
    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE,
              MIIM_RTL8211F_PAGE_SELECT, 0x0);

    /* set up forced speed rather than aneg */

    return;
}

int
update_link(struct eth_mac_regs *eth_mac, int phy_addr)
{
    unsigned int mii_reg;

    /*
     * Wait if the link is up, and autonegotiation is in progress
     * (ie - we're capable and it's not done)
     */
    mii_reg = phy_read(eth_mac, phy_addr, MDIO_DEVAD_NONE, MII_BMSR);

    /*
     * If the link is up then
     * don't wait for autoneg again
     */
    if (mii_reg & BMSR_LSTATUS) {
        return 0;
    }

    /* Read the link a second time to clear the latched state */
    mii_reg = phy_read(eth_mac, phy_addr, MDIO_DEVAD_NONE, MII_BMSR);

    if (mii_reg & BMSR_LSTATUS) {
        return 0;
    }

    return -1;
}

uint32_t
phy_startup(struct eth_mac_regs *eth_mac, int phy_addr)
{
    if (update_link(eth_mac, phy_addr) < 0) {
        print("Link isn't up\n");
    }

    unsigned int speed;
    unsigned int mii_reg;
    uint32_t conf = 0;
    int i = 0;

    phy_write(eth_mac, phy_addr, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0xa43);
    mii_reg = phy_read(eth_mac, phy_addr, MDIO_DEVAD_NONE, MIIM_RTL8211F_PHY_STATUS);

    while (!(mii_reg & MIIM_RTL8211F_PHYSTAT_LINK)) {
        if (i > PHY_AUTONEGOTIATE_TIMEOUT) {
            print(" Status time out !\n");
            break;
        }

        if ((i++ % 1000) == 0) {
            print('.');
        }
        udelay(1000);
        mii_reg = phy_read(eth_mac, phy_addr, MDIO_DEVAD_NONE,
                           MIIM_RTL8211F_PHY_STATUS);
    }

    if (mii_reg & MIIM_RTL8211F_PHYSTAT_DUPLEX) {
        conf |= FULLDPLXMODE;
    }

    speed = (mii_reg & MIIM_RTL8211F_PHYSTAT_SPEED);

    if (speed != MIIM_RTL8211F_PHYSTAT_GBIT) {
        conf |= MII_PORTSELECT;
    } else {
        conf &= ~MII_PORTSELECT;
    }

    if (speed == MIIM_RTL8211F_PHYSTAT_100) {
        conf |= FES_100;
    }

    return conf;
}