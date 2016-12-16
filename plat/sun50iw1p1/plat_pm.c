/*
 * Copyright (c) 2013, ARM Limited and Contributors. All rights reserved.
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

#include <assert.h>
#include <arch_helpers.h>
#include <arm_gic.h>
#include <debug.h>
#include <errno.h>
#include <platform.h>
#include <platform_def.h>
#include <psci.h>
#include <mmio.h>
#include <bakery_lock.h>
#include "sunxi_def.h"
#include "sunxi_private.h"
#include "sunxi_cpu_ops.h"
#include <cci400.h>
#include <console.h>
#include <psci.h>

bakery_lock_t plat_console_lock __attribute__ ((section("tzfw_coherent_mem")));

/*******************************************************************************
 * Private Sunxi function which is used to determine if any platform actions
 * should be performed for the specified affinity instance given its
 * state. Nothing needs to be done if the 'state' is not off or if this is not
 * the highest affinity level which will enter the 'state'.
 ******************************************************************************/
static int32_t sunxi_do_plat_actions(uint32_t afflvl, uint32_t state)
{
	uint32_t max_phys_off_afflvl;

	assert(afflvl <= MPIDR_MAX_AFFLVL);

	if (state != PSCI_STATE_OFF)
		return -EAGAIN;

	/*
	 * Find the highest affinity level which will be suspended and postpone
	 * all the platform specific actions until that level is hit.
	 */
	max_phys_off_afflvl = psci_get_max_phys_off_afflvl();
	assert(max_phys_off_afflvl != PSCI_INVALID_DATA);
	assert(psci_get_suspend_afflvl() >= max_phys_off_afflvl);
	if (afflvl != max_phys_off_afflvl)
		return -EAGAIN;

	return 0;
}

/*******************************************************************************
 * Sunxi handler called when an affinity instance is about to be turned on. The
 * level and mpidr determine the affinity instance.
 ******************************************************************************/
int32_t sunxi_affinst_on(uint64_t mpidr,
			uint64_t sec_entrypoint,
			uint64_t ns_entrypoint,
			uint32_t afflvl,
			uint32_t state)
{
	int cpu_nr = (mpidr >> MPIDR_AFF0_SHIFT) & MPIDR_AFFLVL_MASK;
	int cluster_nr = (mpidr >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK;

	if (afflvl != MPIDR_AFFLVL0)
		return PSCI_E_SUCCESS;

	sun50i_set_secondary_entry(sec_entrypoint, cpu_nr);

	sun50i_cpu_power_up(cluster_nr, cpu_nr);

	return PSCI_E_SUCCESS;
}

/*******************************************************************************
 * Sunxi handler called when an affinity instance has just been powered on after
 * being turned off earlier. The level and mpidr determine the affinity
 * instance. The 'state' arg. allows the platform to decide whether the cluster
 * was turned off prior to wakeup and do what's necessary to setup it up
 * correctly.
 ******************************************************************************/
int32_t sunxi_affinst_on_finish(uint64_t mpidr, uint32_t afflvl, uint32_t state)
{
	/* Determine if any platform actions need to be executed. */
	if (sunxi_do_plat_actions(afflvl, state) == -EAGAIN)
		return PSCI_E_SUCCESS;

	// set smp bit before cache enable
	platform_smp_init();

	/* Enable the gic cpu interface */
	arm_gic_cpuif_setup();
	arm_gic_pcpu_distif_setup();

	return PSCI_E_SUCCESS;
}

/*******************************************************************************
 * sunxi handler called when an affinity instance is about to enter standby.
 ******************************************************************************/
int sunxi_affinst_standby(unsigned int power_state)
{
	unsigned int target_afflvl;
	uint64_t scr = 0;

	/* Sanity check the requested state */
	target_afflvl = psci_get_pstate_afflvl(power_state);

    	/*
	 * It's possible to enter standby only on affinity level 0 i.e. a core.
	 * Ignore any other affinity level.
	 */
	if (target_afflvl != MPIDR_AFFLVL0)
		return PSCI_E_INVALID_PARAMS;

	scr = read_scr_el3();
	/* enable physical IRQ bit for NS world to wakeup the CPU */
	write_scr_el3(scr | SCR_IRQ_BIT);
	isb();

    	/*
	 * Enter standby state
	 * dsb is good practice before using wfi to enter low power states
	 */
	dsb();
	wfi();

    	/*
	 * Restore SCR to the original value, sync of scr_el3 is done
	 * by eret while el3_exit to save some execution cycles.
	 */
	write_scr_el3(scr);

	return PSCI_E_SUCCESS;
}

/*******************************************************************************
 * Common function called while turning a cpu off or suspending it. It is called
 * from sunxi_off() or sunxi_suspend() when these functions in turn are called for
 * the highest affinity level which will be powered down. It performs the
 * actions common to the OFF and SUSPEND calls.
 ******************************************************************************/
static int32_t sunxi_power_down_common(uint32_t afflvl, uint64_t mpidr,
				       uint64_t sec_entrypoint)
{
	int cpu_nr = (mpidr >> MPIDR_AFF0_SHIFT) & MPIDR_AFFLVL_MASK;
	int cluster_nr = (mpidr >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK;

	/* Prevent interrupts from spuriously waking up this cpu */
	arm_gic_cpuif_deactivate();

	sun50i_cpu_power_down(cluster_nr, cpu_nr);

	return PSCI_E_SUCCESS;
}

/*******************************************************************************
 * Handler called when an affinity instance is about to be turned off. The
 * level and mpidr determine the affinity instance. The 'state' arg. allows the
 * platform to decide whether the cluster is being turned off and take
 * appropriate actions.
 *
 * CAUTION: There is no guarantee that caches will remain turned on across calls
 * to this function as each affinity level is dealt with. So do not write & read
 * global variables across calls. It will be wise to do flush a write to the
 * global to prevent unpredictable results.
 ******************************************************************************/
static int32_t sunxi_affinst_off(uint64_t mpidr, uint32_t afflvl, uint32_t state)
{
	/* Determine if any platform actions need to be executed */
	if (sunxi_do_plat_actions(afflvl, state) == -EAGAIN)
		return PSCI_E_SUCCESS;

	return sunxi_power_down_common(afflvl, mpidr, 0);
}

/*******************************************************************************
 * Handler called when an affinity instance is about to be suspended. The
 * level and mpidr determine the affinity instance. The 'state' arg. allows the
 * platform to decide whether the cluster is being turned off and take apt
 * actions. The 'sec_entrypoint' determines the address in BL3-1 from where
 * execution should resume.
 *
 * CAUTION: There is no guarantee that caches will remain turned on across calls
 * to this function as each affinity level is dealt with. So do not write & read
 * global variables across calls. It will be wise to do flush a write to the
 * global to prevent unpredictable results.
 ******************************************************************************/
static int32_t sunxi_affinst_suspend(uint64_t mpidr,
				    uint64_t sec_entrypoint,
				    uint64_t ns_entrypoint,
				    uint32_t afflvl,
				    uint32_t state)
{
	/* Determine if any platform actions need to be executed */
	if (sunxi_do_plat_actions(afflvl, state) == -EAGAIN)
		return PSCI_E_SUCCESS;

	if (afflvl == psci_get_suspend_afflvl())
		console_exit();

	return sunxi_power_down_common(afflvl, mpidr, sec_entrypoint);
}

/*******************************************************************************
 * Sunxi handler called when an affinity instance has just been powered on after
 * having been suspended earlier. The level and mpidr determine the affinity
 * instance.
 * TODO: At the moment we reuse the on finisher and reinitialize the secure
 * context. Need to implement a separate suspend finisher.
 ******************************************************************************/
static int32_t sunxi_affinst_suspend_finish(uint64_t mpidr,
					   uint32_t afflvl,
					   uint32_t state)
{
	if ((afflvl == psci_get_suspend_afflvl()) && ((mpidr & 0xff) == 0x0)) {
		arm_gic_init(GICC_BASE, GICD_BASE, 0, NULL, 0);
		arm_gic_setup();
		console_init(SUNXI_UART0_BASE, UART0_CLK_IN_HZ, UART0_BAUDRATE);

	}

	return sunxi_affinst_on_finish(mpidr, afflvl, state);
}

/*******************************************************************************
 * Sunxi handlers to shutdown/reboot the system
 ******************************************************************************/
static void __dead2 sunxi_system_off(void)
{
	int ret;

	ret = sunxi_power_set_cpu_voltage(-1);

	ERROR("PSCI system shutdown: %d: still alive ...\n", ret);

	wfi();
	panic();
}

static void __dead2 sunxi_system_reset(void)
{
	mmio_write_32(0x01c20cb4, 1);
	mmio_write_32(0x01c20cb8, 1);
	mmio_write_32(0x01c20cb0, (0xa57 << 1) | 0x01);

	wfi();
	ERROR("Sunxi System Reset: operation not handled.\n");
	panic();
}

/*******************************************************************************
 * Export the platform handlers to enable psci to invoke them
 ******************************************************************************/
static const plat_pm_ops_t sunxi_ops = {
	.affinst_standby = sunxi_affinst_standby,
	.affinst_on		= sunxi_affinst_on,
	.affinst_on_finish	= sunxi_affinst_on_finish,
	.affinst_off		= sunxi_affinst_off,
	.affinst_suspend	= sunxi_affinst_suspend,
	.affinst_suspend_finish	= sunxi_affinst_suspend_finish,
	.system_off		= sunxi_system_off,
	.system_reset		= sunxi_system_reset
};

/*******************************************************************************
 * Export the platform specific power ops.
 ******************************************************************************/
int32_t platform_setup_pm(const plat_pm_ops_t **plat_ops)
{
	*plat_ops = &sunxi_ops;
	bakery_lock_init(&plat_console_lock);
	return 0;
}
