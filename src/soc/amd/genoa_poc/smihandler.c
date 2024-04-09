/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <acpi/acpi.h>
#include <amdblocks/acpi.h>
#include <amdblocks/acpimmio.h>
#include <amdblocks/psp.h>
#include <amdblocks/smi.h>
#include <amdblocks/smm.h>
#include <arch/hlt.h>
#include <arch/io.h>
#include <console/console.h>
#include <cpu/x86/cache.h>
#include <cpu/x86/smm.h>
#include <soc/smi.h>
#include <soc/smu.h>
#include <soc/southbridge.h>
#include <types.h>

/*
 * Both the psp_notify_sx_info and the smu_sx_entry call will clobber the SMN index register
 * during the SMN accesses. Since the SMI handler is the last thing that gets called before
 * entering S3, this won't interfere with any indirect SMN accesses via the same register pair.
 */
static void fch_slp_typ_handler(void)
{
	uint32_t pci_ctrl;
	uint16_t pm1cnt;
	uint8_t slp_typ, rst_ctrl;

	/* Figure out SLP_TYP */
	pm1cnt = acpi_read16(MMIO_ACPI_PM1_CNT_BLK);
	printk(BIOS_SPEW, "SMI#: SLP = 0x%04x\n", pm1cnt);
	slp_typ = acpi_sleep_from_pm1(pm1cnt);

	/* Do any mainboard sleep handling */
	mainboard_smi_sleep(slp_typ);

	switch (slp_typ) {
	case ACPI_S0:
		printk(BIOS_DEBUG, "SMI#: Entering S0 (On)\n");
		break;
	case ACPI_S3:
		printk(BIOS_DEBUG, "SMI#: Entering S3 (Suspend-To-RAM)\n");
		break;
	case ACPI_S4:
		printk(BIOS_DEBUG, "SMI#: Entering S4 (Suspend-To-Disk)\n");
		break;
	case ACPI_S5:
		printk(BIOS_DEBUG, "SMI#: Entering S5 (Soft Power off)\n");
		break;
	default:
		printk(BIOS_DEBUG, "SMI#: ERROR: SLP_TYP reserved\n");
		break;
	}

	if (slp_typ >= ACPI_S3) {
		wbinvd();

		clear_all_smi_status();

		/* Do not send SMI before AcpiPm1CntBlkx00[SlpTyp] */
		pci_ctrl = pm_read32(PM_PCI_CTRL);
		pci_ctrl &= ~FORCE_SLPSTATE_RETRY;
		pm_write32(PM_PCI_CTRL, pci_ctrl);

		/* Enable SlpTyp */
		rst_ctrl = pm_read8(PM_RST_CTRL1);
		rst_ctrl |= SLPTYPE_CONTROL_EN;
		pm_write8(PM_RST_CTRL1, rst_ctrl);

		smu_sx_entry(); /* Leave SlpTypeEn clear, SMU will set */
		printk(BIOS_ERR, "System did not go to sleep\n");
		hlt();
	}
}

/*
 * Table of functions supported in the SMI handler.  Note that SMI source setup
 * in fch.c is unrelated to this list.
 */
static const struct smi_sources_t smi_sources[] = {
	{ .type = SMITYPE_SMI_CMD_PORT, .handler = fch_apmc_smi_handler },
	{ .type = SMITYPE_SLP_TYP, .handler = fch_slp_typ_handler},
};

void *get_smi_source_handler(int source)
{
	size_t i;

	for (i = 0 ; i < ARRAY_SIZE(smi_sources) ; i++)
		if (smi_sources[i].type == source)
			return smi_sources[i].handler;

	return NULL;
}
