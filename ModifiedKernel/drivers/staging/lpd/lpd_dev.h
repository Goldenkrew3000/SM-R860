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

#ifndef ___SAMSUNG_LPD_DEV_H__
#define ___SAMSUNG_LPD_DEV_H__


#include <linux/types.h>
#include <linux/cdev.h>


struct lpd_chardev_info {
	struct cdev cdev;
	dev_t devid;
	struct class *class;
	struct device *csdev;
};


#define DEF_IOCTL_DESC(_cmd_, _func_, _desc_) {	\
	.cmd = _cmd_,				\
	.func = _func_,				\
	.desc = _desc_				\
}

enum {
	IOCTL_IDX_SET_COMPOSER = 0,
	IOCTL_IDX_SET_SENSOR,
	IOCTL_IDX_GET_SENSOR_META,
	IOCTL_IDX_SET_BRIGHTNESS,

	IOCTL_IDX_PRE_UPDATE,
	IOCTL_IDX_UPDATE,

	IOCTL_IDX_UPDATE_COMPOSER,
	IOCTL_IDX_UPDATE_SENSOR,
	IOCTL_IDX_UPDATE_BRIGHTNESS,
#ifdef USE_LEGACY_S_LSI_CODE
	IOCTL_IDX_PREPARE,
#endif
	IOCTL_IDX_MAX
};

#define CDEV_NAME				"exynos_lpd"

#define IOCTL_SET_COMPOSER_META			_IOW('l', 202, struct lpd_composer_meta)
#define IOCTL_SET_SENSOR_META			_IOW('l', 203, struct lpd_sensor_meta)
#define IOCTL_SET_BRIGHTNESS_META		_IOW('l', 204, struct lpd_brightness_meta)
#define IOCTL_GET_SENSOR_META			_IOW('l', 205, struct lpd_sensor_meta)

/* Regacy meta update */
#define IOCTL_CMD_PRE_UPDATE_META		_IOW('l', 101, struct lpd_composer_meta)
#define IOCTL_CMD_UPDATE_META			_IOW('l', 102, struct lpd_composer_meta)

#define IOCTL_UPDATE_COMPOSER_META		_IOW('l', 211, struct lpd_composer_meta)
#define IOCTL_UPDATE_SENSOR_META		_IOW('l', 212, struct lpd_sensor_meta)
#define IOCTL_UPDATE_BRIGHTNESS_META		_IOW('l', 211, struct lpd_brightness_meta)

#define IOCTL_CMD_PREPARE			_IOW('l', 201, struct lpd_meta_data)

#endif
