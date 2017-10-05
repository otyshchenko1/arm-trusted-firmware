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

#include "rcar_def.h"
#include "rcar_private.h"

struct op_points
{
	uint32_t freq;
	uint32_t voltage;
} rcar_op_points[] = {
	{500,  820}, /* Hz, mV */
	{1000, 820},
	{1500, 820},
	{1600, 900},
	{1700, 960}
};

#define NR_OPP (sizeof(rcar_op_points) / sizeof(rcar_op_points[0]))

int current_opp_index = 2;
int current_opp_limit = NR_OPP;
int current_opp_latency = 300; /* uS */

uint32_t rcar_dvfs_get_get_opp_voltage(int oppnr)
{
	if (oppnr < 0 || oppnr >= NR_OPP)
		return ~0;

	return rcar_op_points[oppnr].voltage;
}

uint32_t rcar_dvfs_get_get_opp_frequency(int oppnr)
{
	if (oppnr < 0 || oppnr >= NR_OPP)
		return ~0;

	return rcar_op_points[oppnr].freq * 1000000;
}

int rcar_dvfs_set_index(int index)
{
	if (index < 0 || index >= NR_OPP)
		return -1;

	if (index < current_opp_index) {
		//rcar_clock_set_cpu_clock(rcar_op_points[index].freq, 1);
		//rcar_power_set_cpu_voltage(rcar_op_points[index].voltage);
	} else {
		//rcar_power_set_cpu_voltage(rcar_op_points[index].voltage);
		//rcar_clock_set_cpu_clock(rcar_op_points[index].freq, 1);
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
	return NR_OPP;
}

int rcar_dvfs_get_latency(void)
{
	return current_opp_latency;
}
