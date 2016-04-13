/**
 * Wabtec CPU-1900 FPGA I/O wrapper.
 */
#ifndef WABTEC_CPU1900_IO_H_INCLUDED
#define WABTEC_CPU1900_IO_H_INCLUDED

#include "wabtec-cpu1900.h"

static inline uint8_t fpga_read_u8(u16 reg)
{
	if (reg < CPU1900_FPGA_REG_SIZE)
		return inb(CPU1900_FPGA_REG_BASE + reg);
	return -1;
}

static inline uint16_t fpga_read_u16(u16 reg)
{
	if (reg < CPU1900_FPGA_REG_SIZE - 1)
		return inw(CPU1900_FPGA_REG_BASE + reg);
	return -1;
}

static inline void fpga_write_u8(u16 reg, u8 val)
{
	if (reg < CPU1900_FPGA_REG_SIZE)
		outb(val, CPU1900_FPGA_REG_BASE + reg);
}

static inline void fpga_write_u16(u16 reg, u16 val)
{
	if (reg < CPU1900_FPGA_REG_SIZE - 1)
		return outw(val, CPU1900_FPGA_REG_BASE + reg);
}

#endif /* WABTEC_CPU1900_IO_H_INCLUDED */
