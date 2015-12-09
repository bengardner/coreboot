/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * Copyright (C) 2013-2014 Sage Electronic Engineering, LLC.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* This file is derived from the flashrom project. */
#include <types.h>
#include <lib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bootstate.h>
#include <delay.h>
#include <arch/io.h>
#include <console/console.h>
#include <device/pci_ids.h>
#include <spi_flash.h>
#include <soc/spi.h>

#include <soc/lpc.h>
#include <soc/pci_devs.h>

#ifndef __SMM__
#include <device/device.h>
#include <device/pci.h>
#endif /* !__SMM__ */

typedef struct spi_slave ich_spi_slave;

static int ichspi_lock = 0;

typedef struct ich9_spi_regs {
	uint32_t bfpr;
	uint16_t hsfs;
	uint16_t hsfc;
	uint32_t faddr;
	uint32_t _reserved0;
	uint32_t fdata[16];
	uint32_t frap;
	uint32_t freg[5];
	uint32_t _reserved1[3];
	uint32_t pr[5];
	uint32_t _reserved2[2];
	uint8_t ssfs;
	uint8_t ssfc[3];
	uint16_t preop;
	uint16_t optype;
	uint8_t opmenu[8];
	uint8_t _reserved3[16];
	uint32_t fdoc;
	uint32_t fdod;
	uint8_t _reserved4[8];
	uint32_t afc;
	uint32_t lvscc;
	uint32_t uvscc;
	uint8_t _reserved5[4];
	uint32_t fpb;
	uint8_t _reserved6[28];
	uint32_t srdl;
	uint32_t srdc;
	uint32_t srd;
} __attribute__((packed)) ich9_spi_regs;

typedef struct ich_spi_controller {
	union {
		ich9_spi_regs *regs;
		void          *base;
	};
	int hw_mode;
	uint8_t *opmenu;
	int menubytes;
	uint16_t *preop;
	uint16_t *optype;
	uint32_t *addr;
	uint8_t *data;
	unsigned databytes;
	uint8_t *status;
	uint16_t *control;
} ich_spi_controller;

static ich_spi_controller cntlr;

enum {
	SPIS_SCIP =		0x0001,
	SPIS_GRANT =		0x0002,
	SPIS_CDS =		0x0004,
	SPIS_FCERR =		0x0008,
	SSFS_AEL =		0x0010,
	SPIS_LOCK =		0x8000,
	SPIS_RESERVED_MASK =	0x7ff0,
	SSFS_RESERVED_MASK =	0x7fe2
};

enum {
	SPIC_SCGO =		0x000002,
	SPIC_ACS =		0x000004,
	SPIC_SPOP =		0x000008,
	SPIC_DBC =		0x003f00,
	SPIC_DS =		0x004000,
	SPIC_SME =		0x008000,
	SSFC_SCF_MASK =		0x070000,
	SSFC_RESERVED =		0xf80000
};

enum {
	HSFS_FDONE =		0x0001,
	HSFS_FCERR =		0x0002,
	HSFS_AEL =		0x0004,
	HSFS_BERASE_MASK =	0x0018,
	HSFS_BERASE_SHIFT =	3,
	HSFS_SCIP =		0x0020,
	HSFS_FDOPSS =		0x2000,
	HSFS_FDV =		0x4000,
	HSFS_FLOCKDN =		0x8000
};

enum {
	HSFC_FGO =		0x0001,
	HSFC_FCYCLE__MASK =	0x0006,
	HSFC_FCYCLE__READ =	0x0000,
	HSFC_FCYCLE__WRITE =	0x0004,
	HSFC_FCYCLE__ERASE =	0x0006,
	HSFC_FCYCLE__SHIFT =	1,
	HSFC_FDBC_MASK =	0x3f00,
	HSFC_FDBC_SHIFT =	8,
	HSFC_FSMIE =		0x8000
};

enum {
	SPI_OPCODE_TYPE_READ_NO_ADDRESS =	0,
	SPI_OPCODE_TYPE_WRITE_NO_ADDRESS =	1,
	SPI_OPCODE_TYPE_READ_WITH_ADDRESS =	2,
	SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS =	3
};

#if IS_ENABLED(CONFIG_DEBUG_SPI_FLASH)

static u8 readb_(const void *addr)
{
	u8 v = read8(addr);
	printk(BIOS_DEBUG, "read %2.2x from %4.4x\n",
	       v, ((unsigned) addr & 0xffff) - 0xf020);
	return v;
}

static u16 readw_(const void *addr)
{
	u16 v = read16(addr);
	printk(BIOS_DEBUG, "read %4.4x from %4.4x\n",
	       v, ((unsigned) addr & 0xffff) - 0xf020);
	return v;
}

static u32 readl_(const void *addr)
{
	u32 v = read32(addr);
	printk(BIOS_DEBUG, "read %8.8x from %4.4x\n",
	       v, ((unsigned) addr & 0xffff) - 0xf020);
	return v;
}

static void writeb_(u8 b, void *addr)
{
	write8(addr, b);
	printk(BIOS_DEBUG, "wrote %2.2x to %4.4x\n",
	       b, ((unsigned) addr & 0xffff) - 0xf020);
}

static void writew_(u16 b, void *addr)
{
	write16(addr, b);
	printk(BIOS_DEBUG, "wrote %4.4x to %4.4x\n",
	       b, ((unsigned) addr & 0xffff) - 0xf020);
}

static void writel_(u32 b, void *addr)
{
	write32(addr, b);
	printk(BIOS_DEBUG, "wrote %8.8x to %4.4x\n",
	       b, ((unsigned) addr & 0xffff) - 0xf020);
}

#else /* CONFIG_DEBUG_SPI_FLASH ^^^ enabled  vvv NOT enabled */

#define readb_(a) read8(a)
#define readw_(a) read16(a)
#define readl_(a) read32(a)
#define writeb_(val, addr) write8(addr, val)
#define writew_(val, addr) write16(addr, val)
#define writel_(val, addr) write32(addr, val)

#endif  /* CONFIG_DEBUG_SPI_FLASH ^^^ NOT enabled */

static struct spi_flash *spi_hw_probe(struct spi_slave *spi);

static void write_reg(const void *value, void *dest, uint32_t size)
{
	const uint8_t *bvalue = value;
	uint8_t *bdest = dest;

	while (size >= 4) {
		writel_(*(const uint32_t *)bvalue, bdest);
		bdest += 4; bvalue += 4; size -= 4;
	}
	while (size) {
		writeb_(*bvalue, bdest);
		bdest++; bvalue++; size--;
	}
}

static void read_reg(const void *src, void *value, uint32_t size)
{
	const uint8_t *bsrc = src;
	uint8_t *bvalue = value;

	while (size >= 4) {
		*(uint32_t *)bvalue = readl_(bsrc);
		bsrc += 4; bvalue += 4; size -= 4;
	}
	while (size) {
		*bvalue = readb_(bsrc);
		bsrc++; bvalue++; size--;
	}
}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs)
{
	ich_spi_slave *slave = malloc(sizeof(*slave));

	if (!slave) {
		printk(BIOS_DEBUG, "ICH SPI: Bad allocation\n");
		return NULL;
	}

	spi_init();

	memset(slave, 0, sizeof(*slave));

	slave->bus = bus;
	slave->cs = cs;
	if (cntlr.hw_mode) {
		slave->force_programmer_specific = 1;
		slave->programmer_specific_probe = spi_hw_probe;
	}

	return slave;
}

static ich9_spi_regs *spi_regs(void)
{
	device_t dev;
	uint32_t sbase;

#ifdef __SMM__
	dev = PCI_DEV(0, LPC_DEV, LPC_FUNC);
#else
	dev = dev_find_slot(0, PCI_DEVFN(LPC_DEV, LPC_FUNC));
#endif
	sbase = pci_read_config32(dev, SBASE);
	sbase &= ~0x1ff;

	return (void *)sbase;
}

int __attribute__((weak)) mainboard_get_spi_config(struct spi_config *cfg)
{
	return -1;
}

void spi_init(void)
{
	if (cntlr.data)
		return;
	if (cntlr.regs)
		return;
	ich9_spi_regs *ich9_spi = spi_regs();
	cntlr.regs = ich9_spi;

#if ENV_RAMSTAGE
	struct spi_config cfg;

	if (mainboard_get_spi_config(&cfg) < 0) {
		printk(BIOS_DEBUG, "No SPI lockdown configuration.\n");
	} else {
		writew_(cfg.preop, &ich9_spi->preop);
		writew_(cfg.optype, &ich9_spi->optype);
		writel_(cfg.opmenu[0], ich9_spi->opmenu + 0);
		writel_(cfg.opmenu[1], ich9_spi->opmenu + 4);
		writew_(readw_(&ich9_spi->hsfs) | HSFS_FLOCKDN, &ich9_spi->hsfs);
		writel_(cfg.uvscc, &ich9_spi->uvscc);
		writel_(cfg.lvscc | VCL, &ich9_spi->lvscc);
	}
#endif
	/* Hardware mode if the flash descriptor is valid and VSCC is locked */
	if ((readw_(&ich9_spi->hsfs) & HSFS_FDV) != 0 &&
	    (readl_(&ich9_spi->lvscc) & VCL) != 0) {
		printk(BIOS_DEBUG, "SPI: Hardware Sequencing\n");
		cntlr.hw_mode = 1;
	} else {
		printk(BIOS_DEBUG, "SPI: Software Sequencing\n");
		cntlr.hw_mode = 0;
		ichspi_lock = readw_(&ich9_spi->hsfs) & HSFS_FLOCKDN;
		cntlr.opmenu = ich9_spi->opmenu;
		cntlr.menubytes = sizeof(ich9_spi->opmenu);
		cntlr.optype = &ich9_spi->optype;
		cntlr.addr = &ich9_spi->faddr;
		cntlr.data = (uint8_t *)ich9_spi->fdata;
		cntlr.databytes = sizeof(ich9_spi->fdata);
		cntlr.status = &ich9_spi->ssfs;
		cntlr.control = (uint16_t *)ich9_spi->ssfc;
		cntlr.preop = &ich9_spi->preop;
	}
}

static void spi_init_cb(void *unused)
{
	spi_init();
}

BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_ENTRY, spi_init_cb, NULL);

int spi_claim_bus(struct spi_slave *slave)
{
	/* Handled by ICH automatically. */
	return 0;
}

void spi_release_bus(struct spi_slave *slave)
{
	/* Handled by ICH automatically. */
}

typedef struct spi_transaction {
	const uint8_t *out;
	uint32_t bytesout;
	uint8_t *in;
	uint32_t bytesin;
	uint8_t type;
	uint8_t opcode;
	uint32_t offset;
} spi_transaction;

static inline void spi_use_out(spi_transaction *trans, unsigned bytes)
{
	trans->out += bytes;
	trans->bytesout -= bytes;
}

static inline void spi_use_in(spi_transaction *trans, unsigned bytes)
{
	trans->in += bytes;
	trans->bytesin -= bytes;
}

static void spi_setup_type(spi_transaction *trans)
{
	trans->type = 0xFF;

	/* Try to guess spi type from read/write sizes. */
	if (trans->bytesin == 0) {
		if (trans->bytesout > 4)
			/*
			 * If bytesin = 0 and bytesout > 4, we presume this is
			 * a write data operation, which is accompanied by an
			 * address.
			 */
			trans->type = SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS;
		else
			trans->type = SPI_OPCODE_TYPE_WRITE_NO_ADDRESS;
		return;
	}

	if (trans->bytesout == 1) { /* and bytesin is > 0 */
		trans->type = SPI_OPCODE_TYPE_READ_NO_ADDRESS;
		return;
	}

	if (trans->bytesout == 4) { /* and bytesin is > 0 */
		trans->type = SPI_OPCODE_TYPE_READ_WITH_ADDRESS;
	}

	/* Fast read command is called with 5 bytes instead of 4 */
	if (trans->out[0] == SPI_OPCODE_FAST_READ && trans->bytesout == 5) {
		trans->type = SPI_OPCODE_TYPE_READ_WITH_ADDRESS;
		--trans->bytesout;
	}
}

static int spi_setup_opcode(spi_transaction *trans)
{
	uint16_t optypes;
	uint8_t opmenu[cntlr.menubytes];

	trans->opcode = trans->out[0];
	spi_use_out(trans, 1);
	if (!ichspi_lock) {
		/* The lock is off, so just use index 0. */
		writeb_(trans->opcode, cntlr.opmenu);
		optypes = readw_(cntlr.optype);
		optypes = (optypes & 0xfffc) | (trans->type & 0x3);
		writew_(optypes, cntlr.optype);
		return 0;
	} else {
		/* The lock is on. See if what we need is on the menu. */
		uint8_t optype;
		uint16_t opcode_index;

		/* Write Enable is handled as atomic prefix */
		if (trans->opcode == SPI_OPCODE_WREN)
			return 0;

		read_reg(cntlr.opmenu, opmenu, sizeof(opmenu));
		for (opcode_index = 0; opcode_index < cntlr.menubytes;
				opcode_index++) {
			if (opmenu[opcode_index] == trans->opcode)
				break;
		}

		if (opcode_index == cntlr.menubytes) {
			printk(BIOS_DEBUG, "ICH SPI: Opcode %x not found\n",
				trans->opcode);
			return -1;
		}

		optypes = readw_(cntlr.optype);
		optype = (optypes >> (opcode_index * 2)) & 0x3;
		if (trans->type == SPI_OPCODE_TYPE_WRITE_NO_ADDRESS &&
			optype == SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS &&
			trans->bytesout >= 3) {
			/* We guessed wrong earlier. Fix it up. */
			trans->type = optype;
		}
		if (optype != trans->type) {
			printk(BIOS_DEBUG, "ICH SPI: Transaction doesn't fit type %d\n",
				optype);
			return -1;
		}
		return opcode_index;
	}
}

static int spi_setup_offset(spi_transaction *trans)
{
	/* Separate the SPI address and data. */
	switch (trans->type) {
	case SPI_OPCODE_TYPE_READ_NO_ADDRESS:
	case SPI_OPCODE_TYPE_WRITE_NO_ADDRESS:
		return 0;
	case SPI_OPCODE_TYPE_READ_WITH_ADDRESS:
	case SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS:
		trans->offset = ((uint32_t)trans->out[0] << 16) |
				((uint32_t)trans->out[1] << 8) |
				((uint32_t)trans->out[2] << 0);
		spi_use_out(trans, 3);
		return 1;
	default:
		printk(BIOS_DEBUG, "Unrecognized SPI transaction type %#x\n", trans->type);
		return -1;
	}
}

/*
 * Wait for up to 60ms til status register bit(s) turn 1 (in case wait_til_set
 * below is True) or 0. In case the wait was for the bit(s) to set - write
 * those bits back, which would cause resetting them.
 *
 * Return the last read status value on success or -1 on failure.
 */
static int ich_status_poll(u16 bitmask, int wait_til_set)
{
	int timeout = 40000; /* This will result in 400 ms */
	u16 status = 0;

	while (timeout--) {
		status = readw_(cntlr.status);
		if (wait_til_set ^ ((status & bitmask) == 0)) {
			if (wait_til_set)
				writew_((status & bitmask), cntlr.status);
			return status;
		}
		udelay(10);
	}

	printk(BIOS_DEBUG, "ICH SPI: SCIP timeout, read %x, expected %x\n",
		status, bitmask);
	return -1;
}

unsigned int spi_crop_chunk(unsigned int cmd_len, unsigned int buf_len)
{
	return min(cntlr.databytes, buf_len);
}

int spi_xfer(struct spi_slave *slave, const void *dout,
		unsigned int bytesout, void *din, unsigned int bytesin)
{
	uint16_t control;
	int16_t opcode_index;
	int with_address;
	int status;

	spi_transaction trans = {
		dout, bytesout,
		din, bytesin,
		0xff, 0xff, 0
	};

	/* There has to always at least be an opcode. */
	if (!bytesout || !dout) {
		printk(BIOS_DEBUG, "ICH SPI: No opcode for transfer\n");
		return -1;
	}
	/* Make sure if we read something we have a place to put it. */
	if (bytesin != 0 && !din) {
		printk(BIOS_DEBUG, "ICH SPI: Read but no target buffer\n");
		return -1;
	}

	if (ich_status_poll(SPIS_SCIP, 0) == -1)
		return -1;

	writew_(SPIS_CDS | SPIS_FCERR, cntlr.status);

	spi_setup_type(&trans);
	if ((opcode_index = spi_setup_opcode(&trans)) < 0)
		return -1;
	if ((with_address = spi_setup_offset(&trans)) < 0)
		return -1;

	if (!ichspi_lock && trans.opcode == SPI_OPCODE_WREN) {
		/*
		 * Treat Write Enable as Atomic Pre-Op if possible
		 * in order to prevent the Management Engine from
		 * issuing a transaction between WREN and DATA.
		 */
		writew_(trans.opcode, cntlr.preop);
		return 0;
	}

	/* Preset control fields */
	control = SPIC_SCGO | ((opcode_index & 0x07) << 4);

	/* Issue atomic preop cycle if needed */
	if (readw_(cntlr.preop))
		control |= SPIC_ACS;

	if (!trans.bytesout && !trans.bytesin) {
		/* SPI addresses are 24 bit only */
		if (with_address)
			writel_(trans.offset & 0x00FFFFFF, cntlr.addr);

		/*
		 * This is a 'no data' command (like Write Enable), its
		 * bytesout size was 1, decremented to zero while executing
		 * spi_setup_opcode() above. Tell the chip to send the
		 * command.
		 */
		writew_(control, cntlr.control);

		/* wait for the result */
		status = ich_status_poll(SPIS_CDS | SPIS_FCERR, 1);
		if (status == -1)
			return -1;

		if (status & SPIS_FCERR) {
			printk(BIOS_DEBUG, "ICH SPI: Command transaction error\n");
			return -1;
		}

		goto spi_xfer_exit;
	}

	/*
	 * Check if this is a write command attempting to transfer more bytes
	 * than the controller can handle. Iterations for writes are not
	 * supported here because each SPI write command needs to be preceded
	 * and followed by other SPI commands, and this sequence is controlled
	 * by the SPI chip driver.
	 */
	if (trans.bytesout > cntlr.databytes) {
		printk(BIOS_DEBUG, "ICH SPI: Too much to write. Does your SPI chip driver use"
		     " spi_crop_chunk()?\n");
		return -1;
	}

	/*
	 * Read or write up to databytes bytes at a time until everything has
	 * been sent.
	 */
	while (trans.bytesout || trans.bytesin) {
		uint32_t data_length;

		/* SPI addresses are 24 bit only */
		writel_(trans.offset & 0x00FFFFFF, cntlr.addr);

		if (trans.bytesout)
			data_length = min(trans.bytesout, cntlr.databytes);
		else
			data_length = min(trans.bytesin, cntlr.databytes);

		/* Program data into FDATA0 to N */
		if (trans.bytesout) {
			write_reg(trans.out, cntlr.data, data_length);
			spi_use_out(&trans, data_length);
			if (with_address)
				trans.offset += data_length;
		}

		/* Add proper control fields' values */
		control &= ~((cntlr.databytes - 1) << 8);
		control |= SPIC_DS;
		control |= (data_length - 1) << 8;

		/* write it */
		writew_(control, cntlr.control);

		/* Wait for Cycle Done Status or Flash Cycle Error. */
		status = ich_status_poll(SPIS_CDS | SPIS_FCERR, 1);
		if (status == -1)
			return -1;

		if (status & SPIS_FCERR) {
			printk(BIOS_DEBUG, "ICH SPI: Data transaction error\n");
			return -1;
		}

		if (trans.bytesin) {
			read_reg(cntlr.data, trans.in, data_length);
			spi_use_in(&trans, data_length);
			if (with_address)
				trans.offset += data_length;
		}
	}

spi_xfer_exit:
	/* Clear atomic preop now that xfer is done */
	writew_(0, cntlr.preop);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Below is the code for hardware sequencing

/**
 * Wait for the SCIP bit in the HSFSTS regsiter to clear.
 * Return the last read status value on success or -1 on timeout.
 */
static int spi_hw_wait_idle(void)
{
	int      timeout = 40000; /* This will result in 400 ms */
	uint16_t status = 0;

	while (timeout--) {
		status = readw_(cntlr.base + HSFSTS);
		if ((status & HSFS_SCIP) == 0)
			return status;
		udelay(10);
	}
	printk(BIOS_WARNING, "SF: idle timeout, status %x\n", status);
	return -1;
}

static int spi_hw_rw(struct spi_flash *flash, u32 offset, size_t len, void *buf, bool is_write)
{
	size_t   left = len;
	uint32_t addr;
	size_t   rw_size;
	uint16_t status;

	offset &= 0x00ffffff;
	addr    = offset;

	if (!flash || len == 0 || !buf)
		return CB_ERR;

	/* wait until SPI is idle */
	if (spi_hw_wait_idle() == -1)
		return CB_ERR;

	while (left > 0) {
		/* can't cross 4 KB or do more than 64 bytes */
		rw_size = min(64, min(left, 0x1000 - (addr & 0x0fff)));

		/* clear status from any previous transaction */
		writew_(HSFS_FDONE | HSFS_FCERR | HSFS_AEL,
			cntlr.base + HSFSTS);

		/* Set the SPI addresses */
		writel_(addr & 0x00FFFFFF, cntlr.base + FADDR);

		/* write the data */
		if (is_write)
			write_reg(buf, cntlr.base + FDATA, rw_size);

		/* start the write transaction */
		writew_(((rw_size - 1) << HSFC_FDBC_SHIFT) | HSFC_FGO |
			(is_write ? HSFC_FCYCLE__WRITE : HSFC_FCYCLE__READ),
			cntlr.base + HSFCTL);

		/* wait until SPI SCIP is clear */
		status = spi_hw_wait_idle();
		if (status == -1)
			return CB_ERR;

		/* check for error */
		if (status & (HSFS_AEL | HSFS_FCERR)) {
			printk(BIOS_WARNING, "BIOS SPI: %s Error: 0x%04x\n",
			       is_write ? "Write" : "Read", status);
			return CB_ERR;
		}

		/* read in the results */
		if (!is_write)
			read_reg(cntlr.base + FDATA, buf, rw_size);

#if IS_ENABLED(CONFIG_DEBUG_SPI_FLASH)
		printk(BIOS_DEBUG, "SPI: %s %zd bytes at %p\n",
		       is_write ? "wrote" : "read", rw_size, (void *)addr);
		hexdump(buf, rw_size);
#endif

		/* advance pointers */
		buf  += rw_size;
		addr += rw_size;
		left -= rw_size;
	}
	return CB_SUCCESS;
}

static int spi_hw_read(struct spi_flash *flash, u32 offset, size_t len, void *buf)
{
	return spi_hw_rw(flash, offset, len, buf, false);
}

static int spi_hw_write(struct spi_flash *flash, u32 offset, size_t len, const void *buf)
{
	return spi_hw_rw(flash, offset, len, (void *)buf, true);
}

/**
 * Erases blocks of data.
 * The offset must start at the start of a block and the length must be a
 * multiple of the erase block size.
 */
static int spi_hw_erase(struct spi_flash *flash, u32 offset, size_t len)
{
	uint16_t status;
	int      blk_size, blk_mask;

	if (!flash || len == 0)
		return CB_ERR;

	offset &= 0x00ffffff;

	blk_size = flash->sector_size;
	blk_mask = blk_size - 1;
	if (((offset & blk_mask) != 0) || ((len & blk_mask) != 0)) {
		printk(BIOS_ERR, "SF Erase: Bad offset [%p] or length [%zd] for block size %d\n",
		       (void *)offset, len, blk_size);
		return CB_ERR;
	}

	/* wait until SPI is idle */
	if (spi_hw_wait_idle() == -1)
		return CB_ERR;

	while (len > 0) {
		/* clear status from any previous transaction */
		writew_(HSFS_FDONE | HSFS_FCERR | HSFS_AEL,
			cntlr.base + HSFSTS);

		/* Set the SPI addresses (any address in the block is OK) */
		writel_(offset & 0x00FFFFFF, cntlr.base + FADDR);

		/* start the erase transaction */
		writew_(HSFC_FCYCLE__ERASE | HSFC_FGO, cntlr.base + HSFCTL);

		/* wait until SPI SCIP is clear (this takes a while) */
		status = spi_hw_wait_idle();
		if (status == -1)
			return CB_ERR;

		/* check for error */
		if (status & (HSFS_AEL | HSFS_FCERR)) {
			printk(BIOS_WARNING, "SF: Erase Error: 0x%04x\n", status);
			return CB_ERR;
		}

#if IS_ENABLED(CONFIG_DEBUG_SPI_FLASH)
		printk(BIOS_DEBUG, "SF: Erased block %p\n", (void *)offset);
#endif
		offset += blk_size;
		len    -= blk_size;
	}
	return CB_SUCCESS;
}

static int spi_hw_status(struct spi_flash *flash, u8 *reg)
{
	/* Fake it - doesn't look this is used anywhere */
	if (flash && reg) {
		*reg = 0;
		return CB_SUCCESS;
	}
	return CB_ERR;
}

static int decode_berase(int val)
{
	static const int sizes[4] = { 0x00100, 0x01000, 0x02000, 0x10000 };
	return sizes[val & 0x03];
}

static struct spi_flash *spi_hw_probe(struct spi_slave *spi)
{
	struct spi_flash *flash;

	flash = malloc(sizeof(*flash));
	if (!flash) {
		printk(BIOS_WARNING, "SF: Failed to allocate memory\n");
		return NULL;
	}

	flash->spi = spi;
	flash->name = "HWSeq";

	/* Assumptions:
	 *  - The BIOS region is located at the end of the flash (0xffffff)
	 *  - LVSCC.lbes == UVSCC.ubes (same erase size)
	 */
	uint32_t bfpr = readl_(cntlr.base + BFPREG);
	flash->size = 1 + ((bfpr >> 4) | 0x0fff); /* bit 24:12 are in 28:16 */

	uint32_t lvscc = readl_(cntlr.base + LVSCC);
	flash->sector_size = decode_berase(lvscc);
	flash->erase_cmd = (lvscc >> 8) & 0xff; // N/A for HW sequencing
	flash->status_cmd = 0; // N/A for HW sequencing
	flash->read = spi_hw_read;
	flash->write = spi_hw_write;
	flash->erase = spi_hw_erase;
	flash->status = spi_hw_status;

	return flash;
}
