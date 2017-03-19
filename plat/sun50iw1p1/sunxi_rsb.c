/*
 * Copyright (c) 2017 ARM Limited and Contributors. All rights reserved.
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

#define R_PRCM_BASE	0x1f01400ULL
#define R_TWI_BASE	0x1f02400ULL
#define R_PIO_BASE	0x1f02c00ULL

#define RSB_BASE	0x1f03400ULL
#define RSB_CTRL	0x00
#define RSB_CCR		0x04
#define RSB_INTE	0x08
#define RSB_STAT	0x0c
#define RSB_DADDR0	0x10
#define RSB_DLEN	0x18
#define RSB_DATA0	0x1c
#define RSB_LCR		0x24
#define RSB_PMCR	0x28
#define RSB_CMD		0x2c
#define RSB_SADDR	0x30

#define RSBCMD_SRTA	0xE8
#define RSBCMD_RD8	0x8B
#define RSBCMD_RD16	0x9C
#define RSBCMD_RD32	0xA6
#define RSBCMD_WR8	0x4E
#define RSBCMD_WR16	0x59
#define RSBCMD_WR32	0x63

#define BIT(n) (1U << (n))

/* Initialize the RSB controller and its pins. */
int sunxi_rsb_init(void)
{
	uint32_t reg;

	/* un-gate PIO clock */
	reg = mmio_read_32(R_PRCM_BASE + 0x28);
	mmio_write_32(R_PRCM_BASE + 0x28, reg | 0x01);

	/* get currently configured function for pins PL0 and PL1 */
	reg = mmio_read_32(R_PIO_BASE + 0x00);
	if ((reg & 0xff) == 0x33) {
		NOTICE("already configured for TWI\n");
		return -EBUSY;
	}

	if ((reg & 0xff) == 0x22) {
		NOTICE("PMIC: already configured for RSB\n");
		return -EEXIST;	/* configured for RSB mode already */
	}

	/* switch pins PL0 and PL1 to RSB */
	mmio_write_32(R_PIO_BASE + 0, (reg & ~0xff) | 0x22);

	/* level 2 drive strength */
	reg = mmio_read_32(R_PIO_BASE + 0x14);
	mmio_write_32(R_PIO_BASE + 0x14, (reg & ~0x0f) | 0xa);

	/* set both ports to pull-up */
	reg = mmio_read_32(R_PIO_BASE + 0x1c);
	mmio_write_32(R_PIO_BASE + 0x1c, (reg & ~0x0f) | 0x5);

	/* assert & de-assert reset of RSB */
	reg = mmio_read_32(R_PRCM_BASE + 0xb0);
	mmio_write_32(R_PRCM_BASE + 0xb0, reg & ~0x08);
	reg = mmio_read_32(R_PRCM_BASE + 0xb0);
	mmio_write_32(R_PRCM_BASE + 0xb0, reg | 0x08);

	/* un-gate RSB clock */
	reg = mmio_read_32(R_PRCM_BASE + 0x28);
	mmio_write_32(R_PRCM_BASE + 0x28, reg | 0x08);

	mmio_write_32(RSB_BASE + RSB_CTRL, 0x01);	/* soft reset */

	mmio_write_32(RSB_BASE + RSB_CCR, 0x11d);	/* clock to 400 KHz */

	do {
		reg = mmio_read_32(RSB_BASE + RSB_CTRL);
	} while (reg & 1);			/* transaction in progress */

	return 0;
}

int sunxi_rsb_read(uint8_t address)
{
	uint32_t reg;

	mmio_write_32(RSB_BASE + RSB_DLEN, 0x10); /* read a byte, snake oil? */
	mmio_write_32(RSB_BASE + RSB_CMD, RSBCMD_RD8);	/* read a byte */
	mmio_write_32(RSB_BASE + RSB_DADDR0, address);
	mmio_write_32(RSB_BASE + RSB_CTRL, 0x80);	/* start transaction */
	do {
		reg = mmio_read_32(RSB_BASE + RSB_CTRL);
	} while (reg & 0x80);			/* transaction in progress */

	reg = mmio_read_32(RSB_BASE + RSB_STAT);
	if (reg == 0x01) {			/* transaction complete */
		reg = mmio_read_32(RSB_BASE + RSB_DATA0); /* result register */
		return reg & 0xff;
	}

	return -reg;
}

int sunxi_rsb_write(uint8_t address, uint8_t value)
{
	uint32_t reg;

	mmio_write_32(RSB_BASE + RSB_DLEN, 0x00); /* write a byte, snake oil? */
	mmio_write_32(RSB_BASE + RSB_CMD, RSBCMD_WR8);	/* write a byte */
	mmio_write_32(RSB_BASE + RSB_DADDR0, address);
	mmio_write_32(RSB_BASE + RSB_DATA0, value);
	mmio_write_32(RSB_BASE + RSB_CTRL, 0x80);	/* start transaction */
	do {
		reg = mmio_read_32(RSB_BASE + RSB_CTRL);
	} while (reg & 0x80);			/* transaction in progress */

	reg = mmio_read_32(RSB_BASE + RSB_STAT);
	if (reg == 0x01)			/* transaction complete */
		return 0;

	return -reg;
}

void sunxi_rsb_wait(const char *desc)
{
	uint32_t reg;
	int cnt = 0;

	do {
		reg = mmio_read_32(RSB_BASE + RSB_CTRL);
		cnt++;
	} while (reg & 0x80);			/* transaction in progress */

	reg = mmio_read_32(RSB_BASE + RSB_STAT);
	if (reg == 0x01)
		return;

	ERROR("%s: 0x%x\n", desc, reg);
}

/* Initialize the RSB PMIC connection. */
int sunxi_rsb_configure(uint16_t hw_addr, uint8_t rt_addr)
{
	int ret;

	mmio_write_32(RSB_BASE + RSB_PMCR,
		      0x00 | (0x3e << 8) | (0x7c << 16) | BIT(31));

	do {
		ret = mmio_read_32(RSB_BASE + RSB_PMCR);
	} while (ret & (1U << 31));             /* transaction in progress */

	mmio_write_32(RSB_BASE + RSB_CCR, 0x103);       /* 3 MHz */
	mmio_write_32(RSB_BASE + RSB_SADDR, hw_addr | (rt_addr << 16));
	mmio_write_32(RSB_BASE + RSB_CMD, RSBCMD_SRTA);
	mmio_write_32(RSB_BASE + RSB_CTRL, 0x80);
	sunxi_rsb_wait("set run-time address");

	/* Set slave runtime address */
	mmio_write_32(RSB_BASE + RSB_SADDR, rt_addr << 16);

	return 0;
}
