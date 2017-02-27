/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <debug.h>
#include <plat_config.h>
#include <mmio.h>
#include <sys/errno.h>
#include "sunxi_def.h"
#include "sunxi_private.h"

#define BIT(n) (1U << (n))

#define RUNTIME_ADDR	0x2d
#define AXP803_HW_ADDR	0x3a3

static int axp803_set_cpu_voltage(int millivolt)
{
	uint8_t reg;

	if (millivolt <= 0) {			/* power off system */
		sunxi_rsb_write(0x32, sunxi_rsb_read(0x32) | 0x80);
		return 0;			/* hopefully not ... */
	}

	if (millivolt < 800 || millivolt > 1300)
		return -1;

	if (millivolt > 1200)
		reg = (millivolt - 1200) / 20 + 70;
	else
		reg = (millivolt - 500) / 10 + 0;

	sunxi_rsb_write(0x21, reg);	/* DCDC2 */

	while (!(sunxi_rsb_read(0x21) & 0x80))
		;

	return 0;
}

/*
 * Initial PMIC setup for boards using the AXP803 PMIC.
 * DCDC1 must be corrected to 3.3 volts. Also we enable:
 * - DC1SW: Ethernet PHY on most boards
 * - DLDO1: HDMI power
 * - DLDO4: WiFi power
 * Technically those should be enabled by the users (via SCPI), but until
 * U-Boot learns how to do this we do it here.
 * Also this contains a quirk to fix the DRAM voltage on Pine64 boards,
 * which have a wrong default (1.24V instead of 1.36V).
 */
static int axp803_initial_setup(void)
{
	int ret;

	ret = sunxi_rsb_read(0x20);
	if (ret != 0x0e && ret != 0x11) {
		int voltage = (ret & 0x1f) * 10 + 16;

		NOTICE("PMIC: DCDC1 voltage is an unexpected %d.%dV\n",
		       voltage / 10, voltage % 10);
		return -1;
	}

	if (ret != 0x11) {
		/* Set DCDC1 voltage to 3.3 Volts */
		ret = sunxi_rsb_write(0x20, 0x11);
		if (ret < 0) {
			NOTICE("PMIC: error %d writing DCDC1 voltage\n", ret);
			return -2;
		}
	}

	ret = sunxi_rsb_read(0x12);
	if ((ret & 0x37) != 0x01) {
		NOTICE("PMIC: Output power control 2 is an unexpected 0x%x\n",
		       ret);
		return -3;
	}

	if ((ret & 0xc9) != 0xc9) {
		/* Enable DC1SW to power PHY, DLDO4 for WiFi, DLDO1 for HDMI */
		/* TODO: keep WiFi disabled, as not needed in U-Boot? */
		ret = sunxi_rsb_write(0x12, ret | 0xc8);
		if (ret < 0) {
			NOTICE("PMIC: error %d enabling DC1SW/DLDO4/DLDO1\n",
			       ret);
			return -4;
		}
	}

	/*
	 * On the Pine64 the AXP is wired wrongly: to reset DCDC5 to 1.24V.
	 * However the DDR3L chips require 1.36V instead. Fix this up. Other
	 * boards hopefully do the right thing here and don't require any
	 * changes. This should be further confined once we are able to
	 * reliably detect a Pine64 board.
	 */
	ret = sunxi_rsb_read(0x24);	/* read DCDC5 register */
	if ((ret & 0x7f) == 0x26) {	/* check for 1.24V value */
		NOTICE("PMIC: fixing DRAM voltage from 1.24V to 1.36V\n");
		sunxi_rsb_write(0x24, 0x2c);
	}
 
	sunxi_rsb_write(0x15, 0x1a);	/* DLDO1 = VCC3V3_HDMI voltage = 3.3V */

	ret = sunxi_rsb_read(0x14);
	sunxi_rsb_write(0x14, ret | 0x40);	/* DCDC2/3 dual phase */

	axp803_set_cpu_voltage(1100);

	return 0;
}

/*
 * Program the AXP803 via the RSB bus.
 */
static int axp803_probe(void)
{
	int ret;

	ret = sunxi_rsb_init();
	if (ret && ret != -EEXIST) {
		ERROR("Could not init RSB controller.\n");
		return -1;
	}

	if (ret == -EEXIST)
		return ret;

	ret = sunxi_rsb_configure(AXP803_HW_ADDR, RUNTIME_ADDR);
	if (ret) {
		ERROR("Could not configure RSB.\n");
		return -2;
	}
	ret = sunxi_rsb_read(0x03);
	if (ret < 0) {
		ERROR("PMIC: error %d reading PMIC type\n", ret);
		return -2;
	}
	if ((ret & 0xcf) != 0x41) {
		ERROR("PMIC: unknown PMIC type number 0x%x\n", ret);
		return -3;
	}

	return 0;
}

enum pmic_type {
	PMIC_AXP803,
} pmic_type;

int sunxi_power_set_cpu_voltage(int millivolt)
{
	switch (pmic_type) {
	case PMIC_AXP803:
		return axp803_set_cpu_voltage(millivolt);
	}

	return -ENODEV;
}

/*
 * device mapping: blocks of 8 (or 16?)
 * block  0: DCDCn
 * block  1: special (DC1SW, ...)
 * block  2: ALDO
 * block  3: DLDO
 * block  4: ELDO
 * block  5: FLDO
 * block  6: GPIO
 *
 * state: == 0: power off
 *        != 0: power on
 */

static uint32_t axp803_map_devices(uint16_t devid)
{
	unsigned int reg, bit;

	switch (devid) {
	case  0:		/* DCDC */
	case  1:
	case  2:
	case  3:
	case  4:
	case  5:
		reg = 0x10;
		bit = devid;
		break;
	case  8:		/* DC1SW */
		reg = 0x12;
		bit = 7;
		break;
	case 16:		/* ALDO */
	case 17:
	case 18:
		reg = 0x13;
		bit = (devid & 7) + 5;
		break;
	case 24:		/* DLDO */
	case 25:
	case 26:
	case 27:
		reg = 0x12;
		bit = (devid & 7) + 3;
		break;
	case 32:		/* ELDO */
	case 33:
	case 34:
		reg = 0x13;
		bit = (devid & 7) + 0;
		break;
	case 40:		/* FLDO */
	case 41:
		reg = 0x13;
		bit = (devid & 7) + 2;
		break;
	case 48:		/* GPIO0LDO */
	case 49:		/* GPIO1LDO */
		/* TODO: implement */
	default:
		return -1;
	}

	return (bit & 0xff) | (reg << 8);
}

static unsigned int axp803_pstate_get(uint16_t device)
{
	uint32_t regmap = axp803_map_devices(device);
	uint8_t reg;
	int bit, val;

	if (regmap == -1)
		return -1;

	reg = (regmap >> 8) & 0xff;
	bit = regmap & 0xff;

	val = sunxi_rsb_read(reg);
	if (val < 0)
		return -2;

	if (val & BIT(bit))
		return 1;

	return 0;
}

unsigned int sunxi_pstate_get(uint16_t device)
{
	switch (pmic_type) {
	case PMIC_AXP803:
		return axp803_pstate_get(device);
	}

	return ~0;
}

static int axp803_pstate_set(uint16_t device, uint8_t state)
{
	uint32_t regmap = axp803_map_devices(device);
	uint8_t reg;
	int bit, val;

	if (regmap == -1)
		return -1;

	reg = (regmap >> 8) & 0xff;
	bit = regmap & 0xff;

	val = sunxi_rsb_read(reg);
	if (val < 0)
		return -2;

	if (state)
		val |= BIT(bit);
	else
		val &= ~BIT(bit);

	return sunxi_rsb_write(reg, val);
}

int sunxi_pstate_set(uint16_t device, uint8_t state)
{
	switch (pmic_type) {
	case PMIC_AXP803:
		return axp803_pstate_set(device, state);
	}

	return -EINVAL;
}

int sunxi_power_setup(uint16_t socid)
{
	int ret;

	switch (socid) {
	case 0x1689:
		pmic_type = PMIC_AXP803;

		NOTICE("PMIC: Probing for AXP803 on A64\n");
		ret = axp803_probe();
		if (ret) {
			ERROR("PMIC: AXP803 initialization failed: %d\n", ret);
			return ret;
		}
		ret = axp803_initial_setup();
		if (ret) {
			ERROR("PMIC: AXP803 power setup failed: %d\n", ret);
			return ret;
		}
		NOTICE("PMIC: AXP803 successfully setup\n");
		break;
	case 0x1718:
		ret = -ENXIO;
		break;
	default:
		NOTICE("power setup not defined for SoC 0x%04x\n", socid);
		ret = -ENODEV;
	}

	return ret;
}
