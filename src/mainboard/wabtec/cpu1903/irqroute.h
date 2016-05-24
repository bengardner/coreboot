/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2013 Google Inc.
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

#ifndef IRQROUTE_H
#define IRQROUTE_H

#include <soc/intel/fsp_baytrail/include/soc/irq.h>
#include <soc/intel/fsp_baytrail/include/soc/pci_devs.h>

/*
 *                              device 0123
 *                                     4567
 * IR 02h GFX      INT(A)       - PIRQ A--- (02.0) - MSI
 * IR 10h EMMC     INT(ABCD)    - PIRQ x---  -
 * IR 11h SDIO     INT(A)       - PIRQ x---  - SDIO for Wifi, not used
 * IR 12h SD       INT(A)       - PIRQ x---  -
 * IR 13h SATA     INT(A)       - PIRQ A--- (13.0) - MSI
 * IR 14h XHCI     INT(A)       - PIRQ A--- (14.0) - MSI
 * IR 15h LP Audio INT(A)       - PIRQ G--- (15.0 - ACPI only?)
 * IR 17h MMC      INT(A)       - PIRQ B--- (17.0)
 * IR 18h SIO      INT(ABCD)    - PIRQ CCCC (18.0 .. 18.6) DMA, I2C-0..6
 * IR 1Ah TXE      INT(A)       - PIRQ F---  - Disabled
 * IR 1Bh HD Audio INT(A)       - PIRQ G---  - Using LPE
 * IR 1Ch PCIe     INT(ABCD)    - PIRQ A-AA (1c.0, 1c.2, 1c.3) - MSI
 * IR 1Dh EHCI     INT(A)       - PIRQ A---  Same as XHCI, only 1 can be used
 * IR 1Eh SIO      INT(ABCD)    - PIRQ DD-E (1e.0, 1e.3, 1e.4, 1e.5)
 * IR 1Fh LPC      INT(ABCD)    - PIRQ ---F (1f.0, 1f.3)
 *
 * A = MSI devices
 * B = eMMC
 * C = All 18.x (DMA, I2C)
 * D = 1e.0 DMA, HS UART#2, SPI
 * E = Eth0
 * F = HS UART#1 (LOS178 assignment)
 * F = SMBus
 * G = LPE, Wifi (PCIE), SMBus
 * H = Eth1
 *
 * PCI addresses that are used on our board (only interrupt capable shown)
 *  00:02.0 - [A---] GFX_DEV - GPU (i915), MSI
 *  00:13.0 - [A---] SATA_DEV - SATA (ACHI), MSI
 *  00:14.0 - [A---] XHCI_DEV - XHCI (USB), MSI
 *  00:15.0 - [G---] LPE_DEV - Audio ACPI only
 *  00:17.0 - [B---] MMC45_DEV - eMMC
 *  00:18.0 - [C---] SIO1_DEV - DMA
 *  00:18.1 - [-C--] SIO1_DEV - I2C-0 All I2C can be assigned the same IRQ, same as DMA
 *  00:18.2 - [--C-] SIO1_DEV - I2C-1
 *  00:18.3 - [---C] SIO1_DEV - I2C-2
 *  00:18.4 - [C---] SIO1_DEV - I2C-3
 *  00:18.5 - [-C--] SIO1_DEV - I2C-4
 *  00:18.6 - [--C-] SIO1_DEV - I2C-5
 *  00:1c.0 - [E---] BRIDGE1_DEV - PCIE Bridge 0, MSI-X
 *  00:1c.2 - [--F-] BRIDGE1_DEV - PCIE Bridge 2, MSI-X
 *  00:1c.3 - [---G] BRIDGE1_DEV - PCIE Bridge 3, MSI-X
 *  00:1d.0 - [A---] EHCI_DEV - EHCI (USB), MSI (toggled with XHCI)
 *  00:1e.0 - [D---] SIO2_DEV - DMA
 *  00:1e.3 - [--F-] SIO2_DEV - HS UART#1 -- needs its own IRQ for LynxSecure
 *  00:1e.4 - [D---] SIO2_DEV - HS UART#2 - can be same as DMA (1e.0)
 *  00:1e.5 - [-D--] SIO2_DEV - PCU-SPI
 *  00:1f.3 - [---G] PCU_DEV - SMBus
 *  01:00.0 - i210 Ethernet (back), MSI-X
 *  02:00.0 - Wifi, MSI-X (?)
 *  03:00.0 - i210 Ethernet (front), MSI-X
 */

/* PCIe bridge routing */
#define BRIDGE1_DEV	PCIE_DEV

/* PCI bridge IRQs need to be updated in both tables and need to match */
#define PCIE_BRIDGE_IRQ_ROUTES \
	PCIE_BRIDGE_DEV(RP, BRIDGE1_DEV,    B, B, B, B)

/* Devices set as A, A, A, A evaluate as 0, and don't get set */
#define PCI_DEV_PIRQ_ROUTES \
	PCI_DEV_PIRQ_ROUTE(GFX_DEV,     B, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(EMMC_DEV,    H, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(SDIO_DEV,    H, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(SD_DEV,      H, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(SATA_DEV,    B, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(XHCI_DEV,    B, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(LPE_DEV,     G, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(MMC45_DEV,   A, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(SIO1_DEV,    C, C, C, C), \
	PCI_DEV_PIRQ_ROUTE(TXE_DEV,     H, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(HDA_DEV,     G, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(BRIDGE1_DEV, B, B, B, B), \
	PCI_DEV_PIRQ_ROUTE(EHCI_DEV,    B, H, H, H), \
	PCI_DEV_PIRQ_ROUTE(SIO2_DEV,    D, D, F, D), \
	PCI_DEV_PIRQ_ROUTE(PCU_DEV,     G, G, G, G)

/*
 * Route each PIRQ[A-H] to a PIC IRQ[0-15]
 * Reserved: 0, 1, 2, 8, 13
 * PS2 keyboard: 12
 * ACPI/SCI: 9
 * Floppy: 6
 *
 * NOTE: Linux seems to like the D & E reversal better (D=11, E=10)
 */
#define PIRQ_PIC_ROUTES \
	PIRQ_PIC(A,  4), \
	PIRQ_PIC(B,  5), \
	PIRQ_PIC(C,  7), \
	PIRQ_PIC(D, 11), \
	PIRQ_PIC(E, 10), \
	PIRQ_PIC(F, 12), \
	PIRQ_PIC(G, 14), \
	PIRQ_PIC(H, 15)

#endif /* IRQROUTE_H */
