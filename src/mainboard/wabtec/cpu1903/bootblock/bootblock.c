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

	/* Increment the boot count.
	 * Clear the test bits when the count rolls over to 0 to break out of an
	 * endless reset loop.
	 * Also clear the tests when a failure is detected and copy the failure bit.
	 */
	u8 val, tst, cnt, bbr;

	val  = fpga_read_u8(CPU1900_REG_BIOS_BOOT_COUNT);
	tst  = val & CPU1900_REG_BIOS_BOOT_COUNT__TEST;
	cnt  = val + 1;
	cnt &= CPU1900_REG_BIOS_BOOT_COUNT__COUNT;
	val &= ~(CPU1900_REG_BIOS_BOOT_COUNT__TEST | CPU1900_REG_BIOS_BOOT_COUNT__COUNT);

	if (cnt == 0) {
		if ((tst > CPU1900_REG_BIOS_BOOT_COUNT__TEST__NONE) &&
			 (tst < CPU1900_REG_BIOS_BOOT_COUNT__TEST__RES_OVERFLOW)) {
			tst = CPU1900_REG_BIOS_BOOT_COUNT__TEST__RES_OVERFLOW;
		}
	}

	bbr = fpga_read_u8(CPU1900_REG_BIOS_BOOT);
	if ((bbr & CPU1900_REG_BIOS_BOOT__FAILED) != 0) {
		tst = CPU1900_REG_BIOS_BOOT_COUNT__TEST__RES_FAILOVER;
		cnt = 0;
	} else if (tst >= CPU1900_REG_BIOS_BOOT_COUNT__TEST__RES_OVERFLOW) {
		tst = CPU1900_REG_BIOS_BOOT_COUNT__TEST__NONE;
		cnt = 0;
	}
	fpga_write_u8(CPU1900_REG_BIOS_BOOT_COUNT, val | tst | cnt);

	/* set the BIOS alive bit if TEST_ALIVE not set */
	if ((tst != CPU1900_REG_BIOS_BOOT_COUNT__TEST__ALIVE_REBOOT) &&
		 (tst != CPU1900_REG_BIOS_BOOT_COUNT__TEST__ALIVE_HANG)) {
		fpga_write_u8(CPU1900_REG_BIOS_BOOT, bbr | CPU1900_REG_BIOS_BOOT__ALIVE);
	}
}
