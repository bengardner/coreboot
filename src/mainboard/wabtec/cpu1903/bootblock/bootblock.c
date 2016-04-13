#include "mainboard/wabtec/cpu1903/wabtec-cpu1900-io.h"

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
	fpga_write_u8(CPU1900_REG_STATUS_LED_DUTY, CPU1900_LED_RED_BLINK);
	fpga_write_u8(CPU1900_REG_STATUS_LED_RATE, CPU1900_LED_10_HZ);

	/* TEST: Put all the PCIe devices into reset */
	fpga_write_u8(CPU1900_REG_RESET_1,
	              CPU1900_REG_RESET_1__PCIE_RESET |
	              CPU1900_REG_RESET_1__FP_ETH_RESET |
	              CPU1900_REG_RESET_1__BP_ETH_RESET);

	fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_CB_BOOTBLOCK);

	/* set the BIOS alive bit */
	fpga_write_u8(CPU1900_REG_BIOS_BOOT,
	              fpga_read_u8(CPU1900_REG_BIOS_BOOT) | CPU1900_REG_BIOS_BOOT__ALIVE);
}
