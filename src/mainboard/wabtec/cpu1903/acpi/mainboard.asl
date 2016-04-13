/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2011 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#define USE_PRP0001	1

Device (PWRB)
{
	Name(_HID, EisaId("PNP0C0C"))
}

Device (FPGA) // CPU1900 FPGA fixed IO address
{
	Name (_HID, "WABT1900")
	Name (_UID, 0)
	Name (_CRS, ResourceTemplate()
	{
		WordIO(ResourceConsumer, MinFixed, MaxFixed, SubDecode, EntireRange,
			0,                     // AddressGranulatiry
			0x1100, 0x12ff,        // AddressMinimum, AddressMaximum
			0,                     // AddressTranslation
			0x0200,                // RangeLength
			,,,,
		)
	})
}

Scope (\_SB.PCI0.I2C2)
{
	Device (RTC0) // CPU1900 RTC DS3431 on I2C-2 addr 0x68
	{
#ifdef USE_PRP0001
		Name (_HID, "PRP0001")
		Name (_DSD, Package () {
			ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
			Package () {
				Package () {"compatible", "maxim,ds3231"},
			},
		})
#endif
		Name (_CID, Package() { "ds3231" })
		Name (_CRS, ResourceTemplate () {
			I2cSerialBus (
				0x0068, ControllerInitiated, 400000,
				AddressingMode7Bit, "\\_SB.PCI0.I2C2", 0x00,
				ResourceConsumer,,)
		})
	}

	Device (LCD0) // CPU1900 PTN3460 on I2C-2 addr 0x40
	{
		Name (_HID, "PTN3460")
		Name (_CRS, ResourceTemplate () {
			I2cSerialBus (
				0x0040, ControllerInitiated, 400000,
				AddressingMode7Bit, "\\_SB.PCI0.I2C2", 0x00,
				ResourceConsumer,,)
		})
		// this is only present on CDUs and some CPUs
		Method (_STA)
		{
			If (LEqual (\S240, 1)) {
				Return (0xF)
			} Else {
				Return (0x0)
			}
		}
	}

	Device (GP01) // CPU1900 PCA9539 on I2C-2 addr 0x74
	{
		Name (_HID, "INT3491")
		Name (_CID, "PCA9539")
		Name (_CRS, ResourceTemplate () {
			I2cSerialBus (
				0x0074, ControllerInitiated, 400000,
				AddressingMode7Bit, "\\_SB.PCI0.I2C2", 0x00,
				ResourceConsumer,,)
		})
		// this is only present on some CPUs
		Method (_STA)
		{
			If (LEqual (\S274, 1)) {
				Return (0xF)
			} Else {
				Return (0x0)
			}
		}
	}
}

Scope (\_SB.PCI0.I2C3)
{
	Device (AT24)
	{
#ifdef USE_PRP0001
		Name (_HID, "PRP0001")
		Name (_DSD, Package () {
			ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
			Package () {
				Package () {"compatible", Package () {"st,m24c02", "linux,at24"}},
				Package () {"size", 256},
				Package () {"pagesize", 16},
			},
		})
#endif
		Name (_CID, Package() { "24c02" })
		Name (_CRS, ResourceTemplate () {
			I2cSerialBus (
				0x0057, ControllerInitiated, 400000,
				AddressingMode7Bit, "\\_SB.PCI0.I2C3", 0x00,
				ResourceConsumer,,)
		})
	}

	Device (TMP0) // CPU1900 TMP75C on I2C-3 addr 0x48
	{
#ifdef USE_PRP0001
		Name (_HID, "PRP0001")
		Name (_DSD, Package () {
			ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
			Package () {
				Package () {"compatible", "ti,tmp75c"},
			},
		})
#endif
		Name (_CID, Package() { "tmp75c" })
		Name (_CRS, ResourceTemplate () {
			I2cSerialBus (
				0x0048, ControllerInitiated, 400000,
				AddressingMode7Bit, "\\_SB.PCI0.I2C3", 0x00,
				ResourceConsumer,,)
		})
	}
}

Scope (\_SB.PCI0.I2C5)
{
	Device (COD0) // CPU1900 TLV320AIC3204 (CODEC) on I2C5 addr 0x18
	{
		Name (_HID, "104C3204")
		Name (_CID, Package() { "AIC3204" })
		Name (_CRS, ResourceTemplate () {
			I2cSerialBus (
				0x0018, ControllerInitiated, 400000,
				AddressingMode7Bit, "\\_SB.PCI0.I2C5", 0x00,
				ResourceConsumer,,)
		})

		// this is only present on CDUs and some CPUs
		Method (_STA)
		{
			If (LEqual (\S518, 1)) {
				Return (0xF)
			} Else {
				Return (0x0)
			}
		}
	}
}

Scope (\_SB.PCI0)
{
	/* just giving it a name */
	Device (SMB0)
	{
		Name (_DDN, "SMBus Controller #1")
		Name (_ADR, 0x001f0003)

		Device (SPD0)
		{
#ifdef USE_PRP0001
			Name (_HID, "PRP0001")
			Name (_DSD, Package () {
				ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
				Package () {
					Package () {"compatible", Package () {"st,m24c02", "linux,at24"}},
					Package () {"size", 256},
					Package () {"pagesize", 16},
					Package () {"read-only", 1},
				},
			})
#endif
			Name (_CID, Package() { "24c02" })
			Name (_CRS, ResourceTemplate () {
				I2cSerialBus (
					0x0050, ControllerInitiated, 400000,
					AddressingMode7Bit, "\\_SB.PCI0.SMB0", 0x00,
					ResourceConsumer,,)
			})
		}

		Device (SPD1)
		{
#ifdef USE_PRP0001
			Name (_HID, "PRP0001")
			Name (_DSD, Package () {
				ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
				Package () {
					Package () {"compatible", Package () {"st,m24c02", "linux,at24"}},
					Package () {"size", 256},
					Package () {"pagesize", 16},
					Package () {"read-only", 1},
				},
			})
#endif
			Name (_CID, Package() { "24c02" })
			Name (_CRS, ResourceTemplate () {
				I2cSerialBus (
					0x0051, ControllerInitiated, 400000,
					AddressingMode7Bit, "\\_SB.PCI0.SMB0", 0x00,
					ResourceConsumer,,)
			})
		}
	}
}
