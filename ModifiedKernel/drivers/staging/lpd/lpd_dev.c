/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * character device driver for User Application
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "lpd.h"

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#include <soc/samsung/exynos-lpd.h>



#ifdef USE_LEGACY_S_LSI_CODE
int ioctl_func_prepare(struct lpd_device *lpd, void __user *argp)
{
	int idx;
	struct lpd_composer_meta *composer_meta;
	struct lpd_meta_data lpd_meta;

	if (!lpd) {
		lpd_err("null lpd");
		return -EINVAL;
	}

	if (copy_from_user(&lpd_meta, argp, sizeof(struct lpd_meta_data))) {
		lpd_err("failed to copy data from user\n");
		return -EFAULT;
	}

	if (IS_LPD_START_STATE(lpd)) {
		lpd_err("not allowed. should stop first\n");
		return -1;
	}

	composer_meta = &lpd_meta.composer_meta;

	// verify metadata & image info
	if (composer_meta->enable == false) {
		lpd_err("make sure if lpd_activation flag is enabled for LPD\n");
		return -1;
	}

	if (composer_meta->lpd_metadata_version != LPD_METADATA_API_VERSION)
		lpd_warn("LPD meta version mismatch %d.%d.%d vs %d.%d.%d\n",
			META_VERSION_MAJOR(composer_meta->lpd_metadata_version),
			META_VERSION_MINOR(composer_meta->lpd_metadata_version),
			META_VERSION_REVISION(composer_meta->lpd_metadata_version),
			META_VERSION_MAJOR(LPD_METADATA_API_VERSION),
			META_VERSION_MINOR(LPD_METADATA_API_VERSION),
			META_VERSION_REVISION(LPD_METADATA_API_VERSION));
	else
		lpd_info("LPD meta version %d.%d.%d\n",
			META_VERSION_MAJOR(composer_meta->lpd_metadata_version),
			META_VERSION_MINOR(composer_meta->lpd_metadata_version),
			META_VERSION_REVISION(composer_meta->lpd_metadata_version));

	for (idx = 0; idx < composer_meta->resource_cnt; idx++) {

		if (idx < MAX_RESOURCE_CNT) {
			lpd_dbg("resource[%d] addr %x size %x\n", idx,
				composer_meta->resources[idx].addr, composer_meta->resources[idx].buf_size);
		} else {
			lpd_err("resource cnt exceeds the maximum value\n");
			break;
		}
	}

	// save img desc
	memcpy(&lpd->lpd_meta, &lpd_meta, sizeof(struct lpd_meta_data));

	lpd_set_state(lpd, LPD_STATE_PREPARE);

	return 0;
}
#endif


static int check_meta_version(struct lpd_composer_meta *composer_meta)
{

	if (!composer_meta) {
		lpd_err("null meta data\n");
		return -EINVAL;
	}

	if (composer_meta->lpd_metadata_version != LPD_METADATA_API_VERSION) {
		lpd_warn("LPD meta version mismatch %d.%d.%d vs %d.%d.%d\n",
			META_VERSION_MAJOR(composer_meta->lpd_metadata_version),
			META_VERSION_MINOR(composer_meta->lpd_metadata_version),
			META_VERSION_REVISION(composer_meta->lpd_metadata_version),
			META_VERSION_MAJOR(LPD_METADATA_API_VERSION),
			META_VERSION_MINOR(LPD_METADATA_API_VERSION),
			META_VERSION_REVISION(LPD_METADATA_API_VERSION));
	} else {
		lpd_info("LPD meta version %d.%d.%d\n",
			META_VERSION_MAJOR(composer_meta->lpd_metadata_version),
			META_VERSION_MINOR(composer_meta->lpd_metadata_version),
			META_VERSION_REVISION(composer_meta->lpd_metadata_version));
	}
	return 0;
}

static int request_lpd_stop(struct lpd_device *lpd)
{
	int ret = 0;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	mutex_lock(&lpd->op_lock);

	if (lpd->dpu_state != DPU_STATE_DOZE_SUSPEND) {
		lpd_err("wrong dpu state: %d\n", lpd->dpu_state);
		ret = -EINVAL;
		goto err_stop;
	}

	if (lpd->state != LPD_STATE_START_END) {
		lpd_info("wrong lpd state: %d\n", lpd->state);
		ret = -EINVAL;
		goto err_stop;
	}

	lpd_notifier_call(LPD_NOTIFIER_STOP, NULL);
	lpd_notifier_call(LPD_NOTIFIER_STOP_POST, NULL);

err_stop:
	mutex_unlock(&lpd->op_lock);

	return ret;
}


static int request_lpd_start(struct lpd_device *lpd)
{
	int ret = 0;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	mutex_lock(&lpd->op_lock);

	if (lpd->dpu_state != DPU_STATE_DOZE_SUSPEND) {
		lpd_err("wrong dpu state: %d\n", lpd->dpu_state);
		ret = -EINVAL;
		goto err_start;
	}

	if (lpd->state != LPD_STATE_PM_RELEASE) {
		lpd_info("wrong lpd state: %d\n", lpd->state);
		ret = -EINVAL;
		goto err_start;
	}

	//lpd_notifier_call(LPD_NOTIFIER_CONFIG, &decon->lpd_dpu);
	lpd_notifier_call(LPD_NOTIFIER_START_PRE, NULL);
	lpd_notifier_call(LPD_NOTIFIER_START, NULL);

err_start:
	mutex_unlock(&lpd->op_lock);

	return ret;
}


static int ioctl_set_composer_meta(struct lpd_device *lpd, void __user *argp)
{
	int ret = 0;
	struct lpd_composer_meta composer_meta;

	NULL_CHECK(lpd);
	NULL_CHECK(argp);

	lpd_info("lpd start ~ meta update: %lld\n", ktime_to_us(ktime_sub(ktime_get(), lpd->ktime_lpd_start)));

#if !defined(LPD_SRAM_POWER_GATING)
	if (IS_LPD_START_STATE(lpd)) {
		lpd_err("not allowed. should stop first\n");
		return -1;
	}
#endif
	ret = copy_from_user(&composer_meta, argp, sizeof(struct lpd_composer_meta));
	if (ret) {
		lpd_err("failed to copy_from_user\n");
		return ret;
	}

	check_meta_version(&composer_meta);

	lpd->enable = composer_meta.enable;
	lpd_info("meta_data->enable: %d\n", composer_meta.enable);

#if defined(LPD_SRAM_POWER_GATING)
	memcpy(&lpd->lpd_meta.composer_meta, &composer_meta, sizeof(struct lpd_composer_meta));
#else
	ret = lpd_update_composer_data(lpd, &composer_meta);
	if (ret != sizeof(lpd_composer_meta)) {
		lpd_err("failed to update meta data: %d\n", ret);
		return ret;
	}
#endif

	return ret;
}

static int ioctl_set_sensor_meta(struct lpd_device *lpd, void __user *argp)
{
	int ret = 0;
	struct lpd_sensor_meta sensor_meta;


	if (!lpd) {
		lpd_err("null lpd");
		return -EINVAL;
	}

	if (!argp) {
		lpd_err("null argp");
		return -EINVAL;
	}

	if (IS_LPD_START_STATE(lpd)) {
		lpd_err("not allowed. should stop first\n");
		return -1;
	}

	ret = copy_from_user(&sensor_meta, argp, sizeof(struct lpd_sensor_meta));
	if (ret) {
		lpd_err("failed to copy_from_user\n");
		return ret;
	}

#if defined(LPD_SRAM_POWER_GATING)
	memcpy(&lpd->lpd_meta.sensor_meta, &sensor_meta, sizeof(struct lpd_sensor_meta));
#else
	ret = lpd_update_sensor_data(lpd, &sensor_meta);
	if (ret != sizeof(struct lpd_sensor_meta)) {
		lpd_err("failed to update sensor data: %d\n", ret);
		return -EINVAL;
	}
#endif

	return 0;
}

static int ioctl_get_sensor_meta(struct lpd_device *lpd, void __user *argp)
{
	int ret = 0;
	int offset;
	struct lpd_sensor_meta sensor_meta;

	if (!lpd) {
		lpd_err("null lpd");
		return -EINVAL;
	}

	if (!argp) {
		lpd_err("null argp");
		return -EINVAL;
	}

	if (!IS_LPD_START_STATE(lpd)) {
		lpd_err("not allowed\n");
		return -1;
	}

	offset = get_meta_data_offset(lpd, SENSOR_DATA_ID);
	if (offset < 0) {
		lpd_err("ERR:%s: wrong offset: %d\n", __func__, offset);
		return offset;
	}

	ret = lpd_sram_read(lpd, &sensor_meta, sizeof(struct lpd_sensor_meta), offset);
	if (ret < 0) {
		lpd_err("sram read fail\n");
		return ret;
	}

	if (copy_to_user(argp, &sensor_meta, sizeof(struct lpd_sensor_meta))) {
		lpd_err("failed to copy to user\n");
		return ret;
	}

	return ret;

}

static int ioctl_func_pre_update_meta(struct lpd_device *lpd, void __user *argp)
{
	int ret = 0;

	NULL_CHECK(lpd);

	lpd_info("state: %d, req: %d\n", lpd->state, lpd->meta_update_req);
	ret = request_lpd_stop(lpd);
	if (ret) {
		lpd_err("failed to request stop lpd\n");
		return ret;
	}

	lpd->meta_update_req = UPDATE_REQ_PREPARED;
	return ret;
}



static int ioctl_func_update_meta(struct lpd_device *lpd, void __user *argp)
{
	int ret = 0;
	struct lpd_composer_meta composer_meta;

	NULL_CHECK(lpd);
	NULL_CHECK(argp);

	if (lpd->meta_update_req != UPDATE_REQ_PREPARED) {
		lpd_err("req_meta_update was not set\n");
		return -EINVAL;
	}

	if (IS_LPD_START_STATE(lpd)) {
		lpd_err("already lpd start status\n");
		return ret;
	}

	if (IS_LPD_PM_ACQUIRE(lpd)) {
		lpd_err("cur status: pm_acquire, try again\n");
		msleep(300);
		return -EAGAIN;
	}

	ret = copy_from_user(&composer_meta, argp, sizeof(struct lpd_composer_meta));
	if (ret) {
		lpd_err("failed to copy_from_user\n");
		goto update_err;
	}

	check_meta_version(&composer_meta);

	lpd->enable = composer_meta.enable;
	lpd_info("meta_data->enable: %d\n", composer_meta.enable);

	if (composer_meta.enable == false) {
		lpd_info("lpd disabled current state: %d\n", lpd->state);
		if (lpd->state == LPD_STATE_PREPARE)
			lpd_set_state(lpd, LPD_STATE_INIT);

		return 0;
	}
#if defined(LPD_SRAM_POWER_GATING)
	memcpy(&lpd->lpd_meta.composer_meta, &composer_meta, sizeof(struct lpd_composer_meta));
#else
	ret = lpd_update_composer_data(lpd, &composer_meta);
	if (ret != sizeof(lpd_composer_meta)) {
		lpd_err("failed to update meta data: %d\n", ret);
		goto update_err;
	}
#endif
	ret = request_lpd_start(lpd);
	if (ret < 0) {
		lpd_err("failed to request lpd start\n", ret);
		goto update_err;
	}

	lpd->meta_update_req = UPDATE_REQ_DONE;

	return ret;

update_err:
	lpd_set_state(lpd, LPD_STATE_INIT);

	return ret;

}


#if IS_ENABLED(CONFIG_LPD_AUTO_BR)
static int ioctl_set_brightness_meta(struct lpd_device *lpd, void __user *argp)
{
	int i;
	int ret = 0;
	struct lpd_brightness_meta *br_meta;

	if (lpd == NULL) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}
	if (argp == NULL) {
		lpd_err("null argp\n");
		return -EINVAL;
	}

	br_meta = &lpd->br_meta;

	ret = copy_from_user(br_meta, argp, sizeof(struct lpd_brightness_meta));
	if (ret) {
		lpd_err("failed to copy_from_user\n");
		return ret;
	}

	lpd_info("set lpd meta brightness: count: %d, auto: %d\n", br_meta->br_cnt, br_meta->auto_br);
	if (br_meta->br_cnt == 0)
		return 0;

	for (i = 0; i < br_meta->br_cnt; i++)
		lpd_info("br_info[%d]: plat: %d, phy: %d\n",
			i, br_meta->br_info[i].platform_br, br_meta->br_info[i].physical_br);

	return ret;

}
#endif


struct lpd_ioctl_desc lpd_ioctl_info[IOCTL_IDX_MAX] = {
	[IOCTL_IDX_SET_COMPOSER] = DEF_IOCTL_DESC(IOCTL_SET_COMPOSER_META, ioctl_set_composer_meta, "set composer"),
	[IOCTL_IDX_SET_SENSOR] = DEF_IOCTL_DESC(IOCTL_SET_SENSOR_META, ioctl_set_sensor_meta, "set sensor"),
	[IOCTL_IDX_GET_SENSOR_META] = DEF_IOCTL_DESC(IOCTL_GET_SENSOR_META, ioctl_get_sensor_meta, "get sensor"),
#if IS_ENABLED(CONFIG_LPD_AUTO_BR)
	[IOCTL_IDX_SET_BRIGHTNESS] = DEF_IOCTL_DESC(IOCTL_SET_BRIGHTNESS_META, ioctl_set_brightness_meta, "set brightness"),
#else
	[IOCTL_IDX_SET_BRIGHTNESS] = DEF_IOCTL_DESC(IOCTL_SET_BRIGHTNESS_META, NULL, "set brightness"),
	[IOCTL_IDX_UPDATE_BRIGHTNESS] = DEF_IOCTL_DESC(IOCTL_UPDATE_BRIGHTNESS_META, NULL, "update brightness"),
#endif
	[IOCTL_IDX_PRE_UPDATE] = DEF_IOCTL_DESC(IOCTL_CMD_PRE_UPDATE_META, ioctl_func_pre_update_meta, "prepare update meta"),
	[IOCTL_IDX_UPDATE] = DEF_IOCTL_DESC(IOCTL_CMD_UPDATE_META, ioctl_func_update_meta, "update meta"),

#if IS_ENABLED(CONFIG_LPD_UPDATE_META)
#if IS_ENABLED(CONFIG_LPD_AUTO_BR)
	[IOCTL_IDX_UPDATE_BRIGHTNESS] = DEF_IOCTL_DESC(IOCTL_UPDATE_BRIGHTNESS_META, NULL, "update brightness"),
#endif
#else
	[IOCTL_IDX_UPDATE_COMPOSER] = DEF_IOCTL_DESC(IOCTL_UPDATE_COMPOSER_META, NULL, "update composer"),
	[IOCTL_IDX_UPDATE_SENSOR] = DEF_IOCTL_DESC(IOCTL_UPDATE_SENSOR_META, NULL, "update sensor"),
	[IOCTL_IDX_UPDATE_BRIGHTNESS] = DEF_IOCTL_DESC(IOCTL_UPDATE_BRIGHTNESS_META, NULL, "update brightness"),
#endif

#ifdef USE_LEGACY_S_LSI_CODE
	[IOCTL_IDX_PREPARE] = DEF_IOCTL_DESC(IOCTL_CMD_PREPARE, ioctl_func_prepare, "lpd prepare"),
#endif
};


static long lpd_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg_)
{
	int i;
	int ret = 0;
	struct lpd_device *lpd;
	struct lpd_ioctl_desc *ioctl_desc;
	void __user *argp = (void __user *)arg_;

	if (!file) {
		lpd_err("null file\n");
		return -EINVAL;
	}
	lpd = file->private_data;
	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	for (i = 0; i < IOCTL_IDX_MAX; i++) {
		ioctl_desc = &lpd_ioctl_info[i];
		if (ioctl_desc->cmd == cmd) {
			lpd_info(": %s\n", ioctl_desc->desc);
			if (ioctl_desc->func == NULL) {
				lpd_err("function for %s is null\n", ioctl_desc->desc);
				return -EINVAL;
			}
			ret = ioctl_desc->func(lpd, argp);
			if (ret < 0)
				lpd_info("failed to ioctl : %s\n", ioctl_desc->desc);

			goto exit_ioctl;
		}
	}

	lpd_err("undefined cmd : %d(%x)\n", cmd, cmd);
	return -EINVAL;

exit_ioctl:
	return ret;
}

#ifdef CONFIG_COMPAT
static long lpd_dev_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	arg = (unsigned long) compat_ptr(arg);

	return lpd_dev_ioctl(file, cmd, arg);
}
#endif

static int lpd_dev_open(struct inode *inode, struct file *file)
{
	struct lpd_device *lpd;

	if (!inode) {
		lpd_err("null inode\n");
		return -EINVAL;
	}

	if (!file) {
		lpd_err("null file\n");
		return -EINVAL;
	}

	lpd = container_of(inode->i_cdev, struct lpd_device, chardev.cdev);

	file->private_data = lpd;

	return 0;
}

static ssize_t lpd_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	return 0;
}

static ssize_t lpd_dev_write(struct file *file, const char *buffer,
			     size_t length, loff_t *offset)
{
	int ret;
	int8_t num_os;

	/* read int8_t num_os */
	ret = copy_from_user(&num_os, buffer, sizeof(num_os));

	return 0;
}

#if !defined(TEST_DRAM_ACCESS)
static int lpd_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct lpd_device *lpd;
	size_t rmem_size;
	unsigned long usr_req_offset, usr_req_size;

	if (!file) {
		lpd_err("null file\n");
		return -EINVAL;
	}

	lpd = file->private_data;
	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	rmem_size = lpd->rmem.size;

	usr_req_offset = vma->vm_pgoff << PAGE_SHIFT;
	usr_req_size = vma->vm_end - vma->vm_start;

	lpd_info("user request info : total size: %x (off: %x, size: %x), size for mmap: %x\n",
		usr_req_size + usr_req_offset, usr_req_size, usr_req_offset, rmem_size);

	if (usr_req_offset >= rmem_size) {
		lpd_err("invalid offset, user offset: %x, size for mmap: %x\n", usr_req_offset, rmem_size);
		return -ENXIO;
	}

	if ((usr_req_size + usr_req_offset) >= rmem_size) {
		lpd_err("exceed user req info %x(%x, %x) size for mmap %x\n", usr_req_size + usr_req_offset,
			usr_req_size, usr_req_offset, rmem_size);
		return -ENXIO;
	}

	ret = remap_pfn_range(vma, vma->vm_start, __phys_to_pfn(lpd->rmem.base) + vma->vm_pgoff,
			usr_req_size, pgprot_writecombine(vma->vm_page_prot));
	return ret;
}

#else
static int lpd_dev_mmap_dma(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct lpd_device *lpd;
	size_t rmem_size;

	int ret_pfn;

	if (!file) {
		lpd_err("null file\n");
		return -EINVAL;
	}

	lpd = file->private_data;
	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	rmem_size = lpd->rmem.size;

	ret = dma_mmap_from_dev_coherent(lpd->dev, vma, lpd->rmem.io_base, rmem_size, &ret_pfn);
	if(ret == 1 && ret_pfn >=0)
		ret = 0;
	else {
		lpd_err("no dev coherent memory ret %d ret_pfn %d\n", ret, ret_pfn);
		ret = -1;
	}

	return ret;
}

#endif

static const struct file_operations lpd_dev_fileops = {
	.owner = THIS_MODULE,
	.open = lpd_dev_open,
	.read = lpd_dev_read,
	.write = lpd_dev_write,
	.unlocked_ioctl = lpd_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= lpd_dev_compat_ioctl,
#endif

#if !defined(TEST_DRAM_ACCESS)
	.mmap = lpd_dev_mmap,
#else
	.mmap = lpd_dev_mmap_dma,
#endif
};

int lpd_dev_init(struct lpd_device *lpd)
{
	int ret = 0;
	struct lpd_chardev_info *chardev;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}
	chardev = &lpd->chardev;

	ret = alloc_chrdev_region(&chardev->devid, 0, 1, CDEV_NAME);
	if (ret < 0) {
		lpd_err("failed to alloc lpd dev: %d\n", ret);
		goto err;
	}
	cdev_init(&lpd->chardev.cdev, &lpd_dev_fileops);
	cdev_add(&lpd->chardev.cdev, chardev->devid, 1);

	chardev->class = class_create(THIS_MODULE, CDEV_NAME);
	if (IS_ERR(chardev->class)) {
		ret = PTR_ERR(chardev->class);
		lpd_err("failed to create lpd class: %d\n", ret);
		goto err;
	}

	chardev->csdev = device_create(chardev->class, NULL, chardev->devid, NULL, CDEV_NAME);
	if (IS_ERR(chardev->csdev)) {
		ret = PTR_ERR(chardev->csdev);
		lpd_err("failed to create lpd device: %d\n", ret);
		goto err;
	}

	return ret;
err:

	if (chardev->class)
		class_destroy(chardev->class);

	if (chardev->devid) {
		cdev_del(&lpd->chardev.cdev);
		unregister_chrdev_region(chardev->devid, 1);
	}
	return ret;
}

int lpd_dev_uninit(struct lpd_device *lpd)
{
	struct lpd_chardev_info *chardev;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}
	chardev = &lpd->chardev;

	device_destroy(chardev->class, chardev->devid);
	class_destroy(chardev->class);
	cdev_del(&lpd->chardev.cdev);
	unregister_chrdev_region(chardev->devid, 1);
	return 0;
}

