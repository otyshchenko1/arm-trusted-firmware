/*
 * Copyright (c) 2016,2017 ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2015-2017 Renesas Electronics Corporation. All rights reserved.
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
#include "rcar_def.h"
#include "rcar_private.h"

/* TODO These should be taken from avs_driver.c */
#define EFUSE_AVS0			(0U)
#define EFUSE_AVS_NUM		(7U)
static uint32_t efuse_avs = EFUSE_AVS0;

struct op_points
{
	unsigned long freq;	/* Hz */
	unsigned long volt;	/* uV */
};

#define NR_H3_OPP	5
#define NR_M3_OPP	6

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))

/* Describe OPPs exactly how they are described in the device-tree */
static const struct op_points rcar_h3_op_points[EFUSE_AVS_NUM][NR_H3_OPP] = {
	{
		{ 500000000,  820000, },
		{ 1000000000, 820000, },
		{ 1500000000, 820000, },
		{ 1600000000, 900000, },
		{ 1700000000, 960000, },
	},
	{
		{ 500000000,  820000, },
		{ 1000000000, 820000, },
		{ 1500000000, 820000, },
		{ 1600000000, 900000, },
		{ 1700000000, 960000, },
	},
	{
		{ 500000000,  820000, },
		{ 1000000000, 820000, },
		{ 1500000000, 820000, },
		{ 1600000000, 900000, },
		{ 1700000000, 960000, },
	},
	{
		{ 500000000,  790000, },
		{ 1000000000, 790000, },
		{ 1500000000, 790000, },
		{ 1600000000, 870000, },
		{ 1700000000, 910000, },
	},
	{
		{ 500000000,  790000, },
		{ 1000000000, 790000, },
		{ 1500000000, 790000, },
		{ 1600000000, 870000, },
		{ 1700000000, 890000, },
	},
	{
		{ 500000000,  770000, },
		{ 1000000000, 770000, },
		{ 1500000000, 770000, },
		{ 1600000000, 850000, },
		{ 1700000000, 870000, },
	},
	{
		{ 500000000,  750000, },
		{ 1000000000, 750000, },
		{ 1500000000, 750000, },
		{ 1600000000, 830000, },
		{ 1700000000, 860000, },
	},
};

static const struct op_points rcar_m3_op_points[EFUSE_AVS_NUM][NR_M3_OPP] = {
	{
		{ 500000000,  820000, },
		{ 1000000000, 820000, },
		{ 1500000000, 820000, },
		{ 1600000000, 900000, },
		{ 1700000000, 900000, },
		{ 1800000000, 960000, },
	},
	{
		{ 500000000,  820000, },
		{ 1000000000, 820000, },
		{ 1500000000, 820000, },
		{ 1600000000, 900000, },
		{ 1700000000, 900000, },
		{ 1800000000, 960000, },
	},
	{
		{ 500000000,  820000, },
		{ 1000000000, 820000, },
		{ 1500000000, 820000, },
		{ 1600000000, 900000, },
		{ 1700000000, 900000, },
		{ 1800000000, 960000, },
	},
	{
		{ 500000000,  790000, },
		{ 1000000000, 790000, },
		{ 1500000000, 790000, },
		{ 1600000000, 870000, },
		{ 1700000000, 870000, },
		{ 1800000000, 910000, },
	},
	{
		{ 500000000,  790000, },
		{ 1000000000, 790000, },
		{ 1500000000, 790000, },
		{ 1600000000, 870000, },
		{ 1700000000, 870000, },
		{ 1800000000, 890000, },
	},
	{
		{ 500000000,  770000, },
		{ 1000000000, 770000, },
		{ 1500000000, 770000, },
		{ 1600000000, 850000, },
		{ 1700000000, 850000, },
		{ 1800000000, 870000, },
	},
	{
		{ 500000000,  750000, },
		{ 1000000000, 750000, },
		{ 1500000000, 750000, },
		{ 1600000000, 830000, },
		{ 1700000000, 830000, },
		{ 1800000000, 860000, },
	},
};

static int current_opp_index;
static int current_opp_latency;
static int current_opp_limit = 0;
static const struct op_points *current_opp_table;

static int dvfs_inited = 0;

uint32_t rcar_dvfs_get_get_opp_voltage(int oppnr)
{
	if (oppnr < 0 || oppnr >= current_opp_limit)
		return ~0;

	/* Protocol requires voltage to be in mV */
	return current_opp_table[oppnr].volt / 1000;
}

uint32_t rcar_dvfs_get_get_opp_frequency(int oppnr)
{
	if (oppnr < 0 || oppnr >= current_opp_limit)
		return ~0;

	/* Protocol requires frequency to be in Hz */
	return current_opp_table[oppnr].freq;
}

int rcar_dvfs_set_index(int index)
{
	if (index < 0 || index >= current_opp_limit)
		return -1;

	if (index < current_opp_index) {
		//rcar_clock_set_cpu_clock(current_opp_table[oppnr].freq, 1);
		//rcar_power_set_cpu_voltage(current_opp_table[oppnr].volt);
	} else {
		//rcar_power_set_cpu_voltage(rcurrent_opp_table[oppnr].volt);
		//rcar_clock_set_cpu_clock(current_opp_table[oppnr].freq, 1);
	}

	current_opp_index = index;

	return 0;
}

int rcar_dvfs_get_index(void)
{
	return current_opp_index;
}

int rcar_dvfs_get_nr_opp(void)
{
	return current_opp_limit;
}

int rcar_dvfs_get_latency(void)
{
	return current_opp_latency;
}

int rcar_dvfs_opp_init(void)
{
	uint32_t product;

	if (dvfs_inited)
		return 0;

	product = mmio_read_32(RCAR_PRR) & RCAR_PRODUCT_MASK;

	if (product == RCAR_PRODUCT_H3) {
		current_opp_limit = ARRAY_SIZE(rcar_h3_op_points[efuse_avs]);
		current_opp_table = &rcar_h3_op_points[efuse_avs][0];
	} else if (product == RCAR_PRODUCT_M3) {
		current_opp_limit = ARRAY_SIZE(rcar_m3_op_points[efuse_avs]);
		current_opp_table = &rcar_m3_op_points[efuse_avs][0];
	} else
		return -1;

	/* Guess it is 1500000000 Hz for now */
	current_opp_index = 2;
	/* Latency is 300 uS for all OPPs */
	current_opp_latency = 300;

	dvfs_inited = 1;

	return 0;
}
