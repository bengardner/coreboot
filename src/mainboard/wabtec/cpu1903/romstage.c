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
#include <delay.h>
#include <soc/reset.h>
#include <cbfs.h>
#include <fmap.h>
#include <timestamp.h>
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
#include <commonlib/cbfs.h>
#include "wabtec-cpu1900-io.h"


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
	return reset_cause_text[fpga_read_u8(CPU1900_REG_RESET_CAUSE) & CPU1900_REG_RESET_CAUSE__M];
}

/* compute the hash over the last 256 bytes */
#define HASH_REGION_NAME   "RO_BOOTBLOCK_SHA1"
#define CB_HASH_SIZE       256

/**
 * Calculate a hash over the bootblock.
 * The idea is to detect incomplete firmware updates.  The bootblock contains
 * the build ID like "CPU1900-20160317-6-g199f618", which contains the git info.
 *
 * @param dig    (out) digest buffer pointer
 * @param dig_sz size of the buffer
 */
static int cpu1900_calc_hash(void *dig, size_t dig_sz)
{
	struct vb2_digest_context ctx;
	int                       rv;

	rv = vb2_digest_init(&ctx, VB2_HASH_SHA1);
	if (rv)
		return rv;

	/* compute over the last few bytes of the ROM */
	rv = vb2_digest_extend(&ctx, (const uint8_t *)(uint32_t)(-CB_HASH_SIZE), CB_HASH_SIZE);
	if (rv)
		return rv;

	return vb2_digest_finalize(&ctx, dig, dig_sz);
}


/**
 * Lookup the FMAP "RO_BOOTBLOCK_SHA1" and "COREBOOT".
 * Compute the SHA-1 hash over COREBOOT bootblock and compare against what is
 * in RO_BOOTBLOCK_SHA1.
 * On mismatch, log a scary warning, toggle the FPGA BIOS Next Chip Select and
 * reboot.
 */
static void cpu1900_verify_bios(void)
{
	struct region_device rdev_hash;
	uint8_t              dig[20];

	if (fmap_locate_area_as_rdev(HASH_REGION_NAME, &rdev_hash))
		return;

	if (region_device_sz(&rdev_hash) != sizeof(dig)) {
		printk(BIOS_WARNING, "CPU1900: Verification size mismatch: %s=%u dig=%u\n",
		       HASH_REGION_NAME,
		       (int)region_device_sz(&rdev_hash),
		       (int)sizeof(dig));
		return;
	}

	/* I'm (ab)using the VBOOT timing entries here... */
	timestamp_add_now(TS_START_VBOOT);
	post_code(0xd0);
	if ((cpu1900_calc_hash(dig, sizeof(dig)) == 0) &&
	    (memcmp(dig, rdev_mmap_full(&rdev_hash), sizeof(dig)) == 0)) {
		printk(BIOS_WARNING, "CPU1900: BIOS SHA-1 Verified\n");
		timestamp_add_now(TS_END_VBOOT);
		post_code(0xd1);

		/* set the BIOS happy bit */
		fpga_write_u8(CPU1900_REG_BIOS_BOOT,
		              fpga_read_u8(CPU1900_REG_BIOS_BOOT) | CPU1900_REG_BIOS_BOOT__HAPPY);
	} else {
		printk(BIOS_ALERT, "CPU1900: BIOS SHA-1 Check FAILED - Rebooting!\n");
		post_code(0xd2);

		/* Set the NEXT bit to be !BOOT */
		uint8_t val = fpga_read_u8(CPU1900_REG_BIOS_BOOT);
		if (val & CPU1900_REG_BIOS_BOOT__BOOT) {
			val &= ~CPU1900_REG_BIOS_BOOT__NEXT;
		} else {
			val |= CPU1900_REG_BIOS_BOOT__NEXT;
		}
		fpga_write_u8(CPU1900_REG_BIOS_BOOT, val);

		printk(BIOS_ALERT, "CPU1900: Wrote 0x%02x to BIOS BOOT reg\n", val);
		delay(1);
		cold_reset();
	}
}


/**
 * mainboard call for setup that needs to be done before fsp init
 */
void early_mainboard_romstage_entry(void)
{
	/* Set the LED to a 5 Hz red 50% blink */
	fpga_write_u8(CPU1900_REG_STATUS_LED_DUTY, CPU1900_LED_RED_BLINK);
	fpga_write_u8(CPU1900_REG_STATUS_LED_RATE, CPU1900_LED_5_HZ);

	printk(BIOS_NOTICE, "Reset:%s Stage:0x%02x Count:%d Sel:0x%02x[%s] Boot:0x%02x\n",
	       get_reset_cause_text(),
	       fpga_read_u8(CPU1900_REG_BIOS_LAST_STAGE),
	       fpga_read_u8(CPU1900_REG_BIOS_BOOT_COUNT),
	       fpga_read_u8(CPU1900_REG_BIOS_SELECT),
	       (fpga_read_u8(CPU1900_REG_BIOS_SELECT) & 1) ? "Alt" : "Def",
	       fpga_read_u8(CPU1900_REG_BIOS_BOOT));
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
	fpga_write_u8(CPU1900_REG_DTE_LED_DUTY, CPU1900_LED_RED);

	fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_CB_ROMSTAGE);

	cpu1900_verify_bios();
}


/**
 * /brief customize fsp parameters here if needed
 */
void romstage_fsp_rt_buffer_callback(FSP_INIT_RT_BUFFER *FspRtBuffer)
{
}
