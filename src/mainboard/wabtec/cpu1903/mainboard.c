/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2007-2009 coresystems GmbH
 * Copyright (C) 2011 The ChromiumOS Authors.  All rights reserved.
 * Copyright (C) 2014-2015 Sage Electronic Engineering, LLC
 * Copyright (C) 2015 Wabtec Railway Electronics
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
#include <device/device.h>
#include <device/pci_def.h>
#include <device/pci_ops.h>
#include <console/console.h>
#if CONFIG_VGA_ROM_RUN
#include <x86emu/x86emu.h>
#endif
#include <string.h>
#include <delay.h>
#include <smbios.h>
#include <soc/i2c.h>
#include <soc/msr.h>
#include "crc16.h"
#include <pc80/mc146818rtc.h>
#include <arch/acpi.h>
#include <arch/io.h>
#include <arch/interrupt.h>
#include <boot/coreboot_tables.h>
#include <bootstate.h>
#include <cpu/x86/msr.h>
#include "wabtec-cpu1900-io.h"
#include "bootlog.h"
#include <version.h>

#define CEV_BLOCK_0    (1 << 0)
#define CEV_BLOCK_1    (1 << 1)
#define CEV_BLOCK_2    (1 << 2)
#define CEV_BLOCK_3    (1 << 3)
#define CEV_MODS       (1 << 4) // mods chksum is OK
#define CEV_MODS_NZ    (1 << 5) // at least 1 mod is present
#define CEV_DATE       (1 << 6) // date chksum is OK
#define CEV_TOPPART    (1 << 7) // toppart is present

typedef struct cpu1900_eeprom_t {
	char     valid;            // see CEV_xxx
	char     part_board[16];   //  0-13
	uint16_t feature;          // 14-15
	char     model[16];        // 16-29
	char     serial[16];       // 32-45
	char     part_top[16];     // 48-61
	uint8_t  hw_mods[15];      // 64-79  bit array (0-120)
	uint8_t  hw_date[7];       // 80-87  YY YY MM DD HH MM SS
} cpu1900_eeprom_t;
static cpu1900_eeprom_t cpu1900_eeprom;


static int is_print(int ch)
{
	return ((ch >= 32) && (ch <= 127));
}

/**
 * Computes a simple sum of all bytes.
 * The lowest 8 bits of the number should be 0xff to be valid.
 * If the whole number is 0 (sum == 0xff), then the field is not valid.
 */
static int cksum_ok(const uint8_t *data, int len)
{
	int ck_sum = 0;

	while (len > 0) {
		ck_sum += *data;
		data++;
		len--;
	}
	return (ck_sum & 0xff) == 0xff;
}


/**
 * Reads the first 128 bytes of the EEPROM.
 *
 * The 128 byte EEPROM is divided into 4 32-byte blocks.
 * Here is the current layout:
 * Block  Offset  Length  Description
 *   0       0      14    The WRE Part Number of the CPU board
 *   0      14       2    Feature bits
 *   0      16      14    The Model Number (ie, 'CPU-400', 'CDU-300', etc)
 *   1       0      14    The Serial Number
 *   1      14       2    Unused (Default to 0)
 *   1      16      14    The chassis Part number
 *   2       0      16    The hardware mods (bit fields + 1-byte csum)
 *   2      16       8    The modification date (Y Y M D H M S csum)
 *   2      24       6    Unused (Default to 0xff)
 *   3       0       4    The hours of operation, MSB-first
 *   3       4       3    Key 1 Press counter
 *   3       7       3    Key 2 Press counter
 *   3      10       3    Key 3 Press counter
 *   3      13       3    Key 4 Press counter
 *   3      16       3    Key 5 Press counter
 *   3      19       3    Key 6 Press counter
 *   3      22       3    Key 7 Press counter
 *   3      25       3    Key 8 Press counter
 *   3      28       2    Unused (Default to 0xff)
 */
static void m24c02_read(void)
{
	uint8_t eeprom[128];
	int     ret, idx;

	ret = i2c_read(2, 0x57, 0, eeprom, 128);
	if (ret != I2C_SUCCESS) {
		printk(BIOS_WARNING, "m24c02_read: i2c_read() failed: %d\n", ret);
		strcpy(cpu1900_eeprom.part_board, "<error>");
		strcpy(cpu1900_eeprom.model, "<error>");
		strcpy(cpu1900_eeprom.serial, "<error>");
		strcpy(cpu1900_eeprom.part_top, "<error>");
		return;
	}

	for (idx = 0; idx < 4; idx++) {
		cpu1900_eeprom.valid |= crc16_w1_valid(eeprom + (32 * idx), 32) ? (1 << idx) : 0;
	}
	if (cpu1900_eeprom.valid & CEV_BLOCK_0) {
		memcpy(cpu1900_eeprom.part_board, eeprom + 0, 14);
		cpu1900_eeprom.feature = (eeprom[14] << 8) | eeprom[15];
		memcpy(cpu1900_eeprom.model, eeprom + 16, 14);
	} else {
		strcpy(cpu1900_eeprom.part_board, "<invalid>");
		strcpy(cpu1900_eeprom.model, "<invalid>");
	}
	if (cpu1900_eeprom.valid & CEV_BLOCK_1) {
		memcpy(cpu1900_eeprom.serial, eeprom + 32, 14);
		memcpy(cpu1900_eeprom.part_top, eeprom + 48, 14);
		if (is_print(cpu1900_eeprom.part_top[0])) {
			cpu1900_eeprom.valid |= CEV_TOPPART;
		}
		if (cksum_ok(eeprom + 64, 16)) {
			cpu1900_eeprom.valid |= CEV_MODS;
			memcpy(cpu1900_eeprom.hw_mods, eeprom + 64, 15);
			for (idx = 0; idx < sizeof(cpu1900_eeprom.hw_mods); idx++) {
				if (cpu1900_eeprom.hw_mods[idx] != 0) {
					cpu1900_eeprom.valid |= CEV_MODS_NZ;
					break;
				}
			}
		}
		if (cksum_ok(eeprom + 80, 8) && (eeprom[80] != 0)) {
			cpu1900_eeprom.valid |= CEV_DATE;
			memcpy(cpu1900_eeprom.hw_date, eeprom + 80, 7);
		}
	} else {
		strcpy(cpu1900_eeprom.serial, "<invalid>");
		strcpy(cpu1900_eeprom.part_top, "<invalid>");
	}
	//printk(BIOS_WARNING, "EEPROM valid:      [0x%02x]\n",
	//       cpu1900_eeprom.valid);
	printk(BIOS_NOTICE, "EEPROM part board: [%s]\n",
	       cpu1900_eeprom.part_board);
	printk(BIOS_NOTICE, "EEPROM part top:   [%s]\n",
	       cpu1900_eeprom.part_top);
	printk(BIOS_NOTICE, "EEPROM model:      [%s]\n",
	       cpu1900_eeprom.model);
	printk(BIOS_NOTICE, "EEPROM serial:     [%s]\n",
	       cpu1900_eeprom.serial);
	printk(BIOS_NOTICE, "EEPROM feature:    [0x%04x]\n",
	       cpu1900_eeprom.feature);
}

const char *smbios_mainboard_serial_number(void)
{
	return cpu1900_eeprom.serial;
}

static int mods_to_str(const uint8_t *mods, int mods_len, char *buf, uint32_t size)
{
	int last_bit = -1;
	int is_range = 0;
	int bit;
	int len = 0;

	if (!buf || !size)
		return 0;

	for (bit = 0; bit < mods_len; bit++) {
		int idx = bit >> 3;
		int msk = 1 << (bit & 7);

		if (mods[idx] & msk) {
			if (last_bit == -1) {
				len += snprintf(&buf[len], size - len, "%d,", bit);
			} else {
				is_range = 1;
			}
			last_bit = bit;
		} else {
			if (is_range) {
				buf[len - 1] = '-'; /* change last comma to a dash */
				len         += snprintf(&buf[len], size - len, "%d,", last_bit);
				is_range     = 0;
			}
			last_bit = -1;
		}
	}

	/* handle a range that ends on the last bit */
	if (is_range && (last_bit != -1)) {
		buf[len - 1] = '-'; /* change last comma to a dash */
		len         += snprintf(&buf[len], size - len, "%d", last_bit);
	} else {
		/* Eat the last comma */
		if (len > 0) {
			len--;
		}
	}

	buf[len] = 0;
	return len;
}

const char *smbios_mainboard_bios_version(void)
{
	return coreboot_version;
}

const char *smbios_mainboard_version(void)
{
	static char tmp[128];
	int         hw_rev = fpga_read_u8(CPU1900_REG_HW_REV) & 0x0f;
	int         len;

	len = snprintf(tmp, sizeof(tmp), "%d", hw_rev);
	if (cpu1900_eeprom.valid & CEV_MODS_NZ) {
		len += snprintf(tmp + len, sizeof(tmp) - len, " mods:");
		len += mods_to_str(cpu1900_eeprom.hw_mods, 15 * 8, tmp + len, sizeof(tmp) - len);
	}
	if (cpu1900_eeprom.valid & CEV_DATE) {
		len += snprintf(tmp + len, sizeof(tmp) - len, " date:%04d-%02d-%02d",
		                (cpu1900_eeprom.hw_date[0] << 8) | cpu1900_eeprom.hw_date[1],
		                cpu1900_eeprom.hw_date[2],
		                cpu1900_eeprom.hw_date[3]);
	}
	return tmp;
}


const char *smbios_mainboard_product_name(void)
{
	return cpu1900_eeprom.model;
}


#define TMP75C_I2C_BUS    2
#define TMP75C_I2C_ADDR   0x48

#define TMP75C_REG_INPUT  0
#define TMP75C_REG_CONFIG 1
#define TMP75C_REG_HYST   2
#define TMP75C_REG_HIGH   3

#define TMP75C_TEMP_MIN (-55000)
#define TMP75C_TEMP_MAX 125000

/* configuration */
#define TMP75C_ALERT_HYST  120
#define TMP75C_ALERT_LIMIT 125

/**
 * The tmp75c defines 5 16 bit registers. MSB is sent first.
 */
static int tmp75c_read_reg(int bus, int addr, int reg)
{
	uint8_t tmp[2];
	int     ret;

	ret = i2c_read(bus, addr, reg, tmp, 2);
	if (ret == I2C_SUCCESS) {
		int val = (tmp[0] << 8) | tmp[1];
		printk(BIOS_INFO, "tmp75c_read_reg(%d) => 0x%04x\n", reg, val);
		return val;
	}
	printk(BIOS_WARNING, "tmp75c_read_reg(%d): failed: %d\n", reg, ret);
	return -1;
}


static int tmp75c_write_reg(int bus, int addr, int reg, uint16_t val)
{
	uint8_t tmp[2];
	int     ret;

	tmp[0] = val >> 8;
	tmp[1] = val & 0xff;

	ret = i2c_write(bus, addr, reg, tmp, 2);
	if (ret == I2C_SUCCESS) {
		printk(BIOS_INFO, "tmp75c_write_reg(%d, 0x%04x)\n", reg, val);
		return 0;
	}
	printk(BIOS_WARNING, "tmp75c_write_reg(%d, 0x%04x): failed: %d\n",
	       reg, val, ret);
	return -1;
}

/**
 * reg is 12 bits with 1/16 deg C resolution.
 * bits 0-7 are the fraction
 * bits 8-15 are the whole number.
 *
 * To convert a REG to temp, drop the unused bits, multiply by 1000 and divide
 * by 16.
 * @return temperature in deg C * 1000
 */
static int tmp75c_reg_to_degx1000(uint16_t val)
{
	return ((val >> 4) * 1000) >> 4;
}

/**
 * Convert a whole number deg C to a register value.
 */
static uint16_t tmp75c_deg_to_reg(int deg_c)
{
	if (deg_c > TMP75C_TEMP_MAX)
		deg_c = TMP75C_TEMP_MAX;
	if (deg_c < TMP75C_TEMP_MIN)
		deg_c = TMP75C_TEMP_MIN;
	return (deg_c << 8);
}

static void tmp75c_print_temp(const char *name, int val)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%6d", tmp75c_reg_to_degx1000(val));

	printk(BIOS_NOTICE, "tmp75c: %s %3.3s.%3.3s deg C\n", name, buf, buf + 3);
}

/**
 * Read the current temperature and set the Tlow and Thigh and config regs.
 */
static int tmp75c_setup(int bus, int addr, int deg_hyst, int deg_high)
{
	uint16_t old_regs[4];
	uint16_t new_regs[4];
	int      ret;
	int      idx;
	int      temp;

	for (idx = 0; idx < ARRAY_SIZE(old_regs); idx++) {
		ret = tmp75c_read_reg(bus, addr, idx);
		if (ret == -1)
			return 0xff;
		old_regs[idx] = ret;
		new_regs[idx] = ret;
	}

	/* set FQ to 3 for 6 samples */
	new_regs[TMP75C_REG_CONFIG] |= (1 << 11) | (1 << 12);

	/* set the alert limts */
	new_regs[TMP75C_REG_HYST] = tmp75c_deg_to_reg(deg_hyst);
	new_regs[TMP75C_REG_HIGH] = tmp75c_deg_to_reg(deg_high);

	/* commit changes */
	for (idx = 1; idx < ARRAY_SIZE(old_regs); idx++) {
		if ((new_regs[idx] != old_regs[idx]) &&
		    (tmp75c_write_reg(bus, addr, idx, new_regs[idx]) == -1))
			return 0xff;
	}
	temp = new_regs[TMP75C_REG_INPUT];
	tmp75c_print_temp("Input", temp);
	printk(BIOS_NOTICE, "tmp75c: Config 0x%04x\n", new_regs[TMP75C_REG_CONFIG]);
	tmp75c_print_temp("Hyst ", new_regs[TMP75C_REG_HYST]);
	tmp75c_print_temp("High ", new_regs[TMP75C_REG_HIGH]);
	return (temp + 128) >> 8;
}


/**
 * The CMOS RTC gets trashed at every reboot.
 * Copy the time from the RTC on I2C-1 0x68 to the CMOS time.
 * rtc_ds3232_read: 00 : 25 - seconds (00-59)
 * rtc_ds3232_read: 01 : 04 - minutes (00-59)
 * rtc_ds3232_read: 02 : 21 - hours   (00-23)
 * rtc_ds3232_read: 03 : 05 - day     (1-7)
 * rtc_ds3232_read: 04 : 01 - date    (1-31)
 * rtc_ds3232_read: 05 : 90 - month/cent (month & 0x1f) (cent = 0x80)
 * rtc_ds3232_read: 06 : 15 - year (bottom 2 digits)
 */
static void rtc_ds3232_read(void)
{
	uint8_t rtc_data[7];
	int     ret;

	ret = i2c_read(1, 0x68, 0, rtc_data, sizeof(rtc_data));
	if (ret != I2C_SUCCESS) {
		printk(BIOS_WARNING, "rtc_ds3232_read: i2c_read() failed: %d\n", ret);
		return;
	}

	printk(BIOS_DEBUG, "rtc_ds3232_read: %02x %02x %02x %02x %02x %02x %02x\n",
	       rtc_data[0], rtc_data[1], rtc_data[2], rtc_data[3],
	       rtc_data[4], rtc_data[5], rtc_data[6]);

	// TODO: validate RTC values before copying to CMOS RTC?

	cmos_write(rtc_data[0], RTC_CLK_SECOND);
	cmos_write(rtc_data[1], RTC_CLK_MINUTE);
	cmos_write(rtc_data[2], RTC_CLK_HOUR);
	cmos_write(rtc_data[3], RTC_CLK_DAYOFWEEK);
	cmos_write(rtc_data[4], RTC_CLK_DAYOFMONTH);
	cmos_write(rtc_data[5] & 0x1f, RTC_CLK_MONTH);
	cmos_write(rtc_data[6], RTC_CLK_YEAR);
	// assume cent=1 means '20', so cent=0 is '21'
	cmos_write((rtc_data[5] & 0x80) ? 0x20 : 0x21, RTC_CLK_ALTCENTURY);

	printk(BIOS_NOTICE, "RTC: Updated to %02x%02x-%02x-%02x %02x:%02x:%02x day of week=%x\n",
	       cmos_read(RTC_CLK_ALTCENTURY),
	       cmos_read(RTC_CLK_YEAR),
	       cmos_read(RTC_CLK_MONTH),
	       cmos_read(RTC_CLK_DAYOFMONTH),
	       cmos_read(RTC_CLK_HOUR),
	       cmos_read(RTC_CLK_MINUTE),
	       cmos_read(RTC_CLK_SECOND),
	       cmos_read(RTC_CLK_DAYOFWEEK));
}


static int coretemp_show(void)
{
	msr_t m;
	int   tjmax, temp;

	m = rdmsr(MSR_IA32_TEMPERATURE_TARGET);
	tjmax = (m.lo >> 16) & 0xff;

	m = rdmsr(MSR_IA32_THERM_STATUS);
	temp  = tjmax - ((m.lo >> 16) & 0x7f);

	printk(BIOS_NOTICE, "Core Temp: %d deg C\n", temp);
	return temp;
}

/** the I2C transaction should take 100 us, but may take up to 300 us */
#define I2C_GPIO_USEC   100

static int cpu1900_fpga_i2c_wait_idle(void)
{
	int retry = 5;
	u8  val;

	while ((((val = fpga_read_u8(CPU1900_REG_I2C_CS)) & CPU1900_REG_I2C_CS__BUSY) != 0) &&
	       (retry-- > 0)) {
		udelay(I2C_GPIO_USEC);
	}
	if (val & CPU1900_REG_I2C_CS__BUSY)
		printk(BIOS_WARNING, "CPU1900 FPGA I2C idle timeout\n");
	return val;
}


static int cpu1900_fpga_i2c_read(void)
{
	fpga_write_u8(CPU1900_REG_I2C_CS,
	              CPU1900_REG_I2C_CS__RW | CPU1900_REG_I2C_CS__BANK__IN);
	cpu1900_fpga_i2c_wait_idle();
	return fpga_read_u16(CPU1900_REG_I2C_IN_0);
}


static int cpu1900_fpga_read_slotid(void)
{
	u8 val;
	u8 sid = fpga_read_u8(CPU1900_REG_SLOTID) & 0x7f;

	/* see if the I2C interface is present (means SlotID is not) */
	val = fpga_read_u8(CPU1900_REG_I2C_INFO);
	if ((val & CPU1900_REG_I2C_INFO__PRESENT) == 0)
		return sid;

	/* make sure we have the right board revision */
	val &= CPU1900_REG_I2C_INFO__REVISION;
	if (val == CPU1900_REG_I2C_INFO__REVISION__PCA9539) {
		sid &= 0x60; // keep VIN1, VIN2 status
		return sid | (cpu1900_fpga_i2c_read() & 0x1f);
	}
	printk(BIOS_ERR, "CPU1900 FPGA I2C Unknown revision 0x%02x\n", val);
	return sid; // fallback to whatever is in the SLOTID register
}


/*
 * mainboard_enable is executed as first thing in dev_enumerate(),
 * before the call to scan_bus(root, 0);
 * This is the earliest point to add customization.
 */
static void mainboard_enable(device_t dev)
{
	struct resource *res;
	device_t lpc_dev;
	int io_index = 1;
	bootlog_record_t rec;

	memset(&rec, 0, sizeof(rec));

	/*
	 * Add the FPGA's I/O resource at 0x1100-0x12ff on the LPC bus.
	 * The LPC default I/O range (0x0000-0x0fff) is added at the
	 * equivalent of io_index=0. Thusly we add the FPGA's
	 * resource at io_index=1;
	 * This prevents coreboot from reserving overlapping I/O space.
	 * We also add an ACPI entry (FPGA in mainboard.asl) to prevent the OS
	 * from allocating overlapping I/O.
	 */
	lpc_dev = dev_find_slot(0, PCI_DEVFN(0x1f, 0));
	res = new_resource(lpc_dev, IOINDEX_SUBTRACTIVE(io_index, 0));
	res->base = 0x1100;
	res->size = 0x200;
	res->flags = IORESOURCE_IO | IORESOURCE_SUBTRACTIVE |
	             IORESOURCE_ASSIGNED | IORESOURCE_FIXED;

	/* We have I2C devices on I2C 1, 2, and 4 (ACPI I2C2, I2C3, I2C5) */
	i2c_init(1);
	i2c_init(2);
	i2c_init(4);

	rtc_ds3232_read();

	// read the I2C EEPROM containing the serial number, etc
	m24c02_read();

	rec.temp_ext  = tmp75c_setup(2, 0x48, 120, 125);
	rec.temp_core = coretemp_show();
	rec.reason = fpga_read_u8(CPU1900_REG_RESET_CAUSE) & 0x0f;
	rec.fpga_slot_id = cpu1900_fpga_read_slotid() & 0x7f;
	rec.fpga_slot_id |= fpga_read_u8(CPU1900_REG_BIOS_BOOT) << 7;
	rec.fpga_options = fpga_read_u8(CPU1900_REG_FPGA_OPTIONS);
	bootlog_store(&rec);

	printk(BIOS_INFO, "CPU1900: DTE LED green\n");
	fpga_write_u8(CPU1900_REG_DTE_LED_DUTY, CPU1900_LED_GREEN);

	/* FIXME: (TEST CODE) increment the boot count */
	fpga_write_u8(CPU1900_REG_BIOS_BOOT_COUNT,
	              fpga_read_u8(CPU1900_REG_BIOS_BOOT_COUNT) + 1);
}

static void cpu1900_ramstage_start(void *unused)
{
	printk(BIOS_INFO, "CPU1900: Status LED green 10 Hz\n");
	fpga_write_u8(CPU1900_REG_STATUS_LED_DUTY, CPU1900_LED_GREEN_BLINK);
	fpga_write_u8(CPU1900_REG_STATUS_LED_RATE, CPU1900_LED_10_HZ);

	fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_CB_RAMSTAGE);

	/* TEST: wait 100 ms, take all the PCIe devices out of reset, wait 100 ms */
	mdelay(100);
	fpga_write_u8(CPU1900_REG_RESET_1, 0);
	fpga_write_u8(CPU1900_REG_RESET_2, 0);
	mdelay(100);
}

static void cpu1900_after_pci_enum(void *unused)
{
	printk(BIOS_INFO, "CPU1900: DTE LED off\n");
	fpga_write_u8(CPU1900_REG_DTE_LED_DUTY, CPU1900_LED_BLACK);
}

static void cpu1900_payload_boot(void *unused)
{
	printk(BIOS_INFO, "CPU1900: Status LED green 5 Hz\n");
	fpga_write_u8(CPU1900_REG_STATUS_LED_DUTY, CPU1900_LED_GREEN_BLINK);
	fpga_write_u8(CPU1900_REG_STATUS_LED_RATE, CPU1900_LED_5_HZ);

	fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_CB_PAYLOAD);
}

BOOT_STATE_INIT_ENTRY(BS_PRE_DEVICE, BS_ON_ENTRY, cpu1900_ramstage_start, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_ENUMERATE, BS_ON_EXIT, cpu1900_after_pci_enum, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, cpu1900_payload_boot, NULL);

struct chip_operations mainboard_ops = {
	.enable_dev = mainboard_enable,
};
