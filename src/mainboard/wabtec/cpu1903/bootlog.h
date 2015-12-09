/**
 * Keeps track of each boot. Should generally be called once.
 * The file "boot_log.bin" is used. It must be multiple erase blocks.
 * For example, if the erase block size is 4 KB, then it must be at least 8 KB.
 *
 * Copyright (C) 2015 Wabtec Railway Electronics
 * License: GPL 2 or later
 */
#ifndef _WABTEC_CPU1900_BOOTLOG_H_
#define _WABTEC_CPU1900_BOOTLOG_H_

#include <types.h>

/* fields are native byte order (LSB) pad to 32 bytes */
typedef struct bootlog_record_t
{
	uint16_t year;
	uint8_t  month;
	uint8_t  day;
	uint8_t  hour;
	uint8_t  minute;
	uint8_t  second;
	uint8_t  reason;
	uint32_t number;
	uint8_t  temp_core;
	uint8_t  temp_ext;
	uint8_t  fpga_slot_id;
	uint8_t  fpga_options;
} __attribute__((packed)) bootlog_record_t;

/**
 * Function populates RTC time and number.
 * Caller must populate reason, temp_core, temp_ext, etc.
 *
 * @param rec in/out the record to store/that was stored
 */
void bootlog_store(bootlog_record_t *rec);

#endif /* _WABTEC_CPU1900_BOOTLOG_H_ */
