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

	/* Put all the PCIe devices into reset */
	fpga_write_u8(CPU1900_REG_RESET_1,
	              CPU1900_REG_RESET_1__PCIE_RESET |
	              CPU1900_REG_RESET_1__FP_ETH_RESET |
	              CPU1900_REG_RESET_1__BP_ETH_RESET);

	/* Note boot progress */
	fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_CB_BOOTBLOCK);

	/* Increment the boot count */
	fpga_write_u8(CPU1900_REG_CB_BOOT_COUNT, fpga_read_u8(CPU1900_REG_CB_BOOT_COUNT) + 1);

	u8 tst, cnt, bbr, res;

	/* See if a test is active */
	res = CPU1900_REG_CB_RES__M__NONE;
	tst = fpga_read_u8(CPU1900_REG_CB_TEST);
	cnt = 0;
	if (tst != CPU1900_REG_CB_TEST__M__NONE) {
		if (tst > CPU1900_REG_CB_TEST__M__HASH_FAIL) {
			/* invalid test value */
			tst = CPU1900_REG_CB_TEST__M__NONE;
			res = CPU1900_REG_CB_RES__M__INVALID;
		} else {
			cnt = fpga_read_u8(CPU1900_REG_CB_TEST_COUNT);
			if (cnt == 0) {
				/* test expired due to count hitting 0 */
				tst = CPU1900_REG_CB_TEST__M__NONE;
				res = CPU1900_REG_CB_RES__M__COUNT;
			} else {
				/* test still active */
				cnt--;
			}
		}
	}

	/* handle a BIOS boot failure switch */
	bbr = fpga_read_u8(CPU1900_REG_BIOS_BOOT);
	if ((bbr & CPU1900_REG_BIOS_BOOT__FAILED) != 0) {
		res = CPU1900_REG_CB_RES__M__FAILOVER;
		tst = CPU1900_REG_CB_TEST__M__NONE;
		cnt = 0;
	}

	/* save off registers */
	fpga_write_u8(CPU1900_REG_CB_TEST, tst);
	fpga_write_u8(CPU1900_REG_CB_TEST_COUNT, cnt);
	fpga_write_u8(CPU1900_REG_CB_RES, res);

	/* set the BIOS alive bit if TEST_ALIVE not set */
	if ((tst != CPU1900_REG_CB_TEST__M__ALIVE_REBOOT) &&
		 (tst != CPU1900_REG_CB_TEST__M__ALIVE_HANG)) {
		fpga_write_u8(CPU1900_REG_BIOS_BOOT, bbr | CPU1900_REG_BIOS_BOOT__ALIVE);
	}
}
