/*
 * Copyright (C) 2023-2024 Cornelis Networks.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _OPX_HFI1_SIM_H_
#define _OPX_HFI1_SIM_H_

/**********************************************************************
 * Macro and function support for the simulator as well as WFR and JKR.
 *
 * OPX_HFI1_BAR_STORE and OPX_HFI1_BAR_LOAD should be used for PCIe
 * device memory STORE and LOAD (scb, pio, pio_sop, and the uregs)
 *
 * Do not use these macros on regular memory maps. They will fail
 * on the simulator.
 *********************************************************************/

#ifdef OPX_SIM_ENABLED
/* Build simulator support */

__OPX_FORCE_INLINE__
void opx_sim_store(uint64_t offset, uint64_t *value, const char *func, const int line)
{
	long ret, loffset = (long) offset;
	ret = lseek(fi_opx_global.hfi_local_info.sim_fd, offset, SEEK_SET);
	if (ret != loffset) {
		FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "%s:%u FI_OPX_HFI1_BAR_STORE: offset %#16.16lX\n",
			     func, line, offset);
		perror("FI_OPX_HFI1_BAR_STORE: Unable to lseek BAR: ");
		sleep(5);
		abort();
	}
	FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "%s:%u FI_OPX_HFI1_BAR_STORE: %#16.16lX value [%#16.16lX]\n",
		     func, line, offset, *value);
	if (write(fi_opx_global.hfi_local_info.sim_fd, value, sizeof(*value)) < 0) {
		perror("FI_OPX_HFI1_BAR_STORE: Unable to write BAR: ");
		sleep(5);
		abort();
	}
	return;
}

__OPX_FORCE_INLINE__
uint64_t opx_sim_load(uint64_t offset)
{
	uint64_t value;
	long	 ret, loffset = (long) offset;
	FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "FI_OPX_HFI1_BAR_LOAD: offset %#16.16lX\n", loffset);
	ret = lseek(fi_opx_global.hfi_local_info.sim_fd, offset, SEEK_SET);
	if (ret != loffset) {
		perror("FI_OPX_HFI1_BAR_LOAD: Unable to lseek BAR: ");
		sleep(5);
		abort();
	}
	if (read(fi_opx_global.hfi_local_info.sim_fd, &value, sizeof(value)) < 0) {
		perror("FI_OPX_HFI1_BAR_LOAD: Unable to read BAR: ");
		sleep(5);
		abort();
	}
	FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "FI_OPX_HFI1_BAR_LOAD: value %#16.16lX\n", value);
	return value;
}

#define OPX_OPEN_BAR(unit) opx_open_sim_bar(unit)

__OPX_FORCE_INLINE__
void opx_open_sim_bar(unsigned unit)
{
	static const char *sim_barfiles[] = {
		/* Typical sim bar files */
		"/sys/devices/pcif00f:00/f00f:00:00.0/resource0", /* hfi_0 */
		"/sys/devices/pcif00f:00/f00f:00:01.0/resource0", /* hfi_1 */
		"/sys/devices/f00f:01:00.0/resource0",		  /* hfi_0 updated simpci */
		"/sys/devices/f00f:02:00.0/resource0"		  /* hfi_1 updated simpci */
	};

	const char *filename = NULL;
	static char filename_storage[256];

	if (getenv("HFI_FNAME")) {
		/* Arbitrary user specified file name*/
		filename = getenv("HFI_FNAME");
	} else if (getenv("FI_OPX_SIMPCI_V")) {
		/* Old "standard" file names */
		assert(unit < 2); /* simulation limit for this option */
		int v = atoi(getenv("FI_OPX_SIMPCI_V"));
		assert((v == 0) || (v == 1));
		if (v) {
			unit += 2;
		}
		filename = sim_barfiles[unit];
	} else {
		/* Calculate new expected path/filename */
		snprintf(filename_storage, sizeof(filename_storage), "/sys/class/infiniband/hfi1_%d/device/resource0",
			 unit);
		filename = filename_storage;
	}

	fi_opx_global.hfi_local_info.sim_fd = open(filename, O_RDWR);
	if (fi_opx_global.hfi_local_info.sim_fd < 0) {
		FI_WARN(fi_opx_global.prov, FI_LOG_EP_DATA, "HFI_FNAME %s: filename %s\n", getenv("HFI_FNAME"),
			filename);
		perror("fi_opx_sim_open_bar Unable to open BAR\n");
		sleep(5);
		abort();
	}
}

#define OPX_HFI1_BAR_STORE(bar, value)                                      \
	do {                                                                \
		uint64_t _value = value;                                    \
		opx_sim_store((uint64_t) bar, &_value, __func__, __LINE__); \
	} while (false)

#define OPX_HFI1_BAR_LOAD(bar) opx_sim_load((uint64_t) bar)

#define OPX_TXE_PIO_SEND ((uint64_t) 0x2000000)

#define OPX_JKR_RXE_PER_CONTEXT_OFFSET ((uint64_t) 0x1600000)
#define OPX_WFR_RXE_PER_CONTEXT_OFFSET ((uint64_t) 0x1300000)

#define OPX_JKR_RXE_UCTX_STRIDE ((uint64_t) 8 * 1024)
#define OPX_WFR_RXE_UCTX_STRIDE ((uint64_t) 4 * 1024)

#define OPX_HFI1_INIT_PIO_SOP(context, input)                                                           \
	({                                                                                              \
		volatile uint64_t *__pio_sop;                                                           \
		do {                                                                                    \
			if (OPX_HFI1_TYPE & OPX_HFI1_WFR) {                                             \
				__pio_sop = (uint64_t *) (OPX_TXE_PIO_SEND + (context * (64 * 1024L)) + \
							  (16 * 1024 * 1024L));                         \
			} else {                                                                        \
				__pio_sop = (uint64_t *) (OPX_TXE_PIO_SEND + (context * (64 * 1024L)) + \
							  (16 * 1024 * 1024L));                         \
			}                                                                               \
		} while (false);                                                                        \
		__pio_sop;                                                                              \
	})

#define OPX_HFI1_INIT_PIO(context, input)                                                           \
	({                                                                                          \
		volatile uint64_t *__pio;                                                           \
		do {                                                                                \
			if (OPX_HFI1_TYPE & OPX_HFI1_WFR) {                                         \
				__pio = (uint64_t *) (OPX_TXE_PIO_SEND + (context * (64 * 1024L))); \
			} else {                                                                    \
				__pio = (uint64_t *) (OPX_TXE_PIO_SEND + (context * (64 * 1024L))); \
			}                                                                           \
		} while (false);                                                                    \
		__pio;                                                                              \
	})

#define OPX_HFI1_INIT_UREGS(context, input)                                                     \
	({                                                                                      \
		volatile uint64_t *__uregs;                                                     \
		do {                                                                            \
			if (OPX_HFI1_TYPE & OPX_HFI1_WFR) {                                     \
				__uregs = (uint64_t *) (OPX_WFR_RXE_PER_CONTEXT_OFFSET +        \
							((context) * OPX_WFR_RXE_UCTX_STRIDE)); \
			} else {                                                                \
				__uregs = (uint64_t *) (OPX_JKR_RXE_PER_CONTEXT_OFFSET +        \
							((context) * OPX_JKR_RXE_UCTX_STRIDE)); \
			}                                                                       \
		} while (false);                                                                \
		__uregs;                                                                        \
	})

#else
/* Build only "real" HFI1 support (default) */

#define OPX_OPEN_BAR(unit) fi_opx_global.hfi_local_info.sim_fd = -1

#if !defined(NDEBUG) && defined(OPX_DEBUG_VERBOSE)
#define OPX_HFI1_BAR_LOAD(bar)                                                                                       \
	({                                                                                                           \
		volatile uint64_t _value = *(volatile uint64_t *) bar;                                               \
		FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "%s:%u FI_OPX_HFI1_BAR_LOAD: offset %#16.16lX\n",   \
			     __func__, __LINE__, (uint64_t) (bar));                                                  \
		FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "FI_OPX_HFI1_BAR_LOAD: value %#16.16lX\n", _value); \
		_value;                                                                                              \
	})

#define OPX_HFI1_BAR_STORE(bar, value)                                                                              \
	do {                                                                                                        \
		FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "%s:%u FI_OPX_HFI1_BAR_STORE: offset %#16.16lX\n", \
			     __func__, __LINE__, (uint64_t) (bar));                                                 \
		FI_DBG_TRACE(fi_opx_global.prov, FI_LOG_EP_DATA, "FI_OPX_HFI1_BAR_STORE: value %#16.16lX\n",        \
			     (uint64_t) value);                                                                     \
		*(volatile uint64_t *) bar = (uint64_t) value;                                                      \
	} while (false)

#else

#define OPX_HFI1_BAR_STORE(bar, value) *(volatile uint64_t *) bar = (uint64_t) value;

#define OPX_HFI1_BAR_LOAD(bar) *(volatile uint64_t *) bar

#endif

#define OPX_TXE_PIO_SEND ((uint64_t) 0x2000000)

#define OPX_JKR_RXE_PER_CONTEXT_OFFSET ((uint64_t) 0x1600000)
#define OPX_WFR_RXE_PER_CONTEXT_OFFSET ((uint64_t) 0x1300000)

#define OPX_JKR_RXE_UCTX_STRIDE ((uint64_t) 8 * 1024)
#define OPX_WFR_RXE_UCTX_STRIDE ((uint64_t) 4 * 1024)

#define OPX_HFI1_INIT_PIO_SOP(context, input) input

#define OPX_HFI1_INIT_PIO(context, input) input

#define OPX_HFI1_INIT_UREGS(context, input) input

#endif

#endif
