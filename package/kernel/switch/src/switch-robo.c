/*
 * Broadcom BCM5325E/536x switch configuration module
 *
 * Copyright (C) 2005 Felix Fietkau <nbd@nbd.name>
 * Copyright (C) 2008 Michael Buesch <mb@bu3sch.de>
 * Copyright (C) 2013 Hauke Mehrtens <hauke@hauke-m.de>
 * Based on 'robocfg' by Oleg I. Vdovikin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#include "switch-core.h"
#include "etc53xx.h"

#ifdef CONFIG_BCM47XX
#include <bcm47xx_nvram.h>
#endif

#define DRIVER_NAME		"bcm53xx"
#define DRIVER_VERSION		"0.03"
#define PFX			"roboswitch: "

#define ROBO_PHY_ADDR		0x1E	/* robo switch phy address */
#define ROBO_PHY_ADDR_TG3	0x01	/* Tigon3 PHY address */
#define ROBO_PHY_ADDR_BCM63XX	0x00	/* BCM63XX PHY address */

/* MII registers */
#define REG_MII_PAGE	0x10	/* MII Page register */
#define REG_MII_ADDR	0x11	/* MII Address register */
#define REG_MII_DATA0	0x18	/* MII Data register 0 */

#define REG_MII_PAGE_ENABLE	1
#define REG_MII_ADDR_WRITE	1
#define REG_MII_ADDR_READ	2

/* Robo device ID register (in ROBO_MGMT_PAGE) */
#define ROBO_DEVICE_ID		0x30
#define  ROBO_DEVICE_ID_5325	0x25 /* Faked */
#define  ROBO_DEVICE_ID_5395	0x95
#define  ROBO_DEVICE_ID_5397	0x97
#define  ROBO_DEVICE_ID_5398	0x98
#define  ROBO_DEVICE_ID_53115	0x3115
#define  ROBO_DEVICE_ID_53125	0x3125

/* Private et.o ioctls */
#define SIOCGETCPHYRD           (SIOCDEVPRIVATE + 9)
#define SIOCSETCPHYWR           (SIOCDEVPRIVATE + 10)

/* Data structure for a Roboswitch device. */
struct robo_switch {
	char *device;			/* The device name string (ethX) */
	u16 devid;			/* ROBO_DEVICE_ID_53xx */
	bool is_5365;
	bool gmii;			/* gigabit mii */
	u8 corerev;
	int gpio_robo_reset;
	int gpio_lanports_enable;
	struct ifreq ifr;
	struct net_device *dev;
	unsigned char port[9];
};

/* Currently we can only have one device in the system. */
static struct robo_switch robo;


static int do_ioctl(int cmd)
{
	mm_segment_t old_fs = get_fs();
	int ret;

	set_fs(KERNEL_DS);
	ret = robo.dev->netdev_ops->ndo_do_ioctl(robo.dev, &robo.ifr, cmd);
	set_fs(old_fs);

	return ret;
}

static u16 mdio_read(__u16 phy_id, __u8 reg)
{
	struct mii_ioctl_data *mii = if_mii(&robo.ifr);
	int err;

	mii->phy_id = phy_id;
	mii->reg_num = reg;

	err = do_ioctl(SIOCGMIIREG);
	if (err < 0) {
		printk(KERN_ERR PFX "failed to read mdio reg %i with err %i.\n", reg, err);

		return 0xffff;
	}

	return mii->val_out;
}

static void mdio_write(__u16 phy_id, __u8 reg, __u16 val)
{
	struct mii_ioctl_data *mii = if_mii(&robo.ifr);
	int err;

	mii->phy_id = phy_id;
	mii->reg_num = reg;
	mii->val_in = val;

	err = do_ioctl(SIOCSMIIREG);
	if (err < 0) {
		printk(KERN_ERR PFX "failed to write mdio reg: %i with err %i.\n", reg, err);
		return;
	}
}

static int robo_reg(__u8 page, __u8 reg, __u8 op)
{
	int i = 3;

	/* set page number */
	mdio_write(ROBO_PHY_ADDR, REG_MII_PAGE,
		(page << 8) | REG_MII_PAGE_ENABLE);

	/* set register address */
	mdio_write(ROBO_PHY_ADDR, REG_MII_ADDR,
		(reg << 8) | op);

	/* check if operation completed */
	while (i--) {
		if ((mdio_read(ROBO_PHY_ADDR, REG_MII_ADDR) & 3) == 0)
			return 0;
	}

	printk(KERN_ERR PFX "timeout in robo_reg on page %i and reg %i with op %i.\n", page, reg, op);

	return 1;
}

/*
static void robo_read(__u8 page, __u8 reg, __u16 *val, int count)
{
	int i;

	robo_reg(page, reg, REG_MII_ADDR_READ);

	for (i = 0; i < count; i++)
		val[i] = mdio_read(ROBO_PHY_ADDR, REG_MII_DATA0 + i);
}
*/

static __u16 robo_read16(__u8 page, __u8 reg)
{
	robo_reg(page, reg, REG_MII_ADDR_READ);

	return mdio_read(ROBO_PHY_ADDR, REG_MII_DATA0);
}

static __u32 robo_read32(__u8 page, __u8 reg)
{
	robo_reg(page, reg, REG_MII_ADDR_READ);

	return mdio_read(ROBO_PHY_ADDR, REG_MII_DATA0) |
		(mdio_read(ROBO_PHY_ADDR, REG_MII_DATA0 + 1) << 16);
}

static void robo_write16(__u8 page, __u8 reg, __u16 val16)
{
	/* write data */
	mdio_write(ROBO_PHY_ADDR, REG_MII_DATA0, val16);

	robo_reg(page, reg, REG_MII_ADDR_WRITE);
}

static void robo_write32(__u8 page, __u8 reg, __u32 val32)
{
	/* write data */
	mdio_write(ROBO_PHY_ADDR, REG_MII_DATA0, val32 & 0xFFFF);
	mdio_write(ROBO_PHY_ADDR, REG_MII_DATA0 + 1, val32 >> 16);

	robo_reg(page, reg, REG_MII_ADDR_WRITE);
}

/* checks that attached switch is 5365 */
static bool robo_bcm5365(void)
{
	/* set vlan access id to 15 and read it back */
	__u16 val16 = 15;
	robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS, val16);

	/* 5365 will refuse this as it does not have this reg */
	return robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS) != val16;
}

static bool robo_gmii(void)
{
	if (mdio_read(0, ROBO_MII_STAT) & 0x0100)
		return ((mdio_read(0, 0x0f) & 0xf000) != 0);
	return false;
}

static int robo_switch_enable(void)
{
	unsigned int i, last_port;
	u16 val;
#ifdef CONFIG_BCM47XX
	char buf[20];
#endif

	val = robo_read16(ROBO_CTRL_PAGE, ROBO_SWITCH_MODE);
	if (!(val & (1 << 1))) {
		/* Unmanaged mode */
		val &= ~(1 << 0);
		/* With forwarding */
		val |= (1 << 1);
		robo_write16(ROBO_CTRL_PAGE, ROBO_SWITCH_MODE, val);
		val = robo_read16(ROBO_CTRL_PAGE, ROBO_SWITCH_MODE);
		if (!(val & (1 << 1))) {
			printk(KERN_ERR PFX "Failed to enable switch\n");
			return -EBUSY;
		}

		/* No spanning tree for unmanaged mode */
		last_port = (robo.devid == ROBO_DEVICE_ID_5398) ?
				ROBO_PORT7_CTRL : ROBO_PORT4_CTRL;
		for (i = ROBO_PORT0_CTRL; i <= last_port; i++)
			robo_write16(ROBO_CTRL_PAGE, i, 0);

		/* No spanning tree on IMP port too */
		robo_write16(ROBO_CTRL_PAGE, ROBO_IM_PORT_CTRL, 0);
	}

	if (robo.devid == ROBO_DEVICE_ID_53125) {
		/* Make IM port status link by default */
		val = robo_read16(ROBO_CTRL_PAGE, ROBO_PORT_OVERRIDE_CTRL) | 0xb1;
		robo_write16(ROBO_CTRL_PAGE, ROBO_PORT_OVERRIDE_CTRL, val);
		// TODO: init EEE feature
	}

#ifdef CONFIG_BCM47XX
	/* WAN port LED, except for Netgear WGT634U */
	if (bcm47xx_nvram_getenv("nvram_type", buf, sizeof(buf)) >= 0) {
		if (strcmp(buf, "cfe") != 0)
			robo_write16(ROBO_CTRL_PAGE, 0x16, 0x1F);
	}
#endif
	return 0;
}

static void robo_switch_reset(void)
{
	if ((robo.devid == ROBO_DEVICE_ID_5395) ||
	    (robo.devid == ROBO_DEVICE_ID_5397) ||
	    (robo.devid == ROBO_DEVICE_ID_5398)) {
		/* Trigger a software reset. */
		robo_write16(ROBO_CTRL_PAGE, 0x79, 0x83);
		mdelay(500);
		robo_write16(ROBO_CTRL_PAGE, 0x79, 0);
	}
}

#ifdef CONFIG_BCM47XX
static int get_gpio_pin(const char *name)
{
	int i, err;
	char nvram_var[10];
	char buf[30];

	for (i = 0; i < 16; i++) {
		err = snprintf(nvram_var, sizeof(nvram_var), "gpio%i", i);
		if (err <= 0)
			continue;
		err = bcm47xx_nvram_getenv(nvram_var, buf, sizeof(buf));
		if (err <= 0)
			continue;
		if (!strcmp(name, buf))
			return i;
	}
	return -1;
}
#endif

static int robo_probe(char *devname)
{
	__u32 phyid;
	unsigned int i;
	int err = -1;
	struct mii_ioctl_data *mii;

	printk(KERN_INFO PFX "Probing device '%s'\n", devname);
	strcpy(robo.ifr.ifr_name, devname);

	if ((robo.dev = dev_get_by_name(&init_net, devname)) == NULL) {
		printk(KERN_ERR PFX "No such device\n");
		err = -ENODEV;
		goto err_done;
	}
	if (!robo.dev->netdev_ops || !robo.dev->netdev_ops->ndo_do_ioctl) {
		printk(KERN_ERR PFX "ndo_do_ioctl not implemented in ethernet driver\n");
		err = -ENXIO;
		goto err_put;
	}

	robo.device = devname;

	/* try access using MII ioctls - get phy address */
	err = do_ioctl(SIOCGMIIPHY);
	if (err < 0) {
		printk(KERN_ERR PFX "error (%i) while accessing MII phy registers with ioctls\n", err);
		goto err_put;
	}

	/* got phy address check for robo address */
	mii = if_mii(&robo.ifr);
	if ((mii->phy_id != ROBO_PHY_ADDR) &&
	    (mii->phy_id != ROBO_PHY_ADDR_BCM63XX) &&
	    (mii->phy_id != ROBO_PHY_ADDR_TG3)) {
		printk(KERN_ERR PFX "Invalid phy address (%d)\n", mii->phy_id);
		err = -ENODEV;
		goto err_put;
	}

#ifdef CONFIG_BCM47XX
	robo.gpio_lanports_enable = get_gpio_pin("lanports_enable");
	if (robo.gpio_lanports_enable >= 0) {
		err = gpio_request(robo.gpio_lanports_enable, "lanports_enable");
		if (err) {
			printk(KERN_ERR PFX "error (%i) requesting lanports_enable gpio (%i)\n",
			       err, robo.gpio_lanports_enable);
			goto err_put;
		}
		gpio_direction_output(robo.gpio_lanports_enable, 1);
		mdelay(5);
	}

	robo.gpio_robo_reset = get_gpio_pin("robo_reset");
	if (robo.gpio_robo_reset >= 0) {
		err = gpio_request(robo.gpio_robo_reset, "robo_reset");
		if (err) {
			printk(KERN_ERR PFX "error (%i) requesting robo_reset gpio (%i)\n",
			       err, robo.gpio_robo_reset);
			goto err_gpio_robo;
		}
		gpio_set_value(robo.gpio_robo_reset, 0);
		gpio_direction_output(robo.gpio_robo_reset, 1);
		gpio_set_value(robo.gpio_robo_reset, 0);
		mdelay(50);

		gpio_set_value(robo.gpio_robo_reset, 1);
		mdelay(20);
	} else {
		// TODO: reset the internal robo switch
	}
#endif

	phyid = mdio_read(ROBO_PHY_ADDR, 0x2) |
		(mdio_read(ROBO_PHY_ADDR, 0x3) << 16);

	if (phyid == 0xffffffff || phyid == 0x55210022) {
		printk(KERN_ERR PFX "No Robo switch in managed mode found, phy_id = 0x%08x\n", phyid);
		err = -ENODEV;
		goto err_gpio_lanports;
	}

	/* Get the device ID */
	for (i = 0; i < 10; i++) {
		robo.devid = robo_read16(ROBO_MGMT_PAGE, ROBO_DEVICE_ID);
		if (robo.devid)
			break;
		udelay(10);
	}
	if (!robo.devid)
		robo.devid = ROBO_DEVICE_ID_5325; /* Fake it */
	if (robo.devid == ROBO_DEVICE_ID_5325)
		robo.is_5365 = robo_bcm5365();
	else
		robo.is_5365 = false;

	robo.gmii = robo_gmii();
	if (robo.devid == ROBO_DEVICE_ID_5325) {
		for (i = 0; i < 5; i++)
			robo.port[i] = i;
	} else {
		for (i = 0; i < 8; i++)
			robo.port[i] = i;
	}
	robo.port[i] = ROBO_IM_PORT_CTRL;

	robo_switch_reset();
	err = robo_switch_enable();
	if (err)
		goto err_gpio_lanports;

	printk(KERN_INFO PFX "found a 5%s%x!%s at %s\n", robo.devid & 0xff00 ? "" : "3", robo.devid,
		robo.is_5365 ? " It's a BCM5365." : "", devname);

	return 0;

err_gpio_lanports:
	if (robo.gpio_lanports_enable >= 0)
		gpio_free(robo.gpio_lanports_enable);
err_gpio_robo:
	if (robo.gpio_robo_reset >= 0)
		gpio_free(robo.gpio_robo_reset);
err_put:
	dev_put(robo.dev);
	robo.dev = NULL;
err_done:
	return err;
}

static int handle_vlan_port_read_old(switch_driver *d, char *buf, int nr)
{
	__u16 val16;
	int len = 0;
	int j;

	val16 = (nr) /* vlan */ | (0 << 12) /* read */ | (1 << 13) /* enable */;

	if (robo.is_5365) {
		robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS_5365, val16);
		/* actual read */
		val16 = robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_READ);
		if ((val16 & (1 << 14)) /* valid */) {
			for (j = 0; j < d->ports; j++) {
				if (val16 & (1 << j)) {
					len += sprintf(buf + len, "%d", j);
					if (val16 & (1 << (j + 7))) {
						if (j == d->cpuport)
							buf[len++] = 'u';
					} else {
						buf[len++] = 't';
						if (robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_PORT0_DEF_TAG + (j << 1)) == nr)
							buf[len++] = '*';
					}
					buf[len++] = '\t';
				}
			}
			len += sprintf(buf + len, "\n");
		}
	} else {
		u32 val32;
		robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS, val16);
		/* actual read */
		val32 = robo_read32(ROBO_VLAN_PAGE, ROBO_VLAN_READ);
		if ((val32 & (1 << 20)) /* valid */) {
			for (j = 0; j < d->ports; j++) {
				if (val32 & (1 << j)) {
					len += sprintf(buf + len, "%d", j);
					if (val32 & (1 << (j + d->ports))) {
						if (j == d->cpuport)
							buf[len++] = 'u';
					} else {
						buf[len++] = 't';
						if (robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_PORT0_DEF_TAG + (j << 1)) == nr)
							buf[len++] = '*';
					}
					buf[len++] = '\t';
				}
			}
			len += sprintf(buf + len, "\n");
		}
	}

	buf[len] = '\0';

	return len;
}

static int handle_vlan_port_read_new(switch_driver *d, char *buf, int nr)
{
	__u8 vtbl_entry, vtbl_index, vtbl_access;
	__u32 val32;
	int len = 0;
	int j;

	if ((robo.devid == ROBO_DEVICE_ID_5395) ||
	    (robo.devid == ROBO_DEVICE_ID_53115) ||
	    (robo.devid == ROBO_DEVICE_ID_53125)) {
		vtbl_access = ROBO_VTBL_ACCESS_5395;
		vtbl_index = ROBO_VTBL_INDX_5395;
		vtbl_entry = ROBO_VTBL_ENTRY_5395;
	} else {
		vtbl_access = ROBO_VTBL_ACCESS;
		vtbl_index = ROBO_VTBL_INDX;
		vtbl_entry = ROBO_VTBL_ENTRY;
	}

	robo_write16(ROBO_ARLIO_PAGE, vtbl_index, nr);
	robo_write16(ROBO_ARLIO_PAGE, vtbl_access, (1 << 7) | (1 << 0));
	val32 = robo_read32(ROBO_ARLIO_PAGE, vtbl_entry);
	for (j = 0; j < d->ports; j++) {
		if (val32 & (1 << j)) {
			len += sprintf(buf + len, "%d", j);
			if (val32 & (1 << (j + d->ports))) {
				if (j == d->cpuport)
					buf[len++] = 'u';
			} else {
				buf[len++] = 't';
				if (robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_PORT0_DEF_TAG + (j << 1)) == nr)
					buf[len++] = '*';
			}
			buf[len++] = '\t';
		}
	}
	len += sprintf(buf + len, "\n");
	buf[len] = '\0';
	return len;
}

static int handle_vlan_port_read(void *driver, char *buf, int nr)
{
	switch_driver *d = (switch_driver *) driver;

	if (robo.devid != ROBO_DEVICE_ID_5325)
		return handle_vlan_port_read_new(d, buf, nr);
	else
		return handle_vlan_port_read_old(d, buf, nr);
}

static void handle_vlan_port_write_old(switch_driver *d, switch_vlan_config *c, int nr)
{
	__u16 val16;
	__u32 val32;
	__u32 untag = ((c->untag  & ~(1 << d->cpuport)) << d->ports);

	/* write config now */
	val16 = (nr) /* vlan */ | (1 << 12) /* write */ | (1 << 13) /* enable */;
	if (robo.is_5365) {
		robo_write32(ROBO_VLAN_PAGE, ROBO_VLAN_WRITE_5365,
			(1 << 14)  /* valid */ | (untag << 1 ) | c->port);
		robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS_5365, val16);
	} else {
		if (robo.corerev < 3)
			val32 = (1 << 20) | ((nr >> 4) << 12) | untag | c->port;
		else
			val32 = (1 << 24) | (nr << 12) | untag | c->port;
		robo_write32(ROBO_VLAN_PAGE, ROBO_VLAN_WRITE, val32);
		robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS, val16);
	}
}

static void handle_vlan_port_write_new(switch_driver *d, switch_vlan_config *c, int nr)
{
	__u8 vtbl_entry, vtbl_index, vtbl_access;
	__u32 untag = ((c->untag  & ~(1 << d->cpuport)) << d->ports);

	/* write config now */
	if ((robo.devid == ROBO_DEVICE_ID_5395) ||
	    (robo.devid == ROBO_DEVICE_ID_53115) ||
	    (robo.devid == ROBO_DEVICE_ID_53125)) {
		vtbl_access = ROBO_VTBL_ACCESS_5395;
		vtbl_index = ROBO_VTBL_INDX_5395;
		vtbl_entry = ROBO_VTBL_ENTRY_5395;
	} else {
		vtbl_access = ROBO_VTBL_ACCESS;
		vtbl_index = ROBO_VTBL_INDX;
		vtbl_entry = ROBO_VTBL_ENTRY;
	}

	robo_write32(ROBO_ARLIO_PAGE, vtbl_entry, untag | c->port);
	robo_write16(ROBO_ARLIO_PAGE, vtbl_index, nr);
	robo_write16(ROBO_ARLIO_PAGE, vtbl_access, 1 << 7);
}

static int handle_vlan_port_write(void *driver, char *buf, int nr)
{
	switch_driver *d = (switch_driver *)driver;
	switch_vlan_config *c = switch_parse_vlan(d, buf);
	int j;

	if (c == NULL)
		return -EINVAL;

	for (j = 0; j < d->ports; j++) {
		if ((c->untag | c->pvid) & (1 << j)) {
			/* change default vlan tag */
			robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_PORT0_DEF_TAG + (j << 1), nr);
		}
	}

	if (robo.devid != ROBO_DEVICE_ID_5325)
		handle_vlan_port_write_new(d, c, nr);
	else
		handle_vlan_port_write_old(d, c, nr);

	kfree(c);
	return 0;
}

#define set_switch(state) \
	robo_write16(ROBO_CTRL_PAGE, ROBO_SWITCH_MODE, (robo_read16(ROBO_CTRL_PAGE, ROBO_SWITCH_MODE) & ~2) | (state ? 2 : 0));

static int handle_enable_read(void *driver, char *buf, int nr)
{
	return sprintf(buf, "%d\n", (((robo_read16(ROBO_CTRL_PAGE, ROBO_SWITCH_MODE) & 2) == 2) ? 1 : 0));
}

static int handle_enable_write(void *driver, char *buf, int nr)
{
	set_switch(buf[0] == '1');

	return 0;
}

static int handle_port_enable_read(void *driver, char *buf, int nr)
{
	return sprintf(buf, "%d\n", ((robo_read16(ROBO_CTRL_PAGE, robo.port[nr]) & 3) == 3 ? 0 : 1));
}

static int handle_port_enable_write(void *driver, char *buf, int nr)
{
	u16 val16;

	if (buf[0] == '0')
		val16 = 3; /* disabled */
	else if (buf[0] == '1')
		val16 = 0; /* enabled */
	else
		return -EINVAL;

	robo_write16(ROBO_CTRL_PAGE, robo.port[nr],
		(robo_read16(ROBO_CTRL_PAGE, robo.port[nr]) & ~3) | val16);

	return 0;
}

static int handle_port_media_read(void *driver, char *buf, int nr)
{
	u16 bmcr = mdio_read(robo.port[nr], MII_BMCR);
	int media, len;

	if (bmcr & BMCR_ANENABLE)
		media = SWITCH_MEDIA_AUTO;
	else {
		if (bmcr & BMCR_SPEED1000)
			media = SWITCH_MEDIA_1000;
		else if (bmcr & BMCR_SPEED100)
			media = SWITCH_MEDIA_100;
		else
			media = 0;

		if (bmcr & BMCR_FULLDPLX)
			media |= SWITCH_MEDIA_FD;
	}

	len = switch_print_media(buf, media);
	return len + sprintf(buf + len, "\n");
}

static int handle_port_media_write(void *driver, char *buf, int nr)
{
	int media = switch_parse_media(buf);
	u16 bmcr, bmcr_mask;

	if (media & SWITCH_MEDIA_AUTO)
		bmcr = BMCR_ANENABLE | BMCR_ANRESTART;
	else {
		if (media & SWITCH_MEDIA_1000) {
			if (!robo.gmii)
				return -EINVAL;
			bmcr = BMCR_SPEED1000;
		}
		else if (media & SWITCH_MEDIA_100)
			bmcr = BMCR_SPEED100;
		else
			bmcr = 0;

		if (media & SWITCH_MEDIA_FD)
			bmcr |= BMCR_FULLDPLX;
	}

	bmcr_mask = ~(BMCR_SPEED1000 | BMCR_SPEED100 | BMCR_FULLDPLX | BMCR_ANENABLE | BMCR_ANRESTART);
	mdio_write(robo.port[nr], MII_BMCR,
		(mdio_read(robo.port[nr], MII_BMCR) & bmcr_mask) | bmcr);

	return 0;
}

static int handle_enable_vlan_read(void *driver, char *buf, int nr)
{
	return sprintf(buf, "%d\n", (((robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_CTRL0) & (1 << 7)) == (1 << 7)) ? 1 : 0));
}

static int handle_enable_vlan_write(void *driver, char *buf, int nr)
{
	__u16 val16;
	int disable = ((buf[0] != '1') ? 1 : 0);

	val16 = robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_CTRL0);
	robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_CTRL0, disable ? 0 :
		val16 | (1 << 7) /* 802.1Q VLAN */ | (3 << 5) /* mac check and hash */);

	val16 = robo_read16(ROBO_VLAN_PAGE, ROBO_VLAN_CTRL1);
	robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_CTRL1, disable ? 0 :
		val16 | (robo.devid == ROBO_DEVICE_ID_5325 ? (1 << 1) :
		0) | (1 << 2) | (1 << 3)); /* RSV multicast */

	if (robo.devid != ROBO_DEVICE_ID_5325)
		return 0;

	robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_CTRL4, disable ? 0 :
		(1 << 6) /* drop invalid VID frames */);
	robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_CTRL5, disable ? 0 :
		(1 << 3) /* drop miss V table frames */);

	return 0;
}

static void handle_reset_old(switch_driver *d, char *buf, int nr)
{
	int j;
	__u16 val16;

	/* reset vlans */
	for (j = 0; j <= ((robo.is_5365) ? VLAN_ID_MAX_5365 : VLAN_ID_MAX); j++) {
		/* write config now */
		val16 = (j) /* vlan */ | (1 << 12) /* write */ | (1 << 13) /* enable */;
		if (robo.is_5365)
			robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_WRITE_5365, 0);
		else
			robo_write32(ROBO_VLAN_PAGE, ROBO_VLAN_WRITE, 0);
		robo_write16(ROBO_VLAN_PAGE, robo.is_5365 ? ROBO_VLAN_TABLE_ACCESS_5365 :
							    ROBO_VLAN_TABLE_ACCESS,
			     val16);
	}
}

static void handle_reset_new(switch_driver *d, char *buf, int nr)
{
	int j;
	__u8 vtbl_entry, vtbl_index, vtbl_access;

	if ((robo.devid == ROBO_DEVICE_ID_5395) ||
	    (robo.devid == ROBO_DEVICE_ID_53115) ||
	    (robo.devid == ROBO_DEVICE_ID_53125)) {
		vtbl_access = ROBO_VTBL_ACCESS_5395;
		vtbl_index = ROBO_VTBL_INDX_5395;
		vtbl_entry = ROBO_VTBL_ENTRY_5395;
	} else {
		vtbl_access = ROBO_VTBL_ACCESS;
		vtbl_index = ROBO_VTBL_INDX;
		vtbl_entry = ROBO_VTBL_ENTRY;
	}

	for (j = 0; j <= VLAN_ID_MAX; j++) {
		/* write config now */
		robo_write32(ROBO_ARLIO_PAGE, vtbl_entry, 0);
		robo_write16(ROBO_ARLIO_PAGE, vtbl_index, j);
		robo_write16(ROBO_ARLIO_PAGE, vtbl_access, 1 << 7);
	}
}

static int handle_reset(void *driver, char *buf, int nr)
{
	int j;
	switch_driver *d = (switch_driver *) driver;

	/* disable switching */
	set_switch(0);

	if (robo.devid != ROBO_DEVICE_ID_5325)
		handle_reset_new(d, buf, nr);
	else
		handle_reset_old(d, buf, nr);

	/* reset ports to a known good state */
	for (j = 0; j < d->ports; j++) {
		robo_write16(ROBO_CTRL_PAGE, robo.port[j], 0x0000);
		robo_write16(ROBO_VLAN_PAGE, ROBO_VLAN_PORT0_DEF_TAG + (j << 1), 0);
	}

	/* enable switching */
	set_switch(1);

	/* enable vlans */
	handle_enable_vlan_write(driver, "1", 0);

	return 0;
}

static int __init robo_init(void)
{
	int notfound = 1;
	char *device;

	device = strdup("ethX");
	for (device[3] = '0'; (device[3] <= '3') && notfound; device[3]++) {
		if (! switch_device_registered (device))
			notfound = robo_probe(device);
	}
	device[3]--;

	if (notfound) {
		kfree(device);
		return -ENODEV;
	} else {
		static const switch_config cfg[] = {
			{
				.name	= "enable",
				.read	= handle_enable_read,
				.write	= handle_enable_write
			}, {
				.name	= "enable_vlan",
				.read	= handle_enable_vlan_read,
				.write	= handle_enable_vlan_write
			}, {
				.name	= "reset",
				.read	= NULL,
				.write	= handle_reset
			}, { NULL, },
		};
		static const switch_config port[] = {
			{
				.name	= "enable",
				.read	= handle_port_enable_read,
				.write	= handle_port_enable_write
			}, {
				.name	= "media",
				.read	= handle_port_media_read,
				.write	= handle_port_media_write
			}, { NULL, },
		};
		static const switch_config vlan[] = {
			{
				.name	= "ports",
				.read	= handle_vlan_port_read,
				.write	= handle_vlan_port_write
			}, { NULL, },
		};
		switch_driver driver = {
			.name			= DRIVER_NAME,
			.version		= DRIVER_VERSION,
			.interface		= device,
			.cpuport		= 5,
			.ports			= 6,
			.vlans			= 16,
			.driver_handlers	= cfg,
			.port_handlers		= port,
			.vlan_handlers		= vlan,
		};
		if (robo.devid != ROBO_DEVICE_ID_5325) {
			driver.ports = 9;
			driver.cpuport = 8;
		}
		if (robo.is_5365)
			snprintf(driver.dev_name, SWITCH_NAME_BUFSZ, "BCM5365");
		else
			snprintf(driver.dev_name, SWITCH_NAME_BUFSZ, "BCM5%s%x", robo.devid & 0xff00 ? "" : "3", robo.devid);

		return switch_register_driver(&driver);
	}
}

static void __exit robo_exit(void)
{
	switch_unregister_driver(DRIVER_NAME);
	if (robo.dev)
		dev_put(robo.dev);
	if (robo.gpio_robo_reset >= 0)
		gpio_free(robo.gpio_robo_reset);
	if (robo.gpio_lanports_enable >= 0)
		gpio_free(robo.gpio_lanports_enable);
	kfree(robo.device);
}


MODULE_AUTHOR("Felix Fietkau <openwrt@nbd.name>");
MODULE_LICENSE("GPL");

module_init(robo_init);
module_exit(robo_exit);
