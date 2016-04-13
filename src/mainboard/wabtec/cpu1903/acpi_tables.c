/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2012 Google Inc.
 * Copyright (C) 2013-2014 Sage Electronic Engineering, LLC.
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

#include <types.h>
#include <string.h>
#include <cbmem.h>
#include <console/console.h>
#include <arch/acpi.h>
#include <arch/ioapic.h>
#include <arch/acpigen.h>
#include <arch/smp/mpspec.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ids.h>
#include <cpu/cpu.h>
#include <cpu/x86/msr.h>
#include <soc/acpi.h>
#include <soc/nvs.h>
#include <soc/iomap.h>
#ifdef BNG_ACPI_I2C_TEST
#include <soc/pci_devs.h>
#endif
#include <soc/i2c.h>

void acpi_create_gnvs(global_nvs_t *gnvs)
{
	u8 buf[4];

	acpi_init_gnvs(gnvs);

	/* Enable USB ports in S3 */
	gnvs->s3u0 = 1;
	gnvs->s3u1 = 1;

	/* Disable USB ports in S5 */
	gnvs->s5u0 = 0;
	gnvs->s5u1 = 0;

	/* TPM Present */
	gnvs->tpmp = 0;

	/* Enable DPTF */
	gnvs->dpte = 0;

	// Check for PTN3460 on I2C-1 (ACPI I2C2) at 0x40.
	if (i2c_read(1, 0x40, 0, buf, sizeof(buf)) == I2C_SUCCESS) {
		gnvs->dev.S240_PTN3460 = 1;
		printk(BIOS_NOTICE, "Found PTN3460\n");
	}

	// Check for PCA9539 on I2C-1 (ACPI I2C2) at 0x74.
	if (i2c_read(1, 0x74, 0, buf, sizeof(buf)) == I2C_SUCCESS) {
		gnvs->dev.S274_PCA9539 = 1;
		printk(BIOS_NOTICE, "Found PCA9539\n");
	}

	// Check for TLV320AIC3204 on I2C-4 (ACPI I2C5) at 0x18.
	if (i2c_read(4, 0x18, 0, buf, sizeof(buf)) == I2C_SUCCESS) {
		gnvs->dev.S518_AIC3204 = 1;
		printk(BIOS_NOTICE, "Found AIC3204\n");
	}
}

unsigned long acpi_fill_madt(unsigned long current)
{
	/* Local APICs */
	current = acpi_create_madt_lapics(current);

	/* IOAPIC */
	current += acpi_create_madt_ioapic((acpi_madt_ioapic_t *) current,
				2, IO_APIC_ADDR, 0);

	current = acpi_madt_irq_overrides(current);

	return current;
}
