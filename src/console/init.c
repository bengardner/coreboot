/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2003 Eric Biederman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of
 * the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <console/console.h>
#include <console/uart.h>
#include <console/streams.h>
#include <device/pci.h>
#include <option.h>
#include <rules.h>
#include <version.h>
#include "mainboard/wabtec/cpu1903/wabtec-cpu1900-io.h"
#include <option_table.h>

/* While in romstage, console loglevel is built-time constant. */
static ROMSTAGE_CONST int console_loglevel = CONFIG_DEFAULT_CONSOLE_LOGLEVEL;

int console_log_level(int msg_level)
{
#if defined(__PRE_RAM__)
	/* CPU-1900 HACK: console_loglevel store in FPGA scratch register during ROM stage */
	int cll = fpga_read_u8(CPU1900_REG_CB_LOGLVL);
	return (cll >= msg_level);
#else
	return (console_loglevel >= msg_level);
#endif
}

void console_init(void)
{
	int cll;

	cll = fpga_read_u8(CPU1900_REG_CB_LOGLVL);
#if defined(__PRE_RAM__)
	{
		int dcl = read_option(debug_level, CONFIG_DEFAULT_CONSOLE_LOGLEVEL);
		if ((cll < dcl) || (cll > BIOS_NEVER) ||
		    (fpga_read_u8(CPU1900_REG_RESET_CAUSE) == CPU1900_REG_RESET_CAUSE__M__COLD)) {
			/* copy the debug_level from CMOS to the CPU1900 scratch register */
			cll = dcl;
			fpga_write_u8(CPU1900_REG_CB_LOGLVL, cll);
		}
	}
#else
	console_loglevel = cll;
#endif

#if CONFIG_EARLY_PCI_BRIDGE && !defined(__SMM__)
	pci_early_bridge_init();
#endif

	console_hw_init();

	printk(BIOS_NOTICE, "\n\ncoreboot-%s%s %s " ENV_STRING " starting... [%d]\n",
	       coreboot_version, coreboot_extra_version, coreboot_build, cll);
}
