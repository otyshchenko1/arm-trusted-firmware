#include <debug.h>
#include <plat_config.h>
#include <mmio.h>
#include <sys/errno.h>

#include "sunxi_def.h"
#include "sunxi_private.h"

struct op_points
{
	uint32_t freq;
	uint32_t voltage;
} sunxi_op_points[] = {
	{ 408, 1000}, { 648, 1040}, { 816, 1080}, { 912, 1120}, { 960, 1160},
	{1008, 1200}, {1056, 1240}, {1104, 1260}, {1152, 1300}
};

#define NR_OPP (sizeof(sunxi_op_points) / sizeof(sunxi_op_points[0]))

int current_opp_index = 2;
int current_opp_limit = NR_OPP;

uint32_t sunxi_dvfs_get_get_opp_voltage(int oppnr)
{
	if (oppnr < 0 || oppnr >= NR_OPP)
		return ~0;

	return sunxi_op_points[oppnr].voltage;
}

uint32_t sunxi_dvfs_get_get_opp_frequency(int oppnr)
{
	if (oppnr < 0 || oppnr >= NR_OPP)
		return ~0;

	return sunxi_op_points[oppnr].freq * 1000000;
}

int sunxi_dvfs_set_index(int index)
{
	if (index < 0 || index >= NR_OPP)
		return -1;

	if (index < current_opp_index) {
		sunxi_clock_set_cpu_clock(sunxi_op_points[index].freq, 1);
		sunxi_power_set_cpu_voltage(sunxi_op_points[index].voltage);
	} else {
		sunxi_power_set_cpu_voltage(sunxi_op_points[index].voltage);
		sunxi_clock_set_cpu_clock(sunxi_op_points[index].freq, 1);
	}

	current_opp_index = index;

	return 0;
}

int sunxi_dvfs_get_index(void)
{
	return current_opp_index;
}

int sunxi_dvfs_get_nr_opp(void)
{
	return NR_OPP;
}
