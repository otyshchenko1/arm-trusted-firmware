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

#include <assert.h>
#include <console.h>
#include <debug.h>
#include <mmio.h>
#include "rcar_sip_svc.h"
#include "rcar_private.h"
#include <runtime_svc.h>
#include <uuid.h>

/* SiP Service UUID */
DEFINE_SVC_UUID(rcar_sip_svc_uid,
		0xbd0bebfd, 0xe1cb, 0x4dfd, 0x9b, 0x63,
		0x8b, 0x0d, 0x48, 0x77, 0x4a, 0xd6);

#pragma weak rcar_plat_sip_handler
uint64_t rcar_plat_sip_handler(uint32_t smc_fid,
				uint64_t x1,
				uint64_t x2,
				uint64_t x3,
				uint64_t x4,
				void *cookie,
				void *handle,
				uint64_t flags)
{
	ERROR("%s: unhandled SMC (0x%x)\n", __func__, smc_fid);
	SMC_RET1(handle, SMC_UNK);
}

/* This function handles R-Car defined SiP Calls */
uint64_t rcar_sip_handler(uint32_t smc_fid,
			uint64_t x1,
			uint64_t x2,
			uint64_t x3,
			uint64_t x4,
			void *cookie,
			void *handle,
			uint64_t flags)
{
	/* Determine which security state this SMC originated from */
	if (!is_caller_non_secure(flags))
		SMC_RET1(handle, SMC_UNK);

	/* SiP SMC service normal world's call */
	switch (smc_fid) {
	case RCAR_SIP_MBOX_TRIGGER: {
		uint32_t ret = rcar_trigger_scpi(x1, x2, x3, x4);

		SMC_RET1(handle, ret);
	}
	}

	return rcar_plat_sip_handler(smc_fid, x1, x2, x3, x4,
					cookie, handle, flags);
}

/* This function is responsible for handling all SiP calls from the NS world */
uint64_t sip_smc_handler(uint32_t smc_fid,
			 uint64_t x1,
			 uint64_t x2,
			 uint64_t x3,
			 uint64_t x4,
			 void *cookie,
			 void *handle,
			 uint64_t flags)
{
	switch (smc_fid) {
	case SIP_SVC_CALL_COUNT:
		/* Return the number of R-Car SiP Service Calls. */
		SMC_RET1(handle, RCAR_COMMON_SIP_NUM_CALLS);

	case SIP_SVC_UID:
		/* Return UUID to the caller */
		SMC_UUID_RET(handle, rcar_sip_svc_uid);

	case SIP_SVC_VERSION:
		/* Return the version of current implementation */
		SMC_RET2(handle, RCAR_SIP_SVC_VERSION_MAJOR,
				RCAR_SIP_SVC_VERSION_MINOR);

	default:
		return rcar_sip_handler(smc_fid, x1, x2, x3, x4,
			cookie, handle, flags);
	}
}

/* Define a runtime service descriptor for fast SMC calls */
DECLARE_RT_SVC(
	rcar_sip_svc,
	OEN_SIP_START,
	OEN_SIP_END,
	SMC_TYPE_FAST,
	NULL,
	sip_smc_handler
);
