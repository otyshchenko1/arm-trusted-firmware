/*
 * Copyright (c) 2016,2017 ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2017 EPAM Systems Inc. All rights reserved.
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
#include <mmio.h>
#include <sys/errno.h>
#include "rcar_def.h"
#include "rcar_private.h"

#define BIT(n) (1U << (n))
#define GENMASK(hi, lo) (BIT(hi) - 1 - BIT(lo) + 1)

#define SCPI_OK		0
#define SCPI_E_PARAM	1
#define SCPI_E_ALIGN	2
#define SCPI_E_SIZE	3
#define SCPI_E_HANDLER	4
#define SCPI_E_ACCESS	5
#define SCPI_E_RANGE	6
#define SCPI_E_TIMEOUT	7
#define SCPI_E_NOMEM	8
#define SCPI_E_PWRSTATE	9
#define SCPI_E_SUPPORT	10
#define SCPI_E_DEVICE	11
#define SCPI_E_BUSY	12

#define SCP_CMD_CAPABILITY	0x02

#define SCP_CMDS_IMPLEMENTED						\
	0

#define RCAR_SCPI_SHMEM_BASE	DRAM1_NS_SCPI_BASE

static uint32_t scpi_handle_cmd(int cmd, uint8_t *payload_size,
				uintptr_t payload_in, uintptr_t payload_out)
{
	switch (cmd) {
	case SCP_CMD_CAPABILITY:
		mmio_write_32(payload_out + 0x00, (1U << 16) | (2U << 0));
		/*
		 * The SCPI spec says this field holds the payload sizes for
		 * the receive and transmit channel, but the Linux driver
		 * decodes an event version ID from it.
		 * Let's play nice with Linux for now and ignore the spec.
		 *
		 * mmio_write_32(payload_out + 0x04,
		 *		 ((256 - 1) << 16) | (256 - 1));
		 */
		mmio_write_32(payload_out + 0x04, 1U << 16);

		mmio_write_32(payload_out + 0x08, 1U << 24);
		mmio_write_32(payload_out + 0x0c, SCP_CMDS_IMPLEMENTED);
		mmio_write_32(payload_out + 0x10, 0x0);
		mmio_write_32(payload_out + 0x14, 0x0);
		mmio_write_32(payload_out + 0x18, 0x0);
		*payload_size = 0x1c;
		return SCPI_OK;
	}

	return SCPI_E_SUPPORT;
}

uint32_t rcar_trigger_scpi(uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4)
{
	uint32_t ret;
	uint64_t scpi_header;
	uint8_t payload_size;

	scpi_header = *(uint64_t *)(RCAR_SCPI_SHMEM_BASE + 0x100);
	payload_size = (scpi_header >> 16) & 0xff;

	ret = scpi_handle_cmd(scpi_header & 0xff, &payload_size,
			RCAR_SCPI_SHMEM_BASE + 0x108,
			RCAR_SCPI_SHMEM_BASE + 0x8);

	mmio_write_32(RCAR_SCPI_SHMEM_BASE, (scpi_header & 0xffff) |
			(uint32_t)payload_size << 16);
	mmio_write_32(RCAR_SCPI_SHMEM_BASE + 4, ret);

	return ret;
}
