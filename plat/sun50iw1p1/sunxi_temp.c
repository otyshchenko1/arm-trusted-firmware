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
#include <assert.h>
#include <bl_common.h>
#include <context.h>
#include <context_mgmt.h>
#include <interrupt_mgmt.h>
#include <runtime_svc.h>
#include <sys/errno.h>

#include "sunxi_def.h"
#include "sunxi_private.h"

#define CCU_BASE	0x1c20000ULL
#define BUS_CLK_GATING_REG2	0x068
#define THS_CLK_REG		0x074
#define BUS_SOFT_RST_REG3	0x2d0
#define THS_BASE	0x1c25000ULL

#define BIT(n) (1U << (n))

/* temperature = ( MINUPA - reg * MULPA) / DIVPA */
#define MULPA	25000
#define DIVPA	214
#define MINUPA	2170
static int sun50_th_reg_to_temp(uint32_t reg_data)
{
	return ((MINUPA - (int)reg_data) * MULPA) / DIVPA;
}

/* Initialize the temperature sensor */
static int init_ths(void)
{
	uint32_t reg;

	/* de-assert reset of THS */
	reg = mmio_read_32(CCU_BASE + BUS_SOFT_RST_REG3);
	mmio_write_32(CCU_BASE + BUS_SOFT_RST_REG3, reg | BIT(8));

	/* enable THS clock at 4 MHz */
	reg = mmio_read_32(CCU_BASE + THS_CLK_REG) & ~0x3;
	mmio_write_32(CCU_BASE + THS_CLK_REG, reg | 0x3 | BIT(31));

	/* un-gate THS clock */
	reg = mmio_read_32(CCU_BASE + BUS_CLK_GATING_REG2);
	mmio_write_32(CCU_BASE + BUS_CLK_GATING_REG2, reg | BIT(8));

	/* start calibration */
	mmio_write_32(THS_BASE + 0x04, BIT(17));
	/* set aquire times */
	mmio_write_32(THS_BASE + 0x00, 0x190);
	mmio_write_32(THS_BASE + 0x40, 0x190 << 16);
	/* enable filter, average over 8 values */
	mmio_write_32(THS_BASE + 0x70, 0x06);
	/* enable sensors 0-2 (CPU & GPUs) measurement */
	reg = mmio_read_32(THS_BASE + 0x40);
	mmio_write_32(THS_BASE + 0x40, reg | BIT(0) | BIT(1) | BIT(2));

	return 0;
}

/* Setup the temperature sensor */
int sunxi_ths_setup(void)
{
	int ret;

	NOTICE("Configuring thermal sensors\n");

	ret = init_ths();
	if (ret) {
		ERROR("THS: cannot initialize temperature sensor\n");
		return -1;
	}

	return ret;
}

int sunxi_ths_read_temp(int sensor)
{
	int reg;

	if (sensor < 0 || sensor > 2)
		return ~0;

	reg = mmio_read_32(THS_BASE + 0x80 + (4 * sensor));

	return sun50_th_reg_to_temp(reg & 0xfff);
}
