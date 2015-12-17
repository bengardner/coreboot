/**
 * Keeps track of each boot. Should generally be called once.
 * The file "bootlog.bin" is used. It must be multiple erase blocks.
 * For example, if the erase block size is 4 KB, then it must be at least 8 KB.
 *
 * Copyright (C) 2015 Wabtec Railway Electronics
 * License: GPL 2 or later
 */
#include "bootlog.h"
#include <spi_flash.h>
#include <rtc.h>
#include <lib.h>
#include <cbfs.h>
#include <console/console.h>


#define BOOTLOG_FILENAME   "boot_log.bin"


static struct spi_flash *bios_flash_init(void)
{
	static struct spi_flash *spi_bios_flash;

	if (!spi_bios_flash) {
		spi_bios_flash = spi_flash_probe(0, 0);
	}
	return spi_bios_flash;
}

static enum cb_err bios_flash_write(uint32_t offset, size_t len, const void *data)
{
	struct spi_flash *flash = bios_flash_init();
	if (flash) {
		return flash->write(flash, offset, len, data);
	}
	return CB_ERR;
}

static int bios_flash_erase_size(void)
{
	struct spi_flash *flash = bios_flash_init();
	if (flash) {
		return flash->sector_size;
	}
	return CB_ERR;
}

static enum cb_err bios_flash_erase(uint32_t offset, size_t length)
{
	struct spi_flash *flash = bios_flash_init();
	if (flash) {
		return flash->erase(flash, offset, length);
	}
	return CB_ERR;
}

/**
 * Returns whether the entry is blank.
 */
static int entry_is_blank(const bootlog_record_t *ptr)
{
	return (ptr->year == 0xffff);
}

/**
 * Finds the first blank entry in the file.
 * Assumes that the file is filled from index 0 and the next page is erased
 * after the current is filled.
 *
 * @param file_ptr  pointer to the first record in the file
 * @param file_size the number of records in the file
 * @param page_size the number of records in a page
 */
static bootlog_record_t *find_blank(bootlog_record_t *file_ptr, int file_recs, int page_recs)
{
	int idx;
	int page_idx = -1;

	/* check the last entry in each page to find the first page with a blank */
	for (idx = 0; idx < file_recs; idx += page_recs) {
		if (entry_is_blank(&file_ptr[idx + page_recs - 1])) {
			page_idx = idx;
			break;
		}
	}

	/* do a scan of the page to find the first blank entry */
	if (page_idx >= 0) {
		bootlog_record_t *base_ptr = file_ptr + page_idx;
		while (page_recs > 0) {
			page_recs--;
			if (entry_is_blank(base_ptr))
				return base_ptr;
			base_ptr++;
		}
	}
	return NULL;
}


/**
 * Find an empty slot and return the offset to that area.
 * Returns 0 on error.
 */
static uint32_t bootlog_find_blank(uint32_t *prev_cnt)
{
	int              page_bytes, page_recs;
	size_t           file_bytes, file_recs;
	bootlog_record_t *file_ptr;
	bootlog_record_t *cur_ptr;
	bootlog_record_t *prev_ptr;

	/* find the file storage - if missing, this is a config error */
	file_ptr = cbfs_boot_map_with_leak(BOOTLOG_FILENAME, CBFS_TYPE_RAW, &file_bytes);
	if (!file_ptr) {
		printk(BIOS_ERR, "%s: not found '%s'\n", __func__, BOOTLOG_FILENAME);
		return -1;
	}
	file_recs = file_bytes / sizeof(bootlog_record_t);

	/* grab the page size - failure is a config error */
	page_bytes = bios_flash_erase_size();
	if (page_bytes < 0) {
		printk(BIOS_ERR, "%s: bios_flash_erase_size() failed\n", __func__);
		return -1;
	}
	page_recs = page_bytes / sizeof(bootlog_record_t);

	/* sanity check need at least 2 erase pages */
	if ((file_bytes <= page_bytes) ||
	    ((file_bytes & (page_bytes - 1)) != 0)) {
		printk(BIOS_ERR, "%s: '%s' bad size %zd for page size %d\n",
		       __func__, BOOTLOG_FILENAME, file_bytes, page_bytes);
		return -1;
	}

	cur_ptr = find_blank(file_ptr, file_recs, page_recs);

	/* if we didn't find any blanks, then we need to erase the first page */
	if (cur_ptr == NULL) {
		bios_flash_erase((uintptr_t)file_ptr, page_bytes);
		cur_ptr = file_ptr;
	}

	/* find the previous entry */
	if (cur_ptr > file_ptr) {
		prev_ptr = cur_ptr - 1;
	} else {
		prev_ptr = &file_ptr[file_recs - 1];
	}

	/* if the prev_ptr entry is blank, then we get a count of 0xffffffff,
	 * so the next count will be zero.
	 */
	*prev_cnt = prev_ptr->number;

	/* if this entry is the last in a page, then we need to erase the next
	 * page.
	 */
	if (((intptr_t)cur_ptr & (page_bytes - 1)) == (page_bytes - 16)) {
		if (cur_ptr == &file_ptr[file_recs - 1]) {
			// last entry in the file, erase first page
			bios_flash_erase((uintptr_t)file_ptr, page_bytes);
		} else {
			bios_flash_erase((uintptr_t)(cur_ptr + 1), page_bytes);
		}
	}

	return ((intptr_t)cur_ptr) & 0x00ffffff;
}


void bootlog_store(bootlog_record_t *rec)
{
	int32_t         rec_off;
	struct rtc_time dt;
	uint32_t        prev_cnt = 0;

	rec_off = bootlog_find_blank(&prev_cnt);
	if (rec_off < 0) {
		printk(BIOS_WARNING, "%s: find failed\n", __func__);
		return;
	}

	if (rtc_get(&dt) < 0) {
		printk(BIOS_WARNING, "%s: rtc_get failed\n", __func__);
		return;
	}

	rec->year   = dt.year;
	rec->month  = dt.mon;
	rec->day    = dt.mday;
	rec->hour   = dt.hour;
	rec->minute = dt.min;
	rec->second = dt.sec;
	rec->number = prev_cnt + 1;

	bios_flash_write(rec_off, sizeof(*rec), rec);

	printk(BIOS_NOTICE, "%s: 0x%08x: %04d-%02d-%02d %02d:%02d:%02d"
	                    " num=%d reason=%d core=%dC ext=%dC %02x %02x\n",
	       __func__, rec_off,
	       rec->year, rec->month, rec->day, rec->hour, rec->minute, rec->second,
	       rec->number, rec->reason, rec->temp_core, rec->temp_ext,
	       rec->fpga_slot_id, rec->fpga_options);
}
