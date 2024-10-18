/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * memory interface for SRAM and DRAM reserved area
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_LPD_MEM_H__
#define ___SAMSUNG_LPD_MEM_H__
#include <linux/types.h>
#include <asm/io.h>

#if defined(TEST_M55_RESET_RELEASE)
#include <linux/delay.h>
#define SYSREG_CHUB_BASE 0x10C40000
#define USER_REG0 (0x0000)
#define USER_REG1 (0x0004)
#define USER_REG2 (0x0008)
#define USER_REG3 (0x000C)
#define USER_REG4 (0x0010)
#define BOOTADDR_S (0x1000)
#define BOOTADDR_NS (0x1004)
#define YAMIN_INST_CTRL (0x1008)
#define YAMIN_INST_FAULT0 (0x100C)
#define YAMIN_INST_FAULT1 (0x1010)
#define INTREQ_CM4_TO (0x1014)
#define INTREQ_YAMIN_TO (0x1018)
#define YAMIN_INST_CUR (0x1200)

#define PMU_BASE 0x12860000
#define CM55_CFG (0x3500)
#define CM55_STATUS (0x3504)
#define CM55_STATES (0x3508)
#define CM55_OPTION (0x350C)
#define CM55_CTRL (0x3510)
#endif


struct lpd_comp_memory {
	phys_addr_t image_base;
	size_t image_size;

	phys_addr_t lut_base;
	size_t lut_size;

	phys_addr_t canvas_base;
	size_t canvas_size;
};

struct lpd_cmd_memory {
	phys_addr_t base;
	size_t size;
};

struct lpd_reserved_memory {
	bool reserved;
	phys_addr_t base;
	size_t size;
/* lpd_cmd base address will be copied to lpd cmd meta data when before start lpd */

	struct lpd_cmd_memory cmd_mem;
	struct lpd_comp_memory comp_mem;

#if defined(TEST_DRAM_ACCESS) /* Uses DMA */
	char __iomem *io_base;
	dma_addr_t dma;
#endif
};

enum lpd_sram_state {
	LPD_SRAM_INVALID 	= 0,
	LPD_SRAM_LPD_ON		= 1 << 0, // LPD region power on
	LPD_SRAM_CHUB_ON	= 1 << 1, // CHUB boot-up
	LPD_SRAM_CHUB_PG	= 1 << 2, // CHUB power-gating
};

struct lpd_sram_memory {
	u8	state;
	phys_addr_t base;
	size_t 		size;
	void __iomem *io_base;
	struct lpd_sram_offset {
		u32	code;
		u32	ram;
		u32	ipc;
		u32	logbuf;
		u32	desc;
		u32	img;
		u32	dump;
	} off;	
};

#endif
