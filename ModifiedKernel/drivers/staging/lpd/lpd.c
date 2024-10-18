/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for LPD driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "lpd.h"

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-lpd.h>

#include "lpd_mem.h"

struct lpd_device *lpd_drvdata;

static char *lpd_state_names[] = {
	"INIT",
	"PREPARE",
	"PM_ACQUIRE",
	"START_BEGIN",
	"START_END",
	"STOP_BEGIN",
	"STOP_END",
	"PM_RELEASE",
};

static int lpd_parse_dt(struct lpd_device *lpd, struct platform_device *pdev)
{
	int ret;
	struct device *dev;

	NULL_CHECK(lpd);
	dev = lpd->dev;

	NULL_CHECK(dev);

	if (!dev->of_node) {
		lpd_err("driver doesn't support non-dt\n");
		return -ENODEV;
	}

	ret = lpd_rmem_acquire(lpd, pdev);
	if (ret < 0) {
		lpd_err("driver failed to get reserved memory info\n");
		return -EINVAL;
	}

	ret = lpd_sram_acquire(lpd, pdev);
	if (ret < 0) {
		lpd_err("driver failed to get sram info\n");
		return -EINVAL;
	}

	return 0;
}

int lpd_set_state(struct lpd_device *lpd, enum lpd_state state)
{
	NULL_CHECK(lpd);
	mutex_lock(&lpd->status_lock);

	lpd_dbg("state changed %s -> %s\n", lpd_state_names[lpd->state], lpd_state_names[state]);
	lpd->state = state;

	mutex_unlock(&lpd->status_lock);
	return 0;
}


#ifdef USE_LEGACY_S_LSI_CODE //S.LSI Original Codes
// move sram copy to lpd_start for power save
int lpd_prepare_begin(struct lpd_device *lpd)
{
	int i = 0, idx, offset_s = 0, ret;
	int addr;
	size_t size;
	lpd_meta_info *metadata;
	struct lpd_meta_data *lpd_meta;

	NULL_CHECK(lpd);
	lpd_meta = &lpd->lpd_meta;
	NULL_CHECK(lpd_meta);

	metadata = &lpd_meta->composer_meta;
	NULL_CHECK(metadata);

	// download LPW fw on every lpd start
	ret = lpd_sram_load_firmware(lpd);
	ERR_CHECK(ret);

#if defined(TEST_M55_RESET_RELEASE)
	ret = lpd_sram_fw_on(lpd); // Just for test purpose
	ERR_CHECK(ret);
#endif
	ret = lpd_sram_fw_info(lpd);
	ERR_CHECK(ret);

	// copy image data to sram
	offset_s = lpd->sram.off.img;
	for (idx = 0; idx < metadata->resource_cnt; idx++) {
		if (metadata->resources[idx].buf_type == LPD_RSC_BUF_TYPE_SRAM) {
			lpd_dbg("copy image resource to sram\n");
			addr = metadata->resources[idx].addr; // get dram address
			metadata->resources[idx].addr = offset_s; // save sram address
			size = metadata->resources[idx].buf_size;
			lpd_dbg(" %d.%d: addr 0x%x-> 0x%x size 0x%x\n", idx, i, addr, offset_s, size);
			ret = lpd_rmem_to_sram(lpd, addr, offset_s, size);
			ERR_CHECK(ret);
			offset_s += size;
		}
	}

	// copy metadata to sram
	lpd_dbg("copy metadata to sram\n");
	lpd_dbg("addr 0x%x size 0x%x\n", lpd->sram.off.desc, sizeof(struct lpd_meta_data));
	ret = lpd_sram_write(lpd, desc, sizeof(struct lpd_fw_img_desc), lpd->sram.off.desc);
	ERR_CHECK(ret);
	lpd_sram_verify(lpd, desc, 0, lpd->sram.off.desc, sizeof(struct lpd_meta_data));

	return 0;
}

#endif


static int write_sram_lpd_meta(struct lpd_device *lpd)
{
	int ret;
	struct lpd_meta_data *lpd_meta;

	if (lpd == NULL) {
		lpd_err("null lpd");
		return -EINVAL;
	}

	lpd_meta = &lpd->lpd_meta;

	ret = lpd_clear_meta_data(lpd);
	if (ret < 0) {
		lpd_err("failed to clear meta data: %d\n", ret);
		return -EINVAL;
	}

	ret = lpd_update_composer_data(lpd, &lpd_meta->composer_meta);
	if (ret != sizeof(struct lpd_composer_meta)) {
		lpd_err("failed to update composer data: %d\n", ret);
		return -EINVAL;
	}

	ret = lpd_update_sensor_data(lpd, &lpd_meta->sensor_meta);
	if (ret != sizeof(struct lpd_sensor_meta)) {
		lpd_err("failed to update sensor data: %d\n", ret);
		return -EINVAL;
	}

	ret = lpd_update_dpu_data(lpd, &lpd_meta->dpu_meta);
	if (ret != sizeof(struct lpd_dpu_meta)) {
		lpd_err("failed to update dpu data: %d\n", ret);
		return -EINVAL;
	}

	ret = lpd_update_cmd_data(lpd);
	if (ret != sizeof(struct lpd_cmd_meta)) {
		lpd_err("failed to update cmd data: %d\n", ret);
		return -EINVAL;
	}

	ret = lpd_update_comp_data(lpd);
	if (ret != sizeof(struct lpd_comp_meta)) {
		lpd_err("failed to update cmd data: %d\n", ret);
		return -EINVAL;
	}

	return ret;
}


static int lpd_clear_dram(struct lpd_device *lpd)
{
	void *img_buf;
	struct lpd_comp_memory *comp_mem;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}
	comp_mem = &lpd->rmem.comp_mem;
	if (comp_mem->image_size == 0) {
		lpd_err("image base is null\n");
		return 0;
	}

	img_buf = (void *)phys_to_virt(comp_mem->image_base);
	if (img_buf == NULL) {
		lpd_err("null\n");
		return -1;
	}

	memset(img_buf, 0x00, comp_mem->image_size);

	return 0;
}

int lpd_prepare_begin(struct lpd_device *lpd)
{
	int ret;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}
	lpd->ktime_lpd_start = ktime_get();
	ret = lpd_sram_load_firmware(lpd);
	if (ret) {
		lpd_err("failed to load firmware\n");
		return ret;
	}

#if defined(TEST_M55_RESET_RELEASE)
	ret = lpd_sram_fw_on(lpd);
	ERR_CHECK(ret);
#endif
	ret = lpd_sram_fw_info(lpd);
	ERR_CHECK(ret);

	ret = write_sram_lpd_meta(lpd);
	if (ret < 0) {
		lpd_err("failed to write lpd meta to sram\n");
		return ret;
	}
	ret = lpd_clear_dram(lpd);
	if (ret < 0) {
		lpd_err("failed to clear dram\n");
		return ret;
	}

	ret = lpd_init_logbuf(lpd);
	if (ret < 0) {
		lpd_err("failed to init log buf: %d\n", ret);
		return ret;
	}

	return 0;

}


int get_meta_data_offset(struct lpd_device *lpd, enum lpd_fw_desc_id desc_id)
{
	int offset;

	NULL_CHECK(lpd);

	offset = lpd->sram.off.desc;

	switch (desc_id) {
	case SENSOR_DATA_ID:
		offset += 0;
		break;

	case META_DATA_ID:
		offset += sizeof(struct lpd_sensor_meta);
		break;

	case DPU_DATA_ID:
		offset += (sizeof(struct lpd_sensor_meta) + sizeof(struct lpd_composer_meta));
		break;

	case CMD_DATA_ID:
		offset += (sizeof(struct lpd_sensor_meta) + sizeof(struct lpd_composer_meta)
			+ sizeof(struct lpd_dpu_meta));
		break;

	case COMP_DATA_ID:
		offset += (sizeof(struct lpd_sensor_meta) + sizeof(struct lpd_composer_meta)
			+ sizeof(struct lpd_dpu_meta) + sizeof(struct lpd_cmd_meta));
		break;

	case TIME_DATA_ID:
		offset += (sizeof(struct lpd_sensor_meta) + sizeof(struct lpd_composer_meta)
			+ sizeof(struct lpd_dpu_meta) + sizeof(struct lpd_cmd_meta)
			+ sizeof(struct lpd_comp_meta));
		break;

	case DEBUG_DATA_ID:
		offset += (sizeof(struct lpd_sensor_meta) + sizeof(struct lpd_composer_meta)
			+ sizeof(struct lpd_dpu_meta) + sizeof(struct lpd_cmd_meta)
			+ sizeof(struct lpd_comp_meta) + sizeof(struct lpd_time_meta));
		break;

	default:
		lpd_err("ERR:%s undefined desc_id: %d\n", __func__, desc_id);
		return -1;
	}

	return offset;
}


static int check_resource_data(struct lpd_resource_info *rsc)
{
	if (rsc == NULL) {
		lpd_err("null rsc\n");
		return -EINVAL;
	}

	if ((rsc->buf_type < 0) || (rsc->buf_type >= LPD_RSC_BUF_TYPE_MAX)) {
		lpd_err("wrong buffer type: %d\n", rsc->buf_type);
		return -EINVAL;
	}

	if ((rsc->rsc_type < 0) || (rsc->rsc_type >= LPD_RSC_TYPE_MAX)) {
		lpd_err("wrong resource type: %d\n", rsc->rsc_type);
		return -EINVAL;
	}
	return 0;
}


static int check_layout_data(struct lpd_layout_info *layout)
{
	if (layout == NULL) {
		lpd_err("null layout\n");
		return -EINVAL;
	}

	if ((layout->layout_type < 0) || (layout->layout_type >= LPD_LAYOUT_TYPE_MAX)) {
		lpd_err("wrong layout type: %d\n", __func__, layout->layout_type);
		return -EINVAL;
	}

	return 0;
}


static char *rsc_buf_str[] = {
	"SRAM", "DRAM"
};

static char *rsc_type_str[] = {
	"NONE", "BG", "CANVAS",
	"HANDS_HOUR", "HANDS_HOUR2",
	"HANDS_MIN", "HANDS_MIN2",
	"HANDS_SEC",
	"HANDS_PIVOT",
	"COLON1", "COLON2",
	"ICON1", "ICON2", "SYS_ICON",
	"FONT1", "FONT2", "FONT3", "FONT4", "FONT5", "FONT6",
	"AMPM1", "AMPM2",
};

static char *layout_type_str[] = {
	"NONE",
	"BG", "CANVAS", "FG",
	"ANALOG", "DIGITAL",
	"ICON1", "ICON2", "SYS_ICON",
	"NUM1", "NUM2", "NUM3", "NUM4", "NUM5", "NUM6",
	"CLOCK",
	"D_NUM1", "D_NUM2", "D_NUM3", "D_NUM4"
};


static void print_rsc_info(int idx, struct lpd_resource_info *rsc)
{
	size_t buf_size = ARRAY_SIZE(rsc_buf_str);
	size_t rsc_size = ARRAY_SIZE(rsc_type_str);

	if (rsc == NULL) {
		lpd_err("null rsc\n");
		return;
	}

	if ((buf_size > LPD_RSC_BUF_TYPE_MAX) || (rsc_size > LPD_RSC_TYPE_MAX)) {
		lpd_err("Exceed array size: buf: %d(max:%d), rsc: %d(max:%d), check array size\n",
			buf_size, LPD_RSC_BUF_TYPE_MAX, rsc_size, LPD_RSC_TYPE_MAX);
		return;
	}

	lpd_info("rsc info[%2d], type: %s(@%s), %d x %d\n",
		idx, rsc_type_str[rsc->rsc_type], rsc_buf_str[rsc->buf_type],
		rsc->width, rsc->height);
}

static void print_layout_info(int idx, struct lpd_layout_info *layout)
{
	size_t layout_size = ARRAY_SIZE(layout_type_str);

	if (layout == NULL) {
		lpd_err("null layout_size\n");
		return;
	}

	if (layout_size > LPD_LAYOUT_TYPE_MAX) {
		lpd_err("Exceed layout array size: %d(max: %d), check array size\n",
			layout_size, LPD_LAYOUT_TYPE_MAX);
		return;
	}

	lpd_info("layout info[%2d]: %s\n", idx, layout_type_str[layout->layout_type]);
}


static int check_meta_data(struct lpd_device *lpd, struct lpd_composer_meta *composer_meta)
{
	int i;
	int ret = 0;
	struct lpd_resource_info *rsc;
	struct lpd_layout_info *layout;

	lpd_info("meta_data->enable: %d\n", composer_meta->enable);

	if (composer_meta->enable == false)
		return 0;

	if ((composer_meta->resource_cnt <= 0) || (composer_meta->layout_cnt <= 0)) {
		lpd_err("invalid resource cnt: %d, layout cnt: %d\n",
			composer_meta->resource_cnt, composer_meta->layout_cnt);
		return -EINVAL;
	}

	lpd_info("rsc cnt: %d, layout cnt: %d\n", composer_meta->resource_cnt, composer_meta->layout_cnt);

	for (i = 0; i < composer_meta->resource_cnt; i++) {
		rsc = &composer_meta->resources[i];
		ret = check_resource_data(rsc);
		if (ret) {
			lpd_err("invalid resoruce data index: %d\n", i);
			return ret;
		}
		print_rsc_info(i, rsc);
	}

	for (i = 0; i < composer_meta->layout_cnt; i++) {
		layout = &composer_meta->layouts[i];
		ret = check_layout_data(layout);
		if (ret) {
			lpd_err("invalid layout data index: %d\n", i);
			return ret;
		}
		print_layout_info(i, layout);
	}

	return ret;
}


int lpd_update_sensor_data(struct lpd_device *lpd, struct lpd_sensor_meta *sensor_meta)
{
	int i;
	int ret = 0;
	int offset;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	if (sensor_meta == NULL) {
		lpd_err("null sensor meta\n");
		return -EINVAL;
	}

	lpd_dbg("tick: %d\n", sensor_meta->tick);

	if (sensor_meta->tick == 0) {
		lpd_err("invalid tick: %d, make default sensor meta\n", sensor_meta->tick);
		sensor_meta->tick = 60;
		sensor_meta->data_cnt = 0;

		for (i = 0; i < MAX_SENSOR_DATA; i++)
			sensor_meta->id_list[i] = 0;
	}

	lpd_info("br mode: %d\n", sensor_meta->br_mode);
	offset = get_meta_data_offset(lpd, SENSOR_DATA_ID);
	if (offset < 0) {
		lpd_err("ERR:%s: wrong offset: %d\n", __func__, offset);
		return offset;
	}
	lpd_dbg("%s: sram offset 0x%x size 0x%x\n", __func__, offset, sizeof(struct lpd_sensor_meta));

	ret = lpd_sram_write(lpd, sensor_meta, sizeof(struct lpd_sensor_meta), offset);
	if (ret != sizeof(struct lpd_sensor_meta)) {
		lpd_err("failed to write to sram\n");
		return ret;
	}

	return ret;
}


int lpd_update_composer_data(struct lpd_device *lpd, struct lpd_composer_meta *composer_meta)
{
	int ret = 0;
	int idx, offset_s = 0;
	int addr;
	size_t size;
	int offset = 0;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	if (composer_meta == NULL) {
		lpd_err("null composer data\n");
		return -EINVAL;
	}

	ret = check_meta_data(lpd, composer_meta);
	if (ret) {
		lpd_err("ERR:%s: wrong meta data\n", __func__);
		return -EINVAL;
	}

	offset_s = lpd->sram.off.img;
	for (idx = 0; idx < composer_meta->resource_cnt; idx++) {
		if (composer_meta->resources[idx].buf_type == LPD_RSC_BUF_TYPE_SRAM) {
			addr = composer_meta->resources[idx].addr;
			composer_meta->resources[idx].addr = offset_s;
			size = composer_meta->resources[idx].buf_size;
			lpd_dbg(" %d: addr 0x%x-> 0x%x size 0x%x\n", idx, addr, offset_s, size);
			ret = lpd_rmem_to_sram(lpd, addr, offset_s, size);
			ERR_CHECK(ret);
			offset_s += size;
		}
	}
	offset = get_meta_data_offset(lpd, META_DATA_ID);
	if (offset < 0) {
		lpd_err("ERR:%s: wrong offset: %d\n", __func__, offset);
		return offset;
	}

	ret = lpd_sram_write(lpd, composer_meta, sizeof(struct lpd_composer_meta), offset);
	if (ret != sizeof(struct lpd_composer_meta)) {
		lpd_err("failed to write to sram\n");
		return ret;
	}

	lpd_sram_verify(lpd, composer_meta, 0, offset, sizeof(struct lpd_composer_meta));

	return ret;
}


int lpd_clear_meta_data(struct lpd_device *lpd)
{
	int ret, offset;
	struct lpd_meta_data meta_data;

	if (lpd == NULL) {
		lpd_err("null lpd");
		return -EINVAL;
	}

	memset(&meta_data, 0x00, sizeof(struct lpd_meta_data));
	offset = get_meta_data_offset(lpd, SENSOR_DATA_ID);
	if (offset < 0) {
		lpd_err("ERR:wrong offset: %d\n", offset);
		return -EINVAL;
	}

	ret = lpd_sram_write(lpd, &meta_data, sizeof(struct lpd_meta_data), offset);
	if (ret != sizeof(struct lpd_meta_data)) {
		lpd_err("failed to write to sram\n");
		return ret;
	}

	return ret;
}


int lpd_update_dpu_data(struct lpd_device *lpd, struct lpd_dpu_meta *dpu_meta)
{
	int ret;
	int offset = 0;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	if (dpu_meta == NULL) {
		lpd_err("null dpu_meta\n");
		return -EINVAL;
	}

	offset = get_meta_data_offset(lpd, DPU_DATA_ID);
	if (offset < 0) {
		lpd_err("ERR:wrong offset: %d\n", offset);
		return offset;
	}

	ret = lpd_sram_write(lpd, dpu_meta, sizeof(struct lpd_dpu_meta), offset);
	if (ret != sizeof(struct lpd_dpu_meta)) {
		lpd_err("failed to write to sram\n");
		return ret;
	}

	return ret;
}


#if IS_ENABLED(CONFIG_LPD_AUTO_BR)
static int get_panel_br_cmd(struct lpd_reserved_memory *rmem, struct lpd_brightness_meta *br_meta)
{
	int ret, i;
	void *cmd_buf;
	unsigned int *magic_code;
	struct lpd_br_info *br_info;
	struct lpd_panel_cmd panel_cmd;
	struct lpd_cmd_memory *cmd_mem;

	if (rmem == NULL) {
		lpd_err("null reserved memory\n");
		return -EINVAL;
	}

	if (br_meta == NULL) {
		lpd_err("null br meta\n");
		return -EINVAL;
	}

	cmd_mem = &rmem->cmd_mem;
	br_info = &panel_cmd.br_info;

	if (ARRAY_SIZE(br_info->br_list) != ARRAY_SIZE(br_meta->br_info)) {
		lpd_err("mismatch count, check br count! %d:%d\n",
			ARRAY_SIZE(br_info->br_list), ARRAY_SIZE(br_meta->br_info));
		return -1;
	}

	if (cmd_mem->size == 0) {
		lpd_err("cmd size is zero\n");
		return -1;
	}

	cmd_buf = (void *)phys_to_virt(cmd_mem->base);
	if (cmd_buf == NULL) {
		lpd_err("null\n");
		return -1;
	}

	/* set parameter to get panel comamnd */
	panel_cmd.cmd_buf.buf = cmd_buf;
	panel_cmd.cmd_buf.buf_size = LPD_CMD_RMEM_SIZE;

	br_info->br_cnt = br_meta->br_cnt;
	for (i = 0; i < br_meta->br_cnt; i++) {
		br_info->br_list[i] = br_meta->br_info[i].platform_br;
		br_info->nit_list[i] = br_meta->br_info[i].physical_br;
	}

	ret = lpd_config_notifier_call(LPD_CONFIG_BR_CMD, &panel_cmd);

	magic_code = (unsigned int *)cmd_buf;
	lpd_info("lpd cmd magic code1: %x, code2: %x\n",
		magic_code[CMD_SEQ_MAGIC_OFFSET1], magic_code[CMD_SEQ_MAGIC_OFFSET2]);

	if ((magic_code[CMD_SEQ_MAGIC_OFFSET1] != CMD_SEQ_MAGIC_CODE1) ||
		(magic_code[CMD_SEQ_MAGIC_OFFSET2] != CMD_SEQ_MAGIC_CODE2)) {

		lpd_err("invalid magic code\n");
		return -1;
	}

	return ret;

}
#endif



int lpd_update_cmd_data(struct lpd_device *lpd)
{
	int i, ret;
	int offset = 0;
	struct lpd_cmd_meta *cmd_meta;
	struct lpd_cmd_memory *cmd_mem;
	struct lpd_brightness_meta *br_meta;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	br_meta = &lpd->br_meta;
	cmd_mem = &lpd->rmem.cmd_mem;
	cmd_meta = &lpd->lpd_meta.cmd_meta;

	cmd_meta->base = cmd_mem->base;
	cmd_meta->size = cmd_mem->size;

	lpd_info("ARRAY_SIZE META DATA: %d\n", ARRAY_SIZE(cmd_meta->nit_tbl));

	if (br_meta->br_cnt >= ARRAY_SIZE(cmd_meta->nit_tbl)) {
		lpd_err("mismatch count, check br count! %d:%d\n",
			ARRAY_SIZE(cmd_meta->nit_tbl), br_meta->br_cnt);
		return -1;
	}
	lpd_info("br count: %d\n", br_meta->br_cnt);
	for (i = 0; i < br_meta->br_cnt; i++)
		cmd_meta->nit_tbl[i] = br_meta->br_info[i].physical_br;

#if IS_ENABLED(CONFIG_LPD_AUTO_BR)
	if (br_meta->br_cnt) {
		ret = get_panel_br_cmd(&lpd->rmem, br_meta);
		if (ret) {
			lpd_err("failed to get panel brightness command\n");
			return ret;
		}
	}
#endif
	offset = get_meta_data_offset(lpd, CMD_DATA_ID);
	if (offset < 0) {
		lpd_err("ERR: wrong offset: %d\n", offset);
		return offset;
	}

	lpd_info("cmd_buf addr: AP:%x (FW:%x), size: %x\n",
		cmd_meta->base, LPD_FW_DRAM_AP_TO_FW(cmd_meta->base), cmd_meta->size);

	ret = lpd_sram_write(lpd, cmd_meta, sizeof(struct lpd_cmd_meta), offset);
	if (ret != sizeof(struct lpd_cmd_meta)) {
		lpd_err("failed to write to sram\n");
		return ret;
	}

	return ret;
}

int lpd_update_comp_data(struct lpd_device *lpd)
{
	int ret;
	int offset = 0;
	struct lpd_comp_meta *comp_meta;
	struct lpd_comp_memory *comp_mem;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	comp_mem = &lpd->rmem.comp_mem;
	comp_meta = &lpd->lpd_meta.comp_meta;

	comp_meta->image_base = comp_mem->image_base;
	comp_meta->image_size = comp_mem->image_size;

	comp_meta->canvas_base = comp_mem->canvas_base;
	comp_meta->canvas_size = comp_mem->canvas_size;

	comp_meta->lut_base = comp_mem->lut_base;
	comp_meta->lut_size = comp_mem->lut_size;

	offset = get_meta_data_offset(lpd, COMP_DATA_ID);
	if (offset < 0) {
		lpd_err("ERR:wrong offset: %d\n", offset);
		return offset;
	}

	lpd_info("comp info image: addr:%x, size: %x, lut: addr: %x, size: %x, canvas addr: %x, size: %x\n",
		comp_meta->image_base, comp_meta->image_size, comp_meta->canvas_base, comp_meta->canvas_size,
		comp_meta->lut_base, comp_meta->lut_size);

	ret = lpd_sram_write(lpd, comp_meta, sizeof(struct lpd_comp_meta), offset);
	if (ret != sizeof(struct lpd_comp_meta)) {
		lpd_err("failed to write to sram\n");
		return ret;
	}

	return ret;
}



static void print_composer_meta(struct lpd_composer_meta *composer_meta)
{
	if (composer_meta == NULL) {
		lpd_err("null composer meta\n");
		return;
	}

	lpd_info("lpd_enable: %d\n", composer_meta->enable);

}

static void print_sensor_meta(struct lpd_sensor_meta *sensor_meta)
{
	int i;

	if (sensor_meta == NULL) {
		lpd_err("null sensor meta\n");
		return;
	}

	lpd_info("tick       :  %ds\n", sensor_meta->tick);
	lpd_info("data_cnt   :  %d\n", sensor_meta->data_cnt);

	for (i = 0; i < sensor_meta->data_cnt; i++)
		lpd_info("sensor data id [%d]: %d\n", i, sensor_meta->id_list[i]);
}

static void print_dpu_meta(struct lpd_dpu_meta *dpu_meta)
{
	if (dpu_meta == NULL) {
		lpd_err("null dpu meta\n");
		return;
	}

	lpd_info("xres: %d, yres: %d\n", dpu_meta->panel_meta.xres, dpu_meta->panel_meta.yres);
}


void lpd_print_meta_info(struct lpd_device *lpd)
{
	struct lpd_meta_data *lpd_meta;

	if (lpd == NULL) {
		lpd_err("%s lpd is null\n", __func__);
		return;
	}

	lpd_meta = &lpd->lpd_meta;

	print_composer_meta(&lpd_meta->composer_meta);

	print_sensor_meta(&lpd_meta->sensor_meta);

	print_dpu_meta(&lpd_meta->dpu_meta);
}


// wait until dpu is completely off, tempral solution until pmu is ready
static void lpd_wait_for_dpuoff(void)
{
#if defined(TEST_M55_DPU_WAIT)
	u32 status;
	u32 cnt = 10000;
	void __iomem *conf_reg = ioremap(0x12861D00, 0x08);
	void __iomem *status_reg = ioremap(0x12861D04, 0x08);
	void __iomem *states_reg = ioremap(0x12861D08, 0x08);

	// Wait until the DPU  power block is completely turned off
	do {
		status = __raw_readl(status_reg);
		cnt--;
		udelay(10);
	} while (status && cnt);

	if (!cnt)
		lpd_err("DPU power off timeout!, cfg:%x,status:%x, state:%x\n",
				__raw_readl(conf_reg), __raw_readl(status_reg), __raw_readl(states_reg));
	else
		lpd_dbg("DPU power off success cfg:%x,status:%x, state:%x\n",
				__raw_readl(conf_reg), __raw_readl(status_reg), __raw_readl(states_reg));
#endif
}


// START_PRE (pm get sync) -> DPU Disable (pm put sync) -> START
int lpd_pm_acquire(struct lpd_device *lpd)
{
	NULL_CHECK(lpd);

	lpd_info("%s called\n", __func__);

	if (lpd->enable == false) {
		lpd_err("lpd disabled by user\n");
		return -EINVAL;
	}

	if (IS_LPD_START_STATE(lpd)) {
		lpd_err("not allowed as already started\n");
		return -1;
	}

	lpd->debug_info.pm_refcnt++;
	if (lpd->debug_info.pm_refcnt != 1)
		lpd_err("invalid refcnt %d\n", lpd->debug_info.pm_refcnt);

	pm_runtime_get_sync(lpd->dev);

	// LPD SRAM power on
	cal_lpd_sram_power_control(true);


	lpd->sram.state |= LPD_SRAM_LPD_ON;

	lpd_set_state(lpd, LPD_STATE_PM_ACQUIRE);
	return 0;
}

int lpd_start(struct lpd_device *lpd, enum lpd_notifier_steps step)
{
	int ret;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	if (lpd->enable == false) {
		lpd_err("lpd disabled by user\n");
		return -EINVAL;
	}

	switch (step) {
	case LPD_NOTIFIER_STEPS_BEGIN:
		if (lpd->state != LPD_STATE_PM_ACQUIRE) {
			lpd_dbg("require to call PM Acquire first\n");
			return -1;
		}

		ret = lpd_prepare_begin(lpd);
		if (ret != 0) {
			lpd_err("failed to prepare begin lpd\n");
			return ret;
		}

		lpd_wait_for_dpuoff();
		lpd_set_state(lpd, LPD_STATE_START_BEGIN);
		break;

	case LPD_NOTIFIER_STEPS_END:
		if (lpd->state != LPD_STATE_START_BEGIN) {
			lpd_err("invalid lpd state %d\n", lpd->state);
			return -1;
		}

		lpd_set_state(lpd, LPD_STATE_START_END);
		break;
}

	return 0;
}


void lpd_dump_burn_image(struct lpd_device *lpd)
{
	void *image_buf;
	struct lpd_comp_memory *comp_mem;

	comp_mem = &lpd->rmem.comp_mem;

	if (comp_mem->image_size == 0)
		return;

	image_buf = (void *)phys_to_virt(comp_mem->image_base);
	if (image_buf == NULL) {
		lpd_err("null\n");
		return;
	}

	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, image_buf, 480, false);
}


// STOP -> DPU Enable (pm get sync) -> STOP POST (pm put sync)
int lpd_stop(struct lpd_device *lpd, enum lpd_notifier_steps step)
{
	NULL_CHECK(lpd);

	switch(step){
	case LPD_NOTIFIER_STEPS_BEGIN:
		if (!IS_LPD_START_STATE(lpd)) {
			lpd_dbg("not allowed as not started\n");
			return -1;
		}

		lpd_set_state(lpd, LPD_STATE_STOP_BEGIN);
		break;
	case LPD_NOTIFIER_STEPS_END:
		if (lpd->state != LPD_STATE_STOP_BEGIN) {
			lpd_err("require to call STOP first\n");
			return -1;
		}
		lpd_dump_burn_image(lpd);
		lpd_logbuf_outprint(lpd); //print out all the CM55 log after stop
		lpd_read_cm55_fault_status(lpd);
		lpd_set_state(lpd, LPD_STATE_STOP_END);
		break;
	}

	return 0;
}

int lpd_pm_release(struct lpd_device *lpd)
{
	NULL_CHECK(lpd);

	if (!IS_LPD_STOP_STATE(lpd)) {
		lpd_dbg("require to call STOP first\n");
		return -1;
	}

	// LPD SRAM power off
	cal_lpd_sram_power_control(false);

	lpd->sram.state &= ~LPD_SRAM_LPD_ON;

	lpd->debug_info.pm_refcnt--;
	if (lpd->debug_info.pm_refcnt != 0)
		lpd_err("invalid refcnt %d\n", lpd->debug_info.pm_refcnt);
	pm_runtime_put_sync(lpd->dev);

	lpd_set_state(lpd, LPD_STATE_PM_RELEASE);
	return 0;
}

static int lpd_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct lpd_device *lpd;
	int ret = 0;

	NULL_CHECK(pdev);
	dev = &pdev->dev;
	NULL_CHECK(dev);
	lpd = devm_kzalloc(dev, sizeof(struct lpd_device), GFP_KERNEL);
	NULL_CHECK(lpd);
	lpd->dev = dev;

	lpd->sram_logbuf = devm_kzalloc(dev, sizeof(struct lpd_fw_logbuf), GFP_KERNEL);
	if (lpd->sram_logbuf == NULL) {
		lpd_err("%s: failed to alloc sram logbuf\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, lpd);

	/* parse dt and hw init */
	ret = lpd_parse_dt(lpd, pdev);
	if (ret) {
		lpd_err("%s failed to get init hw with ret %d\n", __func__, ret);
		goto err;
	}

	lpd_drvdata = lpd;

	pm_runtime_enable(dev);

	lpd_notifier_init(lpd);

	ret = lpd_dev_init(lpd);
	if (ret) {
		lpd_err("failed to register chrdev\n");
		goto err;
	}

	lpd_dbg_init(lpd);

	mutex_init(&lpd->status_lock);
	mutex_init(&lpd->op_lock);
	lpd_set_state(lpd, LPD_STATE_INIT);
	lpd->enable = false;
	lpd->dpu_state = 0;

	lpd_info("%s is done. ret:%d\n", __func__, ret);
	return 0;

err:
	lpd_err("%s is failed with ret %d\n", __func__, ret);

	lpd_dbg_uninit(lpd);
	lpd_dev_uninit(lpd);

	lpd_notifier_uninit(lpd);

	if(lpd->rmem.reserved)
		lpd_rmem_release(lpd);
	lpd_sram_release(lpd);

	if (lpd->sram_logbuf)
		devm_kfree(dev, lpd->sram_logbuf);

	devm_kfree(dev, lpd);
	return ret;
}

static int lpd_remove(struct platform_device *pdev)
{
	struct lpd_device *lpd = platform_get_drvdata(pdev);
	NULL_CHECK(lpd);

	pm_runtime_disable(lpd->dev);
	lpd_dbg_uninit(lpd);
	lpd_dev_uninit(lpd);

	lpd_notifier_uninit(lpd);

	if(lpd->rmem.reserved)
		lpd_rmem_release(lpd);

	if (lpd->sram_logbuf)
		devm_kfree(lpd->dev, lpd->sram_logbuf);

	return 0;
}

static void lpd_shutdown(struct platform_device *pdev)
{
	struct lpd_device *lpd = platform_get_drvdata(pdev);

	if (lpd == NULL)
		return;

	pm_runtime_disable(lpd->dev);
	lpd_dbg_uninit(lpd);
	lpd_dev_uninit(lpd);

	lpd_notifier_uninit(lpd);

	if(lpd->rmem.reserved)
		lpd_rmem_release(lpd);
}

// call sequence: chub suspend -> lpd suspend -> chub suspend noirq (off by p/g)
static int lpd_suspend(struct device *dev)
{
	struct lpd_device *lpd = dev_get_drvdata(dev);
	NULL_CHECK(lpd);

	if (IS_LPD_SRAM_AVAILABLE(lpd))
		lpd_logbuf_noti_apstate(lpd, 1); // sleep

	lpd->sram.state |= LPD_SRAM_CHUB_PG;

	return 0;
}

// call sequence: chub resume noirq (on from p/g) -> lpd resume -> chub resume
static int lpd_resume(struct device *dev)
{
	struct lpd_device *lpd = dev_get_drvdata(dev);
	NULL_CHECK(lpd);
	lpd->sram.state &= ~LPD_SRAM_CHUB_PG;

	if (IS_LPD_SRAM_AVAILABLE(lpd))
		lpd_logbuf_noti_apstate(lpd, 0); // wake up

	return 0;
}

bool disp_is_on(void)
{
	return IS_LPD_ON_STATE(lpd_drvdata);
}
EXPORT_SYMBOL_GPL(disp_is_on);

static const struct platform_device_id exynos_lpd_ids[] = {
	{"exynos-lpd",},
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_lpd_ids);

static const struct dev_pm_ops lpd_pm_ops = {
	.suspend 	= lpd_suspend,
	.resume		= lpd_resume,
};

static const struct of_device_id lpd_of_match[] = {
	{.compatible = "samsung,exynos-lpd"},
	{},
};
MODULE_DEVICE_TABLE(of, lpd_of_match);

static struct platform_driver lpd_driver = {
	.probe 		= lpd_probe,
	.id_table   = exynos_lpd_ids,
	.remove 	= lpd_remove,
	.shutdown	= lpd_shutdown,
	.driver = {
		   .name = "exynos-lpd",
		   .owner = THIS_MODULE,
		   .of_match_table = lpd_of_match,
		   .pm = &lpd_pm_ops,
	},
};

static int __init lpd_init(void)
{
	return platform_driver_register(&lpd_driver);
}

static void __exit lpd_exit(void)
{
	platform_driver_unregister(&lpd_driver);
}

module_init(lpd_init);
module_exit(lpd_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos Low Power Display");
MODULE_AUTHOR("Dongho Lee <dh205.lee@samsung.com>");
MODULE_AUTHOR("Yeonjun Kim <yenjin.kim@samsung.com>");
