/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for LPD driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_LPD_H__
#define ___SAMSUNG_LPD_H__


/* TEST DRAM : 0 : default, 1: Test DMA
 * for "1: DMA test", need to modify DT
 * ex) at exynos9110-rmem.dtsi
 * lpd_rmem: lpd_rmem {
 *	compatible = "shared-dma-pool";
 *	reg = <0x0 0x9D000000 0x80000>;
 *	no-map;
 * };
 */

/*#define TEST_DRAM_ACCESS*/
/*#define TEST_M55_RESET_RELEASE*/

/* DPU_WAIT : wait for dpu off, temporal until pm sequene update */
/*#define TEST_M55_DPU_WAIT*/

/* NOTI_AP_STATE: inform suspend/resume to CM55 for display, tests */
/*#define TEST_M55_NOTI_AP_STATE*/

/* SRAM Power Gating */
#define LPD_SRAM_POWER_GATING
#define VERIFY_SRAM

#include <linux/platform_device.h>
#include <linux/ktime.h>

#include "lpd_dbg.h"
#include "lpd_fw.h"
#include "lpd_mem.h"
#include "lpd_noti.h"
#include "lpd_dev.h"


#define NULL_CHECK(val)							\
	do {								\
		if (IS_ERR_OR_NULL(val)) {				\
			lpd_err("invalid argument: %d\n", __LINE__);	\
			return -EINVAL;					\
		}							\
	} while (0)

#define ERR_CHECK(val)							\
	do {								\
		if (val < 0) {						\
			lpd_err("invalid argument: %d\n", __LINE__);	\
			return -EINVAL;					\
		}							\
	} while (0)

#define META_VERSION_MAJOR(version)	(version >> 16 & 0xff)
#define META_VERSION_MINOR(version)	(version >> 8 & 0xff)
#define META_VERSION_REVISION(version)	(version & 0xff)

/* operation state of lpd */
enum lpd_state {
	LPD_STATE_INIT,
	LPD_STATE_PREPARE,
	LPD_STATE_PM_ACQUIRE,
	LPD_STATE_START_BEGIN,
	LPD_STATE_START_END,
	LPD_STATE_STOP_BEGIN,
	LPD_STATE_STOP_END,
	LPD_STATE_PM_RELEASE,
};
/* for meta update */
enum update_req_state {
	UPDATE_REQ_DONE = 0,
	UPDATE_REQ_PREPARED,
};

enum lpd_fw_desc_id {
	SENSOR_DATA_ID = 0,
	META_DATA_ID,
	DPU_DATA_ID,
	CMD_DATA_ID,
	COMP_DATA_ID,
	TIME_DATA_ID,
	DEBUG_DATA_ID,
};


struct lpd_device {
	volatile enum lpd_state state;

	struct device *dev;

	struct lpd_chardev_info chardev;
	struct lpd_reserved_memory rmem;
	struct lpd_sram_memory sram;

	struct lpd_meta_data lpd_meta;

	struct lpd_notifier nb;

	struct lpd_debug_info debug_info;

	int enable;

	struct mutex status_lock;
	struct mutex op_lock;

	struct lpd_fw_logbuf *sram_logbuf;

	enum update_req_state meta_update_req;

	int dpu_state;

	struct lpd_brightness_meta br_meta;

	ktime_t ktime_lpd_start;
};

struct lpd_ioctl_desc {
	unsigned int cmd;
	int (*func)(struct lpd_device *lpd, void __user *argp);
	const char *desc;
};

static inline bool IS_LPD_START_STATE(struct lpd_device *lpd)
{
	if (lpd == NULL)
		return 0;

	return lpd->state == LPD_STATE_START_BEGIN || lpd->state == LPD_STATE_START_END;
}

static inline bool IS_LPD_STOP_STATE(struct lpd_device *lpd)
{
	if (lpd == NULL)
		return 0;

	return lpd->state == LPD_STATE_STOP_BEGIN || lpd->state == LPD_STATE_STOP_END;
}

// LPD can be on until STOP_END
static inline bool IS_LPD_ON_STATE(struct lpd_device *lpd)
{
	if (lpd == NULL)
		return 0;

	return IS_LPD_START_STATE(lpd) || lpd->state == LPD_STATE_STOP_BEGIN;
}

static inline bool IS_LPD_PM_ACQUIRE(struct lpd_device *lpd)
{
	if (lpd == NULL)
		return 0;

	return lpd->state == LPD_STATE_PM_ACQUIRE;
}


static inline bool IS_LPD_SRAM_AVAILABLE(struct lpd_device *lpd)
{
	if (lpd == NULL)
		return false;

	if (!(lpd->sram.state & LPD_SRAM_LPD_ON)) {
		lpd_err("%s : Not Ready SRAM\n", __func__);
		return false;
	}

	if (!(lpd->sram.state & LPD_SRAM_CHUB_ON)) {
		lpd_err("%s : Not Ready CHUB\n", __func__);
		return false;
	}

	if (lpd->sram.state & LPD_SRAM_CHUB_PG) {
		lpd_err("%s : Invalid SRAM state\n", __func__);
		return false;
	}

	return true;
}

int lpd_pm_acquire(struct lpd_device *lpd);
int lpd_start(struct lpd_device *lpd, enum lpd_notifier_steps step);
int lpd_stop(struct lpd_device *lpd, enum lpd_notifier_steps step);
int lpd_pm_release(struct lpd_device *lpd);
/* lpd_mem */
int lpd_sram_acquire(struct lpd_device *lpd, struct platform_device *pdev);
int lpd_sram_release(struct lpd_device *lpd);
int lpd_sram_read(struct lpd_device *lpd, void *buffer, size_t size, int offset);
int lpd_sram_write(struct lpd_device *lpd, void *buffer, size_t size, int offset);
#if defined(VERIFY_SRAM)
int lpd_sram_verify(struct lpd_device *lpd, void *buf, int off_d, int off_s, int size);
#else
static line int lpd_sram_verify(struct lpd_device *lpd, void *buf, int off_d, int off_s, int size) { return 0; }
#endif
int lpd_sram_fw_info(struct lpd_device *lpd);
int lpd_sram_load_firmware(struct lpd_device *lpd);
int lpd_sram_fw_on(struct lpd_device *lpd);
int lpd_rmem_acquire(struct lpd_device *lpd, struct platform_device *pdev);
int lpd_rmem_release(struct lpd_device *lpd);
int lpd_rmem_read(struct lpd_device *lpd, char **buffer, size_t size, int offset);
int lpd_rmem_to_sram(struct lpd_device *lpd, int off_d, int off_s, int size);
int get_meta_data_offset(struct lpd_device *lpd, enum lpd_fw_desc_id desc_id);

/* MCD */
int lpd_clear_meta_data(struct lpd_device *lpd);
int lpd_set_state(struct lpd_device *lpd, enum lpd_state state);
int lpd_update_sensor_data(struct lpd_device *lpd, struct lpd_sensor_meta *sensor_meta);
int lpd_update_composer_data(struct lpd_device *lpd, struct lpd_composer_meta *composer_meta);
int lpd_update_dpu_data(struct lpd_device *lpd, struct lpd_dpu_meta *dpu_meta);
int lpd_update_cmd_data(struct lpd_device *lpd);
int lpd_update_comp_data(struct lpd_device *lpd);
int lpd_init_logbuf(struct lpd_device *lpd);
void lpd_print_meta_info(struct lpd_device *lpd);

/* lpd_dev */
int lpd_dev_init(struct lpd_device *lpd);
int lpd_dev_uninit(struct lpd_device *lpd);
/* lpd_dbg */
int lpd_dbg_init(struct lpd_device *lpd);
int lpd_dbg_uninit(struct lpd_device *lpd);
int lpd_logbuf_outprint(struct lpd_device *lpd);
int lpd_read_cm55_fault_status(struct lpd_device *lpd);
#if defined(TEST_M55_NOTI_AP_STATE)
int lpd_logbuf_noti_apstate(struct lpd_device *lpd, u8 apState);
#else
static inline int lpd_logbuf_noti_apstate(struct lpd_device *lpd, u8 apState) { return 0; }
#endif
/* lpd_noti */
int lpd_notifier_init(struct lpd_device *lpd);
int lpd_notifier_uninit(struct lpd_device *lpd);

#endif
