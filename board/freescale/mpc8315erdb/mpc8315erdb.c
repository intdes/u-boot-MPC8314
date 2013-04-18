/*
 * Copyright (C) 2007 Freescale Semiconductor, Inc.
 *
 * Author: Scott Wood <scottwood@freescale.com>
 *         Dave Liu <daveliu@freescale.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <hwconfig.h>
#include <i2c.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <pci.h>
#include <mpc83xx.h>
#include <netdev.h>
#include <asm/io.h>
#include <ns16550.h>
#include <nand.h>

DECLARE_GLOBAL_DATA_PTR;

int board_early_init_f(void)
{
	volatile immap_t *im = (immap_t *)CONFIG_SYS_IMMR;

	if (im->pmc.pmccr1 & PMCCR1_POWER_OFF)
		gd->flags |= GD_FLG_SILENT;

	return 0;
}

#ifndef CONFIG_NAND_SPL

static u8 read_board_info(void)
{
	u8 val8;
	i2c_set_bus_num(0);

	if (i2c_read(CONFIG_SYS_I2C_PCF8574A_ADDR, 0, 0, &val8, 1) == 0)
		return val8;
	else
		return 0;
}

#define PCA9534_ADDR		0x27

#define OUTPUT_REGISTER		0x01
#define CONFIG_REGISTER		0x03

#define MODE_MASK			0xE0
#define RS232_SEL			0x80

#ifdef CONFIG_SYS_I2C_BOARD_LATE_INIT
/*
 *  WriteI2CRegister -
 *
 *	Writes to a register on the PCA9534A
 */
static u8 WriteI2CRegister( u8 iRegister, u8 iValue )
{
	u8 val8;
	i2c_set_bus_num(0);

	return ( i2c_write(PCA9534_ADDR, iRegister, 1, &iValue, 1 ) );
}

/*
 *  SetupRS232Output -
 *
 *  Sets up the PCA9534 to use RS232 output.
 */
void i2c_board_late_init( void )
{
	WriteI2CRegister( CONFIG_REGISTER, ~MODE_MASK );
	WriteI2CRegister( OUTPUT_REGISTER, RS232_SEL );
	return 0;
}
#endif	//CONFIG_SYS_I2C_BOARD_LATE_INIT

int checkboard(void)
{
	static const char * const rev_str[] = {
		"0.0",
		"0.1",
		"1.0",
		"1.1",
		"<unknown>",
	};
	u8 info;
	int i;

	info = read_board_info();
	i = (!info) ? 4: info & 0x03;

	printf("Board: Freescale MPC8315ERDB Rev %s\n", rev_str[i]);

	return 0;
}

static struct pci_region pci_regions[] = {
	{
		bus_start: CONFIG_SYS_PCI_MEM_BASE,
		phys_start: CONFIG_SYS_PCI_MEM_PHYS,
		size: CONFIG_SYS_PCI_MEM_SIZE,
		flags: PCI_REGION_MEM | PCI_REGION_PREFETCH
	},
	{
		bus_start: CONFIG_SYS_PCI_MMIO_BASE,
		phys_start: CONFIG_SYS_PCI_MMIO_PHYS,
		size: CONFIG_SYS_PCI_MMIO_SIZE,
		flags: PCI_REGION_MEM
	},
	{
		bus_start: CONFIG_SYS_PCI_IO_BASE,
		phys_start: CONFIG_SYS_PCI_IO_PHYS,
		size: CONFIG_SYS_PCI_IO_SIZE,
		flags: PCI_REGION_IO
	}
};

static struct pci_region pcie_regions_0[] = {
	{
		.bus_start = CONFIG_SYS_PCIE1_MEM_BASE,
		.phys_start = CONFIG_SYS_PCIE1_MEM_PHYS,
		.size = CONFIG_SYS_PCIE1_MEM_SIZE,
		.flags = PCI_REGION_MEM,
	},
	{
		.bus_start = CONFIG_SYS_PCIE1_IO_BASE,
		.phys_start = CONFIG_SYS_PCIE1_IO_PHYS,
		.size = CONFIG_SYS_PCIE1_IO_SIZE,
		.flags = PCI_REGION_IO,
	},
};

static struct pci_region pcie_regions_1[] = {
	{
		.bus_start = CONFIG_SYS_PCIE2_MEM_BASE,
		.phys_start = CONFIG_SYS_PCIE2_MEM_PHYS,
		.size = CONFIG_SYS_PCIE2_MEM_SIZE,
		.flags = PCI_REGION_MEM,
	},
	{
		.bus_start = CONFIG_SYS_PCIE2_IO_BASE,
		.phys_start = CONFIG_SYS_PCIE2_IO_PHYS,
		.size = CONFIG_SYS_PCIE2_IO_SIZE,
		.flags = PCI_REGION_IO,
	},
};

void pci_init_board(void)
{
	volatile immap_t *immr = (volatile immap_t *)CONFIG_SYS_IMMR;
	volatile sysconf83xx_t *sysconf = &immr->sysconf;
	volatile clk83xx_t *clk = (volatile clk83xx_t *)&immr->clk;
	volatile law83xx_t *pci_law = immr->sysconf.pcilaw;
	volatile law83xx_t *pcie_law = sysconf->pcielaw;
	struct pci_region *reg[] = { pci_regions };
	struct pci_region *pcie_reg[] = { pcie_regions_0, pcie_regions_1, };
	int warmboot;

	/* Enable all 3 PCI_CLK_OUTPUTs. */
	clk->occr |= 0xe0000000;

	/*
	 * Configure PCI Local Access Windows
	 */
	pci_law[0].bar = CONFIG_SYS_PCI_MEM_PHYS & LAWBAR_BAR;
	pci_law[0].ar = LBLAWAR_EN | LBLAWAR_512MB;

	pci_law[1].bar = CONFIG_SYS_PCI_IO_PHYS & LAWBAR_BAR;
	pci_law[1].ar = LBLAWAR_EN | LBLAWAR_1MB;

	warmboot = gd->bd->bi_bootflags & BOOTFLAG_WARM;
	warmboot |= immr->pmc.pmccr1 & PMCCR1_POWER_OFF;

	mpc83xx_pci_init(1, reg, warmboot);

	/* Configure the clock for PCIE controller */
	clrsetbits_be32(&clk->sccr, SCCR_PCIEXP1CM | SCCR_PCIEXP2CM,
				    SCCR_PCIEXP1CM_1 | SCCR_PCIEXP2CM_1);

	/* Deassert the resets in the control register */
	out_be32(&sysconf->pecr1, 0xE0008000);
	out_be32(&sysconf->pecr2, 0xE0008000);
	udelay(2000);

	/* Configure PCI Express Local Access Windows */
	out_be32(&pcie_law[0].bar, CONFIG_SYS_PCIE1_BASE & LAWBAR_BAR);
	out_be32(&pcie_law[0].ar, LBLAWAR_EN | LBLAWAR_512MB);

	out_be32(&pcie_law[1].bar, CONFIG_SYS_PCIE2_BASE & LAWBAR_BAR);
	out_be32(&pcie_law[1].ar, LBLAWAR_EN | LBLAWAR_512MB);

	mpc83xx_pcie_init(2, pcie_reg, warmboot);
}

/* JPW added for legacy flash */
#if defined(CONFIG_FLASH_CFI_LEGACY)
#include <flash.h>
ulong board_flash_get_legacy (ulong base, int banknum, flash_info_t *info )
{
/*	int sect[] = CONFIG_SYS_ATMEL_SECT;
	int sectsz[] = CONFIG_SYS_ATMEL_SECTSZ;
	int i, j, k;*/

	if (base != CONFIG_SYS_FLASH_BASE)
		return 0;
	
/*	info->flash_id		= 0x01000000;*/
	info->portwidth		= FLASH_CFI_8BIT;
	info->chipwidth		= FLASH_CFI_8BIT;
	info->interface		= FLASH_CFI_X8;
/*	info->buffer_size	= 1;
	info->erase_blk_tout	= 16384;
	info->write_tout	= 2;
	info->buffer_write_tout	= 5;
	info->vendor		= CFI_CMDSET_AMD_LEGACY;
	info->cmd_reset		= 0x00F0;
	info->legacy_unlock	= 0;
	info->manufacturer_id	= (u16) ATM_MANUFACT;
	info->device_id		= ATM_ID_LV040;
	info->device_id2	= 0;

	info->ext_addr		= 0;
	info->cfi_version	= 0x3133;
	info->cfi_offset	= 0x0000;
	info->addr_unlock1	= 0x00000555;
	info->addr_unlock2	= 0x000002AA;
	info->name		= "CFI conformant";
	
	info->size		= 0;
	info->sector_count	= CONFIG_SYS_ATMEL_TOTALSECT;
	info->start[0] = base;
	for (k = 0, i = 0; i < CONFIG_SYS_ATMEL_REGION; i++)
	{
		info->size += sect[i] * sectsz[i];

		for (j = 0; j < sect[i]; j++, k++)
		{
			info->start[k+ 1] = info->start[k] + sectsz[i];
			info->protect[k] = 0;
		}
	}*/

	return 1;
}

#endif

#if defined(CONFIG_OF_BOARD_SETUP)
void fdt_tsec1_fixup(void *fdt, bd_t *bd)
{
	const char disabled[] = "disabled";
	const char *path;
	int ret;

	if (hwconfig_arg_cmp("board_type", "tsec1")) {
		return;
	} else if (!hwconfig_arg_cmp("board_type", "ulpi")) {
		printf("NOTICE: No or unknown board_type hwconfig specified.\n"
		       "        Assuming board with TSEC1.\n");
		return;
	}

	ret = fdt_path_offset(fdt, "/aliases");
	if (ret < 0) {
		printf("WARNING: can't find /aliases node\n");
		return;
	}

	path = fdt_getprop(fdt, ret, "ethernet0", NULL);
	if (!path) {
		printf("WARNING: can't find ethernet0 alias\n");
		return;
	}

	do_fixup_by_path(fdt, path, "status", disabled, sizeof(disabled), 1);
}

void ft_board_setup(void *blob, bd_t *bd)
{
	ft_cpu_setup(blob, bd);
#ifdef CONFIG_PCI
	ft_pci_setup(blob, bd);
#endif
	fdt_fixup_dr_usb(blob, bd);
	fdt_tsec1_fixup(blob, bd);
}
#endif

int board_eth_init(bd_t *bis)
{
	cpu_eth_init(bis);	/* Initialize TSECs first */
	return pci_eth_init(bis);
}

#else /* CONFIG_NAND_SPL */

int checkboard(void)
{
	puts("Board: Freescale MPC8315ERDB\n");
	return 0;
}

void board_init_f(ulong bootflag)
{
	board_early_init_f();
	NS16550_init((NS16550_t)(CONFIG_SYS_IMMR + 0x4500),
		     CONFIG_SYS_NS16550_CLK / 16 / CONFIG_BAUDRATE);
	puts("NAND boot... ");
	init_timebase();
	initdram(0);
	relocate_code(CONFIG_SYS_NAND_U_BOOT_RELOC + 0x10000, (gd_t *)gd,
		      CONFIG_SYS_NAND_U_BOOT_RELOC);
}

void board_init_r(gd_t *gd, ulong dest_addr)
{
	nand_boot();
}

void putc(char c)
{
	if (gd->flags & GD_FLG_SILENT)
		return;

	if (c == '\n')
		NS16550_putc((NS16550_t)(CONFIG_SYS_IMMR + 0x4500), '\r');

	NS16550_putc((NS16550_t)(CONFIG_SYS_IMMR + 0x4500), c);
}

#endif /* CONFIG_NAND_SPL */


#define TOBOOL(x)	(((x)!=0)?1:0)
#define NOT(x)		(((x)==0)?1:0)
#define BIT(x)		(1<<(x))
#define BUTTON_INPUT	0x01000000

#ifdef NEXIS_CONSOLE

int console_enabled( void )
{
	volatile immap_t *immr = (volatile immap_t *)CONFIG_SYS_IMMR;

	return ( NOT(TOBOOL(( immr->gpio[0].dat & BUTTON_INPUT ) != 0 )) );
}
#endif

