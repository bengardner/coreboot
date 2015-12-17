/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2013 Google Inc.
 * Copyright (C) 2013 Sage Electronic Engineering, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <stddef.h>
#include <arch/cpu.h>
#include <lib.h>
#include <arch/io.h>
#include <arch/cbfs.h>
#include <arch/stages.h>
#include <console/console.h>
#include <cbmem.h>
#include <cpu/x86/mtrr.h>
#include <romstage_handoff.h>
#include <timestamp.h>
#include <soc/gpio.h>
#include <soc/iomap.h>
#include <soc/lpc.h>
#include <soc/pci_devs.h>
#include <soc/romstage.h>
#include <soc/acpi.h>
#include <soc/baytrail.h>
#include <drivers/intel/fsp1_0/fsp_util.h>
#include "wabtec-cpu1900.h"


static const char *get_reset_cause_text(void)
{
    static const char *reset_cause_text[8] = {
        "[0] Cold Boot",
        "[1] Watchdog Reset",
        "[2] Backplane Sleep",
        "[3] Power Failure",
        "[4] Software Reset",
        "[5] Button",
        "[6] Timeout",
        "[7] Invalid"
    };
    return reset_cause_text[inb(CPU1900_REG_RESET_CAUSE) & CPU1900_REG_RESET_CAUSE__MASK];
}


/**
 * mainboard call for setup that needs to be done before fsp init
 */
void early_mainboard_romstage_entry(void)
{
	/* Set the LED to a 5 Hz red 50% blink */
	outb(CPU1900_LED_RED_BLINK, CPU1900_REG_STATUS_LED_DUTY);
	outb(CPU1900_LED_5_HZ, CPU1900_REG_STATUS_LED_RATE);

	printk(BIOS_NOTICE, "Reset: %s\n", get_reset_cause_text());
}


/**
 * Get function disables - most of these will be done automatically
 * @param fd_mask
 * @param fd2_mask
 */
void get_func_disables(uint32_t *fd_mask, uint32_t *fd2_mask)
{
}


/**
 * /brief mainboard call for setup that needs to be done after fsp init
 */
void late_mainboard_romstage_entry(void)
{
	printk(BIOS_INFO, "CPU1900: DTE LED red\n");
	outb(CPU1900_LED_RED, CPU1900_REG_DTE_LED_DUTY);
}


/**
 * /brief customize fsp parameters here if needed
 */
void romstage_fsp_rt_buffer_callback(FSP_INIT_RT_BUFFER *FspRtBuffer)
{
}
