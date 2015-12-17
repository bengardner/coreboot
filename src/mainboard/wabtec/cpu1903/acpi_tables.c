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
#include <baytrail/acpi.h>
#include <baytrail/nvs.h>
#include <baytrail/iomap.h>
#ifdef BNG_ACPI_I2C_TEST
#include <baytrail/pci_devs.h>
#endif

void acpi_create_gnvs(global_nvs_t *gnvs)
{
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

#ifdef BNG_ACPI_I2C_TEST
	/* Enable I2C 1-3 for ACPI */
	int lpss_idx;
	for (lpss_idx = 1; lpss_idx <= 3; lpss_idx++) {
		device_t dev = dev_find_slot(0, PCI_DEVFN(I2C1_DEV, lpss_idx));
		if (dev->enabled && dev->initialized) {
			gnvs->dev.lpss_en[lpss_idx] = 1;
			gnvs->dev.lpss_bar0[lpss_idx] = pci_read_config32(dev, PCI_BASE_ADDRESS_0);
			gnvs->dev.lpss_bar1[lpss_idx] = pci_read_config32(dev, PCI_BASE_ADDRESS_1);
			printk(BIOS_INFO, "Enabled I2C-%d @ %p %p\n",
			       lpss_idx,
			       (void *)gnvs->dev.lpss_bar0[lpss_idx],
			       (void *)gnvs->dev.lpss_bar1[lpss_idx]);
		}
	}
#endif
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
