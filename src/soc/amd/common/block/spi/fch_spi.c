/* SPDX-License-Identifier: GPL-2.0-only */
/* This file is part of the coreboot project. */

#include <amdblocks/chip.h>
#include <amdblocks/lpc.h>
#include <amdblocks/spi.h>
#include <arch/mmio.h>
#include <console/console.h>
#include <soc/iomap.h>
#include <stdint.h>

static uintptr_t fch_spi_base(void)
{
	uintptr_t base;

	base = lpc_get_spibase();

	if (base)
		return base;

	lpc_set_spibase(SPI_BASE_ADDRESS);
	lpc_enable_spi_rom(SPI_ROM_ENABLE);

	return SPI_BASE_ADDRESS;
}

static void fch_spi_set_spi100(int norm, int fast, int alt, int tpm)
{
	uintptr_t base = fch_spi_base();

	write16((void *)(base + SPI100_SPEED_CONFIG), SPI_SPEED_CFG(norm, fast, alt, tpm));
	write16((void *)(base + SPI100_ENABLE), SPI_USE_SPI100);
}

static void fch_spi_disable_4dw_burst(void)
{
	uintptr_t base = fch_spi_base();
	uint16_t val = read16((void *)(base + SPI100_HOST_PREF_CONFIG));

	write16((void *)(base + SPI100_HOST_PREF_CONFIG), val & ~SPI_RD4DW_EN_HOST);
}

static void fch_spi_set_read_mode(u32 mode)
{
	uintptr_t base = fch_spi_base();
	uint32_t val = read32((void *)(base + SPI_CNTRL0)) & ~SPI_READ_MODE_MASK;

	write32((void *)(base + SPI_CNTRL0), val | SPI_READ_MODE(mode));
}

static void fch_spi_config_mb_modes(void)
{
	const struct soc_amd_common_config *cfg = soc_get_common_config();

	if (!cfg)
		die("Common config structure is NULL!\n");

	const struct spi_config *spi_cfg = &cfg->spi_config;

	fch_spi_set_read_mode(spi_cfg->read_mode);
	fch_spi_set_spi100(spi_cfg->normal_speed, spi_cfg->fast_speed,
			   spi_cfg->altio_speed, spi_cfg->tpm_speed);
}

static void fch_spi_config_em100_modes(void)
{
	fch_spi_set_read_mode(SPI_READ_MODE_NORMAL33M);
	fch_spi_set_spi100(SPI_SPEED_16M, SPI_SPEED_16M, SPI_SPEED_16M, SPI_SPEED_16M);
}

void fch_spi_config_modes(void)
{
	if (CONFIG(EM100))
		fch_spi_config_em100_modes();
	else
		fch_spi_config_mb_modes();
}

void fch_spi_early_init(void)
{
	lpc_set_spibase(SPI_BASE_ADDRESS);
	lpc_enable_spi_rom(SPI_ROM_ENABLE);
	lpc_enable_spi_prefetch();
	fch_spi_disable_4dw_burst();
	fch_spi_config_modes();
}
