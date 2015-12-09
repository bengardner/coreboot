#include "mainboard/wabtec/cpu1903/wabtec-cpu1900.h"

static void bootblock_mainboard_init(void)
{
#ifdef CONFIG_BOOTBLOCK_NORTHBRIDGE_INIT
	bootblock_northbridge_init();
#endif
#ifdef CONFIG_BOOTBLOCK_SOUTHBRIDGE_INIT
	bootblock_southbridge_init();
#endif
#ifdef CONFIG_BOOTBLOCK_CPU_INIT
	bootblock_cpu_init();
#endif

	/* Set the status LED to a very fast RED 50% blink */
	outb(CPU1900_LED_RED_BLINK, CPU1900_REG_STATUS_LED_DUTY);
	outb(CPU1900_LED_10_HZ, CPU1900_REG_STATUS_LED_RATE);

	/* TEST: Put all the PCIe devices into reset */
	outb(CPU1900_REG_RESET_1__PCIE_RESET |
	     CPU1900_REG_RESET_1__FP_ETH_RESET |
	     CPU1900_REG_RESET_1__BP_ETH_RESET,
	     CPU1900_REG_RESET_1);
}

#ifdef CONFIG_STATIC_OPTION_TABLE_NAME
const char *cmos_default_name(void)
{
	if ((inb(CPU1900_REG_DBG) & CPU1900_REG_DBG_MSK) == CPU1900_REG_DBG_VAL)
		return "cmos.debug.default";
	return "cmos.default";
}
#endif
