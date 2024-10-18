/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/math64.h>
#include <linux/ioctl.h>
#include <linux/compat.h>
#include <linux/types.h>
#include <linux/delay.h>
#include "ssp.h"
#include "ssp_dump.h"
#include "ssp_scontext.h"

#define BATCH_IOCTL_MAGIC		0xFC
#define SENSORHUB_DUMP_FILE_PATH "/data/vendor/w_sensorhub/sensor_reg_dump" //"/data/log/sensor_reg_dump"

struct batch_config {
	int64_t timeout;
	int64_t delay;
	int flag;
};

/*************************************************************************/
/* SSP data delay function                                              */
/*************************************************************************/

int get_msdelay(int64_t dDelayRate)
{
	return div_s64(dDelayRate, 1000000);
}

static void enable_sensor(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	u64 uNewEnable = 0;
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);
	int ret = 0;

	data->adDelayBuf[iSensorType] = dNewDelay;
	maxBatchReportLatency = data->batchLatencyBuf[iSensorType];
	batchOptions = data->batchOptBuf[iSensorType];

	switch (data->aiCheckStatus[iSensorType]) {
	case ADD_SENSOR_STATE:
		ssp_dbg("[SSP]: %s - add %llu, New = %lldns\n",
			 __func__, 1ULL << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;

		ret = send_instruction(data, ADD_SENSOR,
				iSensorType, uBuf, 9);
		pr_info("[SSP], delay %d, timeout %d, flag=%d, ret%d",
			dMsDelay, maxBatchReportLatency, uBuf[8], ret);
		if (ret <= 0) {
			uNewEnable =
				(u64)atomic64_read(&data->aSensorEnable)
				& (~(u64)(1ULL << iSensorType));
			atomic64_set(&data->aSensorEnable, uNewEnable);

			data->aiCheckStatus[iSensorType] = NO_SENSOR_STATE;
			break;
		}

		data->aiCheckStatus[iSensorType] = RUNNING_SENSOR_STATE;

		break;
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %llu, New = %lldns\n",
			__func__, 1ULL << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		data->aiCheckStatus[iSensorType] = ADD_SENSOR_STATE;
	}
}

static void change_sensor_delay(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);

	data->adDelayBuf[iSensorType] = dNewDelay;
	data->batchLatencyBuf[iSensorType] = maxBatchReportLatency;
	data->batchOptBuf[iSensorType] = batchOptions;

	switch (data->aiCheckStatus[iSensorType]) {
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %llu, New = %lldns\n",
			__func__, 1ULL << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		break;
	}
}

/*************************************************************************/
/* SSP data enable function                                              */
/*************************************************************************/

static int ssp_remove_sensor(struct ssp_data *data,
	unsigned int uChangedSensor, u64 uNewEnable)
{
	u8 uBuf[4];
	int64_t dSensorDelay = data->adDelayBuf[uChangedSensor];

	ssp_dbg("[SSP]: %s - remove sensor = %llu, current state = %llu\n",
		__func__, (u64)(1ULL << uChangedSensor), uNewEnable);

	data->batchLatencyBuf[uChangedSensor] = 0;
	data->batchOptBuf[uChangedSensor] = 0;

	if (uChangedSensor == ORIENTATION_SENSOR) {
		if (!(atomic64_read(&data->aSensorEnable)
			& (1ULL << ACCELEROMETER_SENSOR))) {
			uChangedSensor = ACCELEROMETER_SENSOR;
		} else {
			change_sensor_delay(data, ACCELEROMETER_SENSOR,
				data->adDelayBuf[ACCELEROMETER_SENSOR]);
			return 0;
		}
	} else if (uChangedSensor == ACCELEROMETER_SENSOR) {
		if (atomic64_read(&data->aSensorEnable)
			& (1ULL << ORIENTATION_SENSOR)) {
			change_sensor_delay(data, ORIENTATION_SENSOR,
				data->adDelayBuf[ORIENTATION_SENSOR]);
			return 0;
		}
	}

	if (!data->bSspShutdown)
		if (atomic64_read(&data->aSensorEnable) & (1ULL << uChangedSensor)) {
			s32 dMsDelay = get_msdelay(dSensorDelay);
			memcpy(&uBuf[0], &dMsDelay, 4);

			send_instruction(data, REMOVE_SENSOR,
				uChangedSensor, uBuf, 4);
		}
	data->aiCheckStatus[uChangedSensor] = NO_SENSOR_STATE;

	return 0;
}

/*************************************************************************/
/* ssp Sysfs                                                             */
/*************************************************************************/
static ssize_t show_sensors_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - cur_enable = %llu\n", __func__,
		(u64)(atomic64_read(&data->aSensorEnable)));

	return snprintf(buf, PAGE_SIZE, "%llu\n",
		(u64)(atomic64_read(&data->aSensorEnable)));
}

static ssize_t set_sensors_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dTemp;
	u64 uNewEnable = 0;
	unsigned int uChangedSensor = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	uNewEnable = (u64)dTemp;
	ssp_dbg("[SSP]: %s - new_enable = %llu, old_enable = %llu\n", __func__,
		 uNewEnable, atomic64_read(&data->aSensorEnable));

	if ((uNewEnable != atomic64_read(&data->aSensorEnable)) &&
		!(data->uSensorState &
		(uNewEnable - atomic64_read(&data->aSensorEnable)))) {
		pr_info("[SSP] %s - %llu is not connected(sensor state: 0x%llx)\n",
			__func__,
			uNewEnable - atomic64_read(&data->aSensorEnable),
			data->uSensorState);
		return -EINVAL;
	}

	if (uNewEnable == atomic64_read(&data->aSensorEnable))
		return size;

	for (uChangedSensor = 0; uChangedSensor < SENSOR_TYPE_SCONTEXT_T;
		uChangedSensor++) {
		if ((atomic64_read(&data->aSensorEnable) & (1ULL << uChangedSensor))
		!= (uNewEnable & (1ULL << uChangedSensor))) {

			if (!(uNewEnable & (1ULL << uChangedSensor))) {
				data->reportedData[uChangedSensor] = false;

				ssp_remove_sensor(data, uChangedSensor,
					uNewEnable); /* disable */
			} else { /* Change to ADD_SENSOR_STATE from KitKat */
				data->aiCheckStatus[uChangedSensor]
					= ADD_SENSOR_STATE;

				enable_sensor(data, uChangedSensor,
					data->adDelayBuf[uChangedSensor]);
			}
		}
	}
	atomic64_set(&data->aSensorEnable, uNewEnable);

	return size;
}

static ssize_t set_cal_data(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dTemp;
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_err("[SSP]%s\n", __func__);

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	if (unlikely(data->bSspShutdown)) {
		pr_err("[SSP]%s, stop sending cal data(shutdown)", __func__);
		return -EBUSY;
	}

	if (dTemp == 1) {
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
		accel_open_calibration(data);
		set_accel_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
		gyro_open_calibration(data);
		set_gyro_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
		pressure_open_calibration(data);
#if !defined(CONFIG_SENSORS_SSP_WISE) && !defined(CONFIG_SENSORS_SSP_FRESH)
		open_pressure_sw_offset_file(data);
#endif
		set_pressure_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
		hrm_open_calibration(data);
		set_hrm_calibration(data);
#endif

#ifdef CONFIG_SENSORS_SSP_BIA_SENSOR
		bia_open_calibration(data);
		set_bia_calibration(data);
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
		mag_open_calibration(data);
		set_mag_cal(data);
#endif
	}

	return size;
}

static ssize_t ssp_cmd_ctl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", (int)data->uCmdCtl);
}

static ssize_t ssp_cmd_ctl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u8 old_val;
	int val;

	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	old_val = data->uCmdCtl;
	data->uCmdCtl = (u8)val;
	pr_info("[SSP] %s : SSP cmd ctl update by %u (from %u)\n",
		__func__, data->uCmdCtl, old_val);

	return size;
}

static int ssp_save_sensorhub_dump (struct ssp_data *data, u8 sensor_type)
{
	int iRet = 0;
	char file_path[64] = {0,};
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;


	//Read sensor reg dump from sensorhub
	iRet = sensor_reg_dump_read(data, sensor_type);
	if (iRet != SUCCESS) {
		//kvfree(buffer);
		pr_err("[SSP]: %s - dump read error\n", __func__);
		return 0;
	}


	snprintf(file_path, sizeof(file_path), "%s-%d.txt", SENSORHUB_DUMP_FILE_PATH, sensor_type);
	ssp_infof("%s", file_path);
	//Store to file
	old_fs = get_fs();
	set_fs(get_ds());
	cal_filp = filp_open(file_path,
			O_CREAT | O_TRUNC | O_RDWR, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		pr_err("[SSP]: %s - Can't open sensorhub dump file, %d\n", __func__, iRet);
		//kvfree(buffer);
		return -EIO;
	}

	iRet = vfs_write(cal_filp, (char *)data->reg_dump, SENSORHUB_REG_DUMP_SIZE, &cal_filp->f_pos);
	if (iRet != SENSORHUB_REG_DUMP_SIZE) {
		pr_err("[SSP]: %s - Can't write sensorhub dump to file (%d)\n", __func__, iRet);
		//kvfree(buffer);
		return -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return iRet;
}
static ssize_t sensor_reg_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	ssize_t count;

	ssp_save_sensorhub_dump(data, ACCELEROMETER_SENSOR);
	print_acc_esn_data(data);
	ssp_save_sensorhub_dump(data, PRESSURE_SENSOR);


	count = snprintf(buf, PAGE_SIZE, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
									"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
									"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
									"%x %x %x %x %x %x %x\n",
		data->reg_dump[0], data->reg_dump[1], data->reg_dump[2], data->reg_dump[3], data->reg_dump[4], data->reg_dump[5], data->reg_dump[6], data->reg_dump[7], data->reg_dump[8], data->reg_dump[9],
		data->reg_dump[10], data->reg_dump[11], data->reg_dump[12], data->reg_dump[13], data->reg_dump[14], data->reg_dump[15], data->reg_dump[16], data->reg_dump[17], data->reg_dump[18], data->reg_dump[19],
		data->reg_dump[20], data->reg_dump[21], data->reg_dump[22], data->reg_dump[23], data->reg_dump[24], data->reg_dump[25], data->reg_dump[26], data->reg_dump[27], data->reg_dump[28], data->reg_dump[29],
		data->reg_dump[30], data->reg_dump[31], data->reg_dump[32], data->reg_dump[33], data->reg_dump[34], data->reg_dump[35], data->reg_dump[36], data->reg_dump[37], data->reg_dump[38], data->reg_dump[39],
		data->reg_dump[40], data->reg_dump[41], data->reg_dump[42], data->reg_dump[43], data->reg_dump[44], data->reg_dump[45], data->reg_dump[46], data->reg_dump[47], data->reg_dump[48], data->reg_dump[49],
		data->reg_dump[50], data->reg_dump[51], data->reg_dump[52], data->reg_dump[53], data->reg_dump[54], data->reg_dump[55], data->reg_dump[56], data->reg_dump[57], data->reg_dump[58], data->reg_dump[59],
		data->reg_dump[60], data->reg_dump[61], data->reg_dump[62], data->reg_dump[63], data->reg_dump[64], data->reg_dump[65], data->reg_dump[66], data->reg_dump[67], data->reg_dump[68], data->reg_dump[69],
		data->reg_dump[70], data->reg_dump[71], data->reg_dump[72], data->reg_dump[73], data->reg_dump[74], data->reg_dump[75], data->reg_dump[76], data->reg_dump[77], data->reg_dump[78], data->reg_dump[79],
		data->reg_dump[80], data->reg_dump[81], data->reg_dump[82], data->reg_dump[83], data->reg_dump[84], data->reg_dump[85], data->reg_dump[86], data->reg_dump[87], data->reg_dump[88], data->reg_dump[89],
		data->reg_dump[90], data->reg_dump[91], data->reg_dump[92], data->reg_dump[93], data->reg_dump[94], data->reg_dump[95], data->reg_dump[96], data->reg_dump[97], data->reg_dump[98], data->reg_dump[99],
		data->reg_dump[100], data->reg_dump[101], data->reg_dump[102], data->reg_dump[103], data->reg_dump[104], data->reg_dump[105], data->reg_dump[106], data->reg_dump[107], data->reg_dump[108], data->reg_dump[109],
		data->reg_dump[110], data->reg_dump[111], data->reg_dump[112], data->reg_dump[113], data->reg_dump[114], data->reg_dump[115], data->reg_dump[116], data->reg_dump[117], data->reg_dump[118], data->reg_dump[119],
		data->reg_dump[120], data->reg_dump[121], data->reg_dump[122], data->reg_dump[123], data->reg_dump[124], data->reg_dump[125], data->reg_dump[126]);

	return count;
}

static ssize_t sensor_stuck_reg_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	ssize_t count;

	iRet = sensor_reg_dump_read(data, data->stuck_sensor_id | 0x80);
	if (iRet != SUCCESS) {
		//kvfree(buffer);
		pr_err("[SSP]: %s - dump read error\n", __func__);
		return 0;
	}

	count = snprintf(buf, PAGE_SIZE, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
									"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
									"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
									"%x %x %x %x %x %x %x\n",
		data->reg_dump[0], data->reg_dump[1], data->reg_dump[2], data->reg_dump[3], data->reg_dump[4], data->reg_dump[5], data->reg_dump[6], data->reg_dump[7], data->reg_dump[8], data->reg_dump[9],
		data->reg_dump[10], data->reg_dump[11], data->reg_dump[12], data->reg_dump[13], data->reg_dump[14], data->reg_dump[15], data->reg_dump[16], data->reg_dump[17], data->reg_dump[18], data->reg_dump[19],
		data->reg_dump[20], data->reg_dump[21], data->reg_dump[22], data->reg_dump[23], data->reg_dump[24], data->reg_dump[25], data->reg_dump[26], data->reg_dump[27], data->reg_dump[28], data->reg_dump[29],
		data->reg_dump[30], data->reg_dump[31], data->reg_dump[32], data->reg_dump[33], data->reg_dump[34], data->reg_dump[35], data->reg_dump[36], data->reg_dump[37], data->reg_dump[38], data->reg_dump[39],
		data->reg_dump[40], data->reg_dump[41], data->reg_dump[42], data->reg_dump[43], data->reg_dump[44], data->reg_dump[45], data->reg_dump[46], data->reg_dump[47], data->reg_dump[48], data->reg_dump[49],
		data->reg_dump[50], data->reg_dump[51], data->reg_dump[52], data->reg_dump[53], data->reg_dump[54], data->reg_dump[55], data->reg_dump[56], data->reg_dump[57], data->reg_dump[58], data->reg_dump[59],
		data->reg_dump[60], data->reg_dump[61], data->reg_dump[62], data->reg_dump[63], data->reg_dump[64], data->reg_dump[65], data->reg_dump[66], data->reg_dump[67], data->reg_dump[68], data->reg_dump[69],
		data->reg_dump[70], data->reg_dump[71], data->reg_dump[72], data->reg_dump[73], data->reg_dump[74], data->reg_dump[75], data->reg_dump[76], data->reg_dump[77], data->reg_dump[78], data->reg_dump[79],
		data->reg_dump[80], data->reg_dump[81], data->reg_dump[82], data->reg_dump[83], data->reg_dump[84], data->reg_dump[85], data->reg_dump[86], data->reg_dump[87], data->reg_dump[88], data->reg_dump[89],
		data->reg_dump[90], data->reg_dump[91], data->reg_dump[92], data->reg_dump[93], data->reg_dump[94], data->reg_dump[95], data->reg_dump[96], data->reg_dump[97], data->reg_dump[98], data->reg_dump[99],
		data->reg_dump[100], data->reg_dump[101], data->reg_dump[102], data->reg_dump[103], data->reg_dump[104], data->reg_dump[105], data->reg_dump[106], data->reg_dump[107], data->reg_dump[108], data->reg_dump[109],
		data->reg_dump[110], data->reg_dump[111], data->reg_dump[112], data->reg_dump[113], data->reg_dump[114], data->reg_dump[115], data->reg_dump[116], data->reg_dump[117], data->reg_dump[118], data->reg_dump[119],
		data->reg_dump[120], data->reg_dump[121], data->reg_dump[122], data->reg_dump[123], data->reg_dump[124], data->reg_dump[125], data->reg_dump[126]);

	return count;
}

static ssize_t set_sensor_stuck_reg_dump(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int sensor_id;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &sensor_id) < 0)
		return -EINVAL;

	pr_info("[SSP] %s - %d\n", __func__, sensor_id);

	data->stuck_sensor_id = (u8) sensor_id;

	return size;
}

#if 0
static ssize_t sensorhub_dump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet = 0;
	char file_path[64] = {0,};
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	struct ssp_data *data = dev_get_drvdata(dev);
	u8 sensor_type;

	if (kstrtou8(buf, 10, &sensor_type) < 0)
		return -EINVAL;

	//Read dump from sensorhub
	iRet = sensorhub_dump_read(data, sensor_type);
	if (iRet != SUCCESS) {
		//kvfree(buffer);
		pr_err("[SSP]: %s - dump read error\n", __func__);
		return 0;
	}


	snprintf(file_path, sizeof(file_path), "%s-%d.txt", SENSORHUB_DUMP_FILE_PATH, sensor_type);
	ssp_infof("%s", file_path);
	//Store to file
	old_fs = get_fs();
	set_fs(get_ds());
	cal_filp = filp_open(file_path,
			O_CREAT | O_TRUNC | O_RDWR, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		pr_err("[SSP]: %s - Can't open sensorhub dump file, %d\n", __func__, iRet);
		//kvfree(buffer);
		return -EIO;
	}

	iRet = vfs_write(cal_filp, (char *)data->reg_dump, SENSORHUB_REG_DUMP_SIZE, &cal_filp->f_pos);
	if (iRet != SENSORHUB_REG_DUMP_SIZE) {
		pr_err("[SSP]: %s - Can't write sensorhub dump to file (%d)\n", __func__, iRet);
		//kvfree(buffer);
		return -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_err("[SSP]: %s - Success\n", __func__);

	return size;
}
#endif

static ssize_t show_acc_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[ACCELEROMETER_SENSOR]);
}

static ssize_t set_acc_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	if ((atomic64_read(&data->aSensorEnable) & (1ULL << ORIENTATION_SENSOR)) &&
		(data->adDelayBuf[ORIENTATION_SENSOR] < dNewDelay))
		data->adDelayBuf[ACCELEROMETER_SENSOR] = dNewDelay;
	else
		change_sensor_delay(data, ACCELEROMETER_SENSOR, dNewDelay);

	return size;
}

#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_32GSENSOR
static ssize_t show_acc32g_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[ACCELEROMETER_SENSOR_32G]);
}

static ssize_t set_acc32g_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	if ((atomic64_read(&data->aSensorEnable) & (1ULL << ORIENTATION_SENSOR)) &&
		(data->adDelayBuf[ORIENTATION_SENSOR] < dNewDelay))
		data->adDelayBuf[ACCELEROMETER_SENSOR_32G] = dNewDelay;
	else
		change_sensor_delay(data, ACCELEROMETER_SENSOR_32G, dNewDelay);

	return size;
}
#endif

static ssize_t show_gyro_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GYROSCOPE_SENSOR]);
}

static ssize_t set_gyro_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GYROSCOPE_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_uncalib_gyro_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GYRO_UNCALIB_SENSOR]);
}

static ssize_t set_uncalib_gyro_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GYRO_UNCALIB_SENSOR, dNewDelay);

	return size;
}

#ifdef CONFIG_SENSORS_SSP_LINEAR_ACCELEROMETER_SENSOR
static ssize_t show_linear_accel_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[LINEAR_ACCEL_SENSOR]);
}

static ssize_t set_linear_accel_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, LINEAR_ACCEL_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
static ssize_t show_mag_uncal_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GEOMAGNETIC_UNCALIB_SENSOR]);
}

static ssize_t set_mag_uncal_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GEOMAGNETIC_UNCALIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_mag_calib_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GEOMAGNETIC_CALIB_SENSOR]);
}

static ssize_t set_mag_calib_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GEOMAGNETIC_CALIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_mag_power_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GEOMAGNETIC_POWER_SENSOR]);
}

static ssize_t set_mag_power_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GEOMAGNETIC_POWER_SENSOR, dNewDelay);

	return size;
}
#endif

static ssize_t show_pressure_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[PRESSURE_SENSOR]);
}

static ssize_t set_pressure_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, PRESSURE_SENSOR, dNewDelay);
	return size;
}

#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
static ssize_t show_light_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[LIGHT_SENSOR]);
}

static ssize_t set_light_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, LIGHT_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_auto_brightness_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[AUTO_BRIGHTNESS_SENSOR]);
}

static ssize_t set_auto_brightness_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, AUTO_BRIGHTNESS_SENSOR, dNewDelay);
	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
static ssize_t show_temp_humi_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[TEMPERATURE_HUMIDITY_SENSOR]);
}

static ssize_t set_temp_humi_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, TEMPERATURE_HUMIDITY_SENSOR, dNewDelay);
	return size;
}
#endif

#ifdef CONFIG_SENSORS_ECG_LIB
static ssize_t show_ecg_lib_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_ECG_LIB_SENSOR]);
}

static ssize_t set_ecg_lib_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_ECG_LIB_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
static ssize_t show_rot_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[ROTATION_VECTOR_SENSOR]);
}

static ssize_t set_rot_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, ROTATION_VECTOR_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_game_rot_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GAME_ROTATION_VECTOR_SENSOR]);
}

static ssize_t set_game_rot_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GAME_ROTATION_VECTOR_SENSOR, dNewDelay);

	return size;
}
#endif
static ssize_t show_hrm_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_RAW_SENSOR]);
}

static ssize_t set_hrm_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_RAW_SENSOR, dNewDelay);
	change_sensor_delay(data, HRM_RAW_FAC_SENSOR, dNewDelay);
	change_sensor_delay(data, HRM_RAW_FAC2_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_hrm_lib_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_LIB_SENSOR]);
}

static ssize_t set_hrm_lib_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_LIB_SENSOR, dNewDelay);

	return size;
}

#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
static ssize_t show_front_hrm_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[FRONT_HRM_RAW_SENSOR]);
}

static ssize_t set_front_hrm_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, FRONT_HRM_RAW_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
static ssize_t show_uv_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[UV_SENSOR]);
}

static ssize_t set_uv_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, UV_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
static ssize_t show_gsr_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", sdev->adDelayBuf[GSR_SENSOR]);
}

static ssize_t set_gsr_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, GSR_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
static ssize_t show_ecg_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", sdev->adDelayBuf[ECG_SENSOR]);
}

static ssize_t set_ecg_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, ECG_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
static ssize_t show_grip_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		sdev->adDelayBuf[GRIP_SENSOR]);
}

static ssize_t set_grip_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, GRIP_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_BIA_SENSOR
static ssize_t show_bia_raw_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_BIA_RAW_SENSOR]);
}

static ssize_t set_bia_raw_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_BIA_RAW_SENSOR, dNewDelay);

	return size;
}
#endif

#ifdef CONFIG_SENSORS_SSP_TEMPERATURE_SENSOR
static ssize_t show_temperature_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[TEMPERATURE_SENSOR]);
}

static ssize_t set_temperature_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, THERMISTER_SENSOR, dNewDelay);

	return size;
}
#endif

static ssize_t feature_list_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	sdev->uFeatureList = get_feature_list(sdev);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", sdev->uFeatureList);
}

static ssize_t ssr_stuck_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);
	int stuck_cnt[4];
	int accel_stuck_value[3];
	int gyro_stuck_value[3];
	int mag_stuck_value[3];
	int baro_stuck_value;
	int gyro_cal_cnt;
	int64_t lib_status1, lib_status2;
	u8* esn = sdev->acc_esn;

	get_ssr_stuck_info(sdev);

	ssp_dbg("[SSP]: %s - 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n",
		__func__, sdev->uSsrStuckInfo[0], sdev->uSsrStuckInfo[1], sdev->uSsrStuckInfo[2],
		sdev->uSsrStuckInfo[3], sdev->uSsrStuckInfo[4], sdev->uSsrStuckInfo[5], sdev->uSsrStuckInfo[6],
		sdev->uSsrStuckInfo[7], sdev->uSsrStuckInfo[8], sdev->uSsrStuckInfo[9]);

	stuck_cnt[0] = sdev->uSsrStuckInfo[0];
	stuck_cnt[1] = sdev->uSsrStuckInfo[1];
	stuck_cnt[2] = sdev->uSsrStuckInfo[2];
	stuck_cnt[3] = sdev->uSsrStuckInfo[3];

	accel_stuck_value[0] = (int)((sdev->uSsrStuckInfo[5] << 8) + sdev->uSsrStuckInfo[4]);
	accel_stuck_value[1] = (int)((sdev->uSsrStuckInfo[7] << 8) + sdev->uSsrStuckInfo[6]);
	accel_stuck_value[2] = (int)((sdev->uSsrStuckInfo[9] << 8) + sdev->uSsrStuckInfo[8]);

	gyro_stuck_value[0] = (int)((sdev->uSsrStuckInfo[11] << 8) + sdev->uSsrStuckInfo[10]);
	gyro_stuck_value[1] = (int)((sdev->uSsrStuckInfo[13] << 8) + sdev->uSsrStuckInfo[12]);
	gyro_stuck_value[2] = (int)((sdev->uSsrStuckInfo[15] << 8) + sdev->uSsrStuckInfo[14]);

	mag_stuck_value[0] = (int)((sdev->uSsrStuckInfo[19] << 24) + (sdev->uSsrStuckInfo[18] << 16)
		+(sdev->uSsrStuckInfo[17] << 8) + sdev->uSsrStuckInfo[16]);
	mag_stuck_value[1] = (int)((sdev->uSsrStuckInfo[23] << 24) + (sdev->uSsrStuckInfo[22] << 16)
		+(sdev->uSsrStuckInfo[21] << 8) + sdev->uSsrStuckInfo[20]);
	mag_stuck_value[2] = (int)((sdev->uSsrStuckInfo[27] << 24) + (sdev->uSsrStuckInfo[26] << 16)
		+(sdev->uSsrStuckInfo[25] << 8) + sdev->uSsrStuckInfo[24]);
	
	baro_stuck_value = (int)((sdev->uSsrStuckInfo[31] << 24) + (sdev->uSsrStuckInfo[30] << 16)
		+(sdev->uSsrStuckInfo[29] << 8) + sdev->uSsrStuckInfo[28]);

	memcpy(&gyro_cal_cnt, &sdev->uSsrStuckInfo[32], sizeof(int32_t));
	memcpy(&lib_status1, &sdev->uSsrStuckInfo[36], sizeof(int64_t));
	memcpy(&lib_status2, &sdev->uSsrStuckInfo[44], sizeof(int64_t));

	return snprintf(buf, PAGE_SIZE, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %llx %llx "\
		"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
		"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
		"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
		"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x "\
		"%x %x %x\n",
		stuck_cnt[0], stuck_cnt[1], stuck_cnt[2], stuck_cnt[3],
		accel_stuck_value[0], accel_stuck_value[1], accel_stuck_value[2],
		gyro_stuck_value[0], gyro_stuck_value[1], gyro_stuck_value[2],
		mag_stuck_value[0], mag_stuck_value[1], mag_stuck_value[2],
		baro_stuck_value, gyro_cal_cnt, lib_status1, lib_status2,
		esn[0], esn[1], esn[2], esn[3], esn[4], esn[5], esn[6], esn[7], esn[8], esn[9],
		esn[10], esn[11], esn[12], esn[13], esn[14], esn[15], esn[16], esn[17], esn[18], esn[19],
		esn[20], esn[21], esn[22], esn[23], esn[24], esn[25], esn[26], esn[27], esn[28], esn[29],
		esn[30], esn[31], esn[32], esn[33], esn[34], esn[35], esn[36], esn[37], esn[38], esn[39],
		esn[40], esn[41], esn[42], esn[43], esn[44], esn[45], esn[46], esn[47], esn[48], esn[49],
		esn[50], esn[51], esn[52], esn[53], esn[54], esn[55], esn[56], esn[57], esn[58], esn[59],
		esn[60], esn[61], esn[62], esn[63], esn[64], esn[65], esn[66], esn[67], esn[68], esn[69],
		esn[70], esn[71], esn[72], esn[73], esn[74], esn[75], esn[76], esn[77], esn[78], esn[79],
		esn[80], esn[81], esn[82]);
}

static ssize_t sensorhub_crash_mini_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int count = snprintf(buf, PAGE_SIZE, "%2d-%2d %2d:%2d:%2d "\
		"%x %x %x %x %x %x %x %x %x %x "\
		"%x %x %x %x %x %x %x %x %x %x "\
		"%x %x %x %x %x %x %x %x %x %x\n",
		data->crash_mini_dump.month, data->crash_mini_dump.day, data->crash_mini_dump.hour, data->crash_mini_dump.min, data->crash_mini_dump.sec,
		data->crash_mini_dump.gpr_dump[0], data->crash_mini_dump.gpr_dump[1], data->crash_mini_dump.gpr_dump[2],
		data->crash_mini_dump.gpr_dump[3], data->crash_mini_dump.gpr_dump[4], data->crash_mini_dump.gpr_dump[5],
		data->crash_mini_dump.gpr_dump[6], data->crash_mini_dump.gpr_dump[7], data->crash_mini_dump.gpr_dump[8],
		data->crash_mini_dump.gpr_dump[9], data->crash_mini_dump.gpr_dump[10], data->crash_mini_dump.gpr_dump[11],
		data->crash_mini_dump.gpr_dump[12], data->crash_mini_dump.gpr_dump[13], data->crash_mini_dump.gpr_dump[14],
		data->crash_mini_dump.gpr_dump[15], data->crash_mini_dump.gpr_dump[16],
		data->crash_mini_dump.hardfault_info[0], data->crash_mini_dump.hardfault_info[1], data->crash_mini_dump.hardfault_info[2],
		data->crash_mini_dump.hardfault_info[3], data->crash_mini_dump.hardfault_info[4], data->crash_mini_dump.hardfault_info[5],
		data->crash_mini_dump.hardfault_info[6], data->crash_mini_dump.hardfault_info[7], data->crash_mini_dump.hardfault_info[8],
		data->crash_mini_dump.hardfault_info[9], data->crash_mini_dump.hardfault_info[10], data->crash_mini_dump.hardfault_info[11],
		data->crash_mini_dump.hardfault_info[12]);
	return count;
}

static ssize_t debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	print_saved_time();

	return snprintf(buf, PAGE_SIZE, "%lld\n", data->debug_enable);
}


static ssize_t set_debug_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t debug_enable;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &debug_enable) < 0)
		return -EINVAL;

	pr_info("[SSP] %s - %lld -> %lld\n", __func__, data->debug_enable, debug_enable);

	data->debug_enable = debug_enable;

	return size;
}

static ssize_t scan_bytes_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int scan_bytes[SENSOR_TYPE_MAX] = {0, };
	int i;

	for (i = 0 ; i < SENSOR_TYPE_MAX ; i++) {
		if (data->indio_devs[i] != NULL) {
			scan_bytes[i] = data->indio_devs[i]->scan_bytes;
		} else {
			scan_bytes[i] = -1;
		}
	}

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
	scan_bytes[0], scan_bytes[1], scan_bytes[2], scan_bytes[3], scan_bytes[4], scan_bytes[5], scan_bytes[6], scan_bytes[7],
	scan_bytes[8], scan_bytes[9], scan_bytes[10], scan_bytes[11], scan_bytes[12], scan_bytes[13], scan_bytes[14], scan_bytes[15],
	scan_bytes[16], scan_bytes[17], scan_bytes[18], scan_bytes[19], scan_bytes[20], scan_bytes[21], scan_bytes[22], scan_bytes[23],
	scan_bytes[24], scan_bytes[25], scan_bytes[26], scan_bytes[27], scan_bytes[28], scan_bytes[29], scan_bytes[30], scan_bytes[31],
	scan_bytes[32]);
}


static ssize_t shub_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int dump_size = 0;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	ssp_dump_file(sdev, &dump_size);

	return snprintf(buf, PAGE_SIZE, "%d\n", dump_size);
}

static ssize_t dumpstate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct ssp_data *sdev = dev_get_drvdata(dev);
	char out_name[64] = {0,};

	ret = ssp_dumpstate(sdev, out_name);

	return snprintf(buf, PAGE_SIZE, "%d, %s\n", ret, out_name);
}

static ssize_t bugreport_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct ssp_data *sdev = dev_get_drvdata(dev);
	char out_name[64] = {0,};

	ret = ssp_bugreport(sdev, out_name);

	return snprintf(buf, PAGE_SIZE, "%d, %s\n", ret, out_name);
}

const char *fw_type_name[4] = {
	"None",
	"Bin",
	"Push",
	"SPU",
};

static ssize_t debug_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s, MODEL:%d, SensorState:0x%llx\n", fw_type_name[data->fw_type],
		data->model_number, data->uSensorState);
}

static ssize_t sensor_position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "A: %u, G: %u, M: %u\n",
			data->accel_position, data->accel_position, data->mag_position);
}

static ssize_t sensor_position_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint8_t tmp[25] = {0,};
	char *strptr, *tkn;
	ssize_t len;
	uint8_t type, pos;

	struct ssp_data *data = dev_get_drvdata(dev);

	len = min(size, sizeof(tmp) - 1);

	memcpy(tmp, buf, len);
	pr_info("[SSP] %s - tmp[%d] %s\n", __func__, (int)size, tmp);

	tmp[len] = '\0';
	strptr = tmp;

	tkn = strsep(&strptr, " ");
	if (!tkn) {
		pr_err("[SSP] %s - sensor type NULL!\n", __func__);
		return -EINVAL;
	}
	if (!strcmp(tkn, "acc")) {
		type = 1;
	} else if (!strcmp(tkn, "mag")) {
		type = 2;
	} else {
		pr_err("[SSP] %s - wrong sensor type!\n", __func__);
		return -EINVAL;
	}

	tkn = strsep(&strptr, " ");
	if (!tkn) {
		pr_err("[SSP] %s - position NULL!\n", __func__);
		return -EINVAL;
	}
	if (kstrtou8(tkn, 0, &pos)) {
		pr_err("[SSP] %s - value INVAL!\n", __func__);
		return -EINVAL;
	}

	pr_info("type: %u. position: %u\n", type, pos);

	if (type == 1) {
		data->accel_position = pos;
	} else if (type == 2) {
		data->mag_position = pos;
	}
	set_sensor_position(data);

	return size;
}

static ssize_t library_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	char *strptr, *tkn;
	uint8_t buffer[30] = {0,};
	uint8_t tbuf[50] = {0,};
	int length = 0;
	int ret = 0;
	ssize_t len;

	if (size >= sizeof(tbuf)) {
		ret = -EIO;
		goto out;
	}

	len = min(size, sizeof(tbuf) - 1);
	memcpy(tbuf, buf, len);
	pr_info("[SSP] %s - in[%d] %s\n", __func__, (int)size, tbuf);

	tbuf[len] = '\0';
	strptr = tbuf;

	for (length = 0; length < 30; length++) {
		tkn = strsep(&strptr, " ");
		if (!tkn)
			break;
		if (kstrtou8(tkn, 0, &buffer[length])) {
			ret = -EINVAL;
			goto out;
		}
	}

	ret = ssp_scontext_test_write(data, buffer, length);

out:
	if (ret == 0)
		pr_err("[SSP] %s : SUCCESS\n", __func__);
	else
		pr_err("[SSP] %s : FAIL (%d)(%d:%d)\n", __func__, ret, size, sizeof(tbuf));

	return size;
}

static int flush(struct ssp_data *data, u8 sensor_type)
{
	int iRet = 0;
	struct ssp_msg *msg;

	ssp_info("%s (%d): Start\n", __func__, sensor_type);

	if (sensor_type == SENSOR_TYPE_SCONTEXT_T)
		return SUCCESS;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		ssp_err("%s, failed to alloc memory for ssp_msg", __func__);
		return -ENOMEM;
	}
	msg->cmd = MSG2SSP_AP_SENSOR_FLUSH;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char *) kzalloc(1, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = sensor_type;

	iRet = ssp_send_command(data, msg->cmd, msg->options, 0,
													 msg->buffer, msg->length, NULL, NULL, msg->data);
	if (iRet != SUCCESS) {
		ssp_err("%s - fail %d", __func__, iRet);
		goto out;
	}
	ssp_info("%s (%d) : End\n", __func__, sensor_type);

out:
	kfree(msg->buffer);
	kfree(msg);
	return iRet;
}

static ssize_t set_flush(struct device *dev,
			 struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int64_t dTemp;
	u8 sensor_type = 0;

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	sensor_type = (u8)dTemp;
	if (!(atomic64_read(&data->aSensorEnable) & (1ULL << sensor_type))) {
		ssp_infof("ssp sensor is not enabled(%d)", sensor_type);
		return -EINVAL;
	}

	if (flush(data, sensor_type) < 0) {
		ssp_errf("ssp returns error for flush(%x)", sensor_type);
		return -EINVAL;
	}
	return size;
}

static ssize_t library_protocol_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);
	int ret = 0;
	int lib_cnt = 0;
	int idx;
	
	get_library_protocol_version(sdev);

	lib_cnt = (int)sdev->lib_version[0];

	ret += snprintf(buf, PAGE_SIZE, "%d ", lib_cnt);

	if(lib_cnt > 0) {
		for(idx = 0; idx < lib_cnt; idx++)
			ret += snprintf(buf + ret, PAGE_SIZE, "%d %d ", 
			(int)sdev->lib_version[idx *2 + 1], (int)sdev->lib_version[idx *2 + 2]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE, "\n");
	
	return ret;
	
}

static ssize_t hrm_shutdown_show(struct device *dev,
       struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);
	int hrm3p3v = regulator_is_enabled(sdev->reg_hrm3p3v);
	int hrm1p8v = regulator_is_enabled(sdev->reg_hrm1p8v);

	pr_info("[SSP] reg_hrm3p3v (%d) : reg_hrm1p8v (%d)\n", hrm3p3v, hrm1p8v);

	if (sdev->reg_hrm3p3v && sdev->reg_hrm1p8v) {
		return snprintf(buf, PAGE_SIZE, "[HRM Shutdown:%d] reg_hrm3p3v enabled(%d) reg_hrm1p8v enabled(%d)\n",
				sdev->bHRMShutdown, hrm3p3v, hrm1p8v);
	} else {
		pr_info("[SSP] %s: reg_hrm3p3v and/or hrm1p8v null\n",__func__);
		return snprintf(buf, PAGE_SIZE, "reg_hrm3p3v null\n");
	}
}

// AFE_TI = 0, AFE_ADI = 1
static ssize_t hrm_shutdown_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);
	int shutdown;

	if (kstrtoint(buf, 10, &shutdown) < 0)
		return -EINVAL;

	if (sdev->reg_hrm3p3v == NULL || sdev->reg_hrm1p8v == NULL) {
		pr_info("[SSP] %s: reg_hrm3p3v null\n",__func__);
		return size;
	}

	sdev->bHRMShutdown = (bool)shutdown;

	if(shutdown == 1) { // off
		pr_info("[SSP] %s: shut down(%d, %d)\n",__func__, regulator_is_enabled(sdev->reg_hrm1p8v),
			regulator_is_enabled(sdev->reg_hrm3p3v));

		gpio_direction_output(sdev->hrm_5v_onoff, 0);

		if(regulator_is_enabled(sdev->reg_hrm3p3v) && sdev->hrm_vender == 1/*AFE_ADI*/) {
			ssp_regulator_ctrl(sdev->reg_hrm3p3v, false, 0);
		}
		if(regulator_is_enabled(sdev->reg_hrm1p8v)) {
			ssp_regulator_ctrl(sdev->reg_hrm1p8v, false, 0);
		}
	} else if(shutdown == 0) { // on sequence 3.3V - 1.8V
		pr_info("[SSP] %s: power on (%d, %d)\n",__func__, regulator_is_enabled(sdev->reg_hrm1p8v),
			regulator_is_enabled(sdev->reg_hrm3p3v));

		gpio_direction_output(sdev->hrm_5v_onoff, 1);

		if(!regulator_is_enabled(sdev->reg_hrm3p3v)) {
			ssp_regulator_ctrl(sdev->reg_hrm3p3v, true, 0);			
		}
		if(!regulator_is_enabled(sdev->reg_hrm1p8v)) {
			ssp_regulator_ctrl(sdev->reg_hrm1p8v, true, 0);			
		}
	}	

	pr_info("[SSP] %s : HRM Shutdown : %d (%d)\n",__func__, sdev->bHRMShutdown, sdev->hrm_vender);
 
	return size;
}

static DEVICE_ATTR(mcu_rev, S_IRUGO, mcu_revision_show, NULL);
static DEVICE_ATTR(mcu_name, S_IRUGO, mcu_model_name_show, NULL);
static DEVICE_ATTR(mcu_update, S_IRUGO, mcu_update_kernel_bin_show, NULL);
static DEVICE_ATTR(mcu_update2, S_IRUGO,
	mcu_update_kernel_crashed_bin_show, NULL);
static DEVICE_ATTR(mcu_reset, S_IRUGO, mcu_reset_show, NULL);
static DEVICE_ATTR(mcu_sensorstate, S_IRUGO, mcu_sensor_state, NULL);
static DEVICE_ATTR(mcu_test, S_IRUGO | S_IWUSR | S_IWGRP,
	mcu_factorytest_show, mcu_factorytest_store);
static DEVICE_ATTR(mcu_sleep_test, S_IRUGO | S_IWUSR | S_IWGRP,
	mcu_sleep_factorytest_show, mcu_sleep_factorytest_store);

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	show_sensors_enable, set_sensors_enable);
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
static DEVICE_ATTR(accel_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_acc_delay, set_acc_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_32GSENSOR
static DEVICE_ATTR(accel32g_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_acc32g_delay, set_acc32g_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_LINEAR_ACCELEROMETER_SENSOR
static DEVICE_ATTR(linear_accel_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_linear_accel_delay, set_linear_accel_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
static DEVICE_ATTR(gyro_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gyro_delay, set_gyro_delay);
static DEVICE_ATTR(uncal_gyro_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncalib_gyro_delay, set_uncalib_gyro_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
#if 0
static DEVICE_ATTR(mag_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_delay, set_mag_delay);
static DEVICE_ATTR(uncal_mag_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncal_mag_delay, set_uncal_mag_delay);
#endif
static DEVICE_ATTR(mag_uncal_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_uncal_delay, set_mag_uncal_delay);
static DEVICE_ATTR(mag_calib_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_calib_delay, set_mag_calib_delay);
static DEVICE_ATTR(mag_power_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_power_delay, set_mag_power_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
static DEVICE_ATTR(pressure_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_pressure_delay, set_pressure_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
static DEVICE_ATTR(light_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_light_delay, set_light_delay);
static DEVICE_ATTR(auto_brightness_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_auto_brightness_delay, set_auto_brightness_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
static DEVICE_ATTR(temp_humid_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_temp_humi_delay, set_temp_humi_delay);
#endif
#ifdef CONFIG_SENSORS_ECG_LIB
static DEVICE_ATTR(ecg_lib_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_ecg_lib_delay, set_ecg_lib_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
static DEVICE_ATTR(rot_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_rot_delay, set_rot_delay);
static DEVICE_ATTR(game_rot_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_game_rot_delay, set_game_rot_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
static DEVICE_ATTR(hrm_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_delay, set_hrm_delay);
static DEVICE_ATTR(hrm_lib_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_lib_delay, set_hrm_lib_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
static DEVICE_ATTR(front_hrm_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_front_hrm_delay, set_front_hrm_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
static DEVICE_ATTR(uv_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uv_delay, set_uv_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
static DEVICE_ATTR(gsr_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gsr_delay, set_gsr_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
static DEVICE_ATTR(ecg_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_ecg_delay, set_ecg_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
static DEVICE_ATTR(grip_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_grip_delay, set_grip_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_TEMPERATURE_SENSOR
static DEVICE_ATTR(temperature_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_temperature_delay, set_temperature_delay);
#endif

static DEVICE_ATTR(set_cal_data, S_IWUSR | S_IWGRP,
	NULL, set_cal_data);
static DEVICE_ATTR(ssp_cmd_ctl, 0664, ssp_cmd_ctl_show, ssp_cmd_ctl_store);
static DEVICE_ATTR(sensor_reg_dump, S_IRUGO, sensor_reg_dump_show, NULL);
static DEVICE_ATTR(sensor_stuck_reg_dump, S_IRUGO | S_IWUSR | S_IWGRP, sensor_stuck_reg_dump_show, set_sensor_stuck_reg_dump);
static DEVICE_ATTR(sensorhub_crash_mini_dump, S_IRUGO, sensorhub_crash_mini_dump_show, NULL);
static DEVICE_ATTR(feature_list, S_IRUGO, feature_list_show, NULL);
static DEVICE_ATTR(ssr_stuck_info, S_IRUGO, ssr_stuck_info_show, NULL);
#ifdef CONFIG_SENSORS_SSP_BIA_SENSOR
static DEVICE_ATTR(bia_raw_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_bia_raw_delay, set_bia_raw_delay);
#endif
static DEVICE_ATTR(debug_enable, S_IRUGO | S_IWUSR | S_IWGRP, debug_enable_show, set_debug_enable);
static DEVICE_ATTR(scan_bytes, S_IRUGO, scan_bytes_show, NULL);
static DEVICE_ATTR(shub_dump, S_IRUGO, shub_dump_show, NULL);
static DEVICE_ATTR(dumpstate, S_IRUGO, dumpstate_show, NULL);
static DEVICE_ATTR(bugreport, S_IRUGO, bugreport_show, NULL);
static DEVICE_ATTR(debug_info, 0444, debug_info_show, NULL);
static DEVICE_ATTR(sensor_pos, S_IRUGO | S_IWUSR | S_IWGRP, sensor_position_show, sensor_position_store);
static DEVICE_ATTR(library_test, S_IWUSR | S_IWGRP, NULL, library_test_store);
static DEVICE_ATTR(ssp_flush, 0220, NULL, set_flush);
static DEVICE_ATTR(protocol_version, S_IRUGO, library_protocol_version_show, NULL);
static DEVICE_ATTR(hrm_shutdown, 0664, hrm_shutdown_show, hrm_shutdown_store);


static struct device_attribute *mcu_attrs[] = {
	&dev_attr_enable,
	&dev_attr_mcu_rev,
	&dev_attr_mcu_name,
	&dev_attr_mcu_test,
	&dev_attr_mcu_sensorstate,
	&dev_attr_mcu_reset,
	&dev_attr_mcu_update,
	&dev_attr_mcu_update2,
	&dev_attr_mcu_sleep_test,
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
	&dev_attr_accel_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_32GSENSOR
	&dev_attr_accel32g_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_LINEAR_ACCELEROMETER_SENSOR
	&dev_attr_linear_accel_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
	&dev_attr_gyro_poll_delay,
	&dev_attr_uncal_gyro_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
	&dev_attr_mag_uncal_poll_delay,
	&dev_attr_mag_calib_poll_delay,
	&dev_attr_mag_power_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
	&dev_attr_pressure_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
	&dev_attr_light_poll_delay,
	&dev_attr_auto_brightness_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
	&dev_attr_temp_humid_poll_delay,
#endif
#ifdef CONFIG_SENSORS_ECG_LIB
	&dev_attr_ecg_lib_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
	&dev_attr_rot_poll_delay,
	&dev_attr_game_rot_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
	&dev_attr_hrm_poll_delay,
	&dev_attr_hrm_lib_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
	&dev_attr_front_hrm_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
	&dev_attr_uv_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
	&dev_attr_gsr_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
	&dev_attr_ecg_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
	&dev_attr_grip_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_BIA_SENSOR
	&dev_attr_bia_raw_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_TEMPERATURE_SENSOR
	&dev_attr_temperature_poll_delay,
#endif
	&dev_attr_set_cal_data,
	&dev_attr_ssp_cmd_ctl,
	&dev_attr_sensor_reg_dump,
	&dev_attr_sensor_stuck_reg_dump,
	&dev_attr_sensorhub_crash_mini_dump,
	&dev_attr_feature_list,
	&dev_attr_ssr_stuck_info,
	&dev_attr_debug_enable,
	&dev_attr_scan_bytes,
	&dev_attr_shub_dump,
	&dev_attr_dumpstate,
	&dev_attr_bugreport,
	&dev_attr_debug_info,
	&dev_attr_sensor_pos,
	&dev_attr_library_test,
	&dev_attr_ssp_flush,
	&dev_attr_protocol_version,
	&dev_attr_hrm_shutdown,
	NULL,
};

static void initialize_mcu_factorytest(struct ssp_data *data)
{
	struct device *ret;

	//sensors_register(data->mcu_device, data, mcu_attrs, "ssp_sensor");
	ret = sensors_register(data->mcu_device, data, mcu_attrs, "ssp_sensor");
	data->mcu_device = ret;

	pr_err("[SSP] initialize_mcu_factorytest [0x%x] [0x%x]\n", data->mcu_device, ret);
}

static void remove_mcu_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->mcu_device, mcu_attrs);
}

static long ssp_batch_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct ssp_data *data
		= container_of(file->private_data,
			struct ssp_data, batch_io_device);

	struct batch_config batch;

	void __user *argp = (void __user *)arg;
	int retries = 2;
	int ret = 0;
	int sensor_type, ms_delay;
	int timeout_ms = 0;
	u8 uBuf[9];

	sensor_type = (cmd & 0xFF);

	if ((cmd >> 8 & 0xFF) != BATCH_IOCTL_MAGIC || sensor_type >= SENSOR_MAX) {
		pr_err("[SSP] Invalid BATCH CMD %x\n", cmd);
		return -EINVAL;
	}

	while (retries--) {
		ret = copy_from_user(&batch, argp, sizeof(batch));
		if (likely(!ret))
			break;
	}
	if (unlikely(ret)) {
		pr_err("[SSP] batch ioctl err(%d)\n", ret);
		return -EINVAL;
	}

	pr_info("[SSP] batch ioctl [arg] delay:%lld, timeout:%lld, flag:0x%0x\n",
		batch.delay, batch.timeout, batch.flag);

	ms_delay = get_msdelay(batch.delay);
	timeout_ms = div_s64(batch.timeout, 1000000);
	memcpy(&uBuf[0], &ms_delay, 4);
	memcpy(&uBuf[4], &timeout_ms, 4);
	uBuf[8] = batch.flag;

	/* For arg input checking */
	if ((batch.timeout < 0) || (batch.delay <= 0)) {
		pr_err("[SSP] Invalid batch values!!\n");
		return -EINVAL;
	} else if ((batch.timeout != 0) && (timeout_ms == 0)) {
		pr_err("[SSP] Invalid batch timeout range!!\n");
		return -EINVAL;
	} else if ((batch.delay != 0) && (ms_delay == 0)) {
		pr_err("[SSP] Invalid batch delay range!!\n");
		return -EINVAL;
	}

	if (batch.timeout) { /* add or dry */

		if (!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* real batch, NOT DRY, change delay */
			ret = 1;
			/* if sensor is not running state, enable will be called.
			   MCU return fail when receive chage delay inst during NO_SENSOR STATE */
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE) {
				ret = send_instruction_sync(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			}
			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = batch.delay;
			}
		} else { /* real batch, DRY RUN */
			ret = send_instruction_sync(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = batch.delay;
			}
		}
	} else { /* remove batch or normal change delay, remove or add will be called. */

		if (!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* no batch, NOT DRY, change delay */
			data->batchOptBuf[sensor_type] = 0;
			data->batchLatencyBuf[sensor_type] = 0;
			data->adDelayBuf[sensor_type] = batch.delay;
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE) {
				send_instruction(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			}
		}
	}

	pr_info("[SSP] batch %d: delay %lld(ms_delay:%d), timeout %lld(timeout_ms:%d), flag %d, ret %d\n",
		sensor_type, batch.delay, ms_delay, batch.timeout, timeout_ms, batch.flag, ret);
	if (!batch.timeout)
		return 0;
	if (ret <= 0)
		return -EINVAL;
	else
		return 0;
}

#ifdef CONFIG_COMPAT
static long ssp_batch_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return ssp_batch_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static struct file_operations ssp_batch_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = ssp_batch_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ssp_batch_compat_ioctl,
#endif
};

int initialize_sysfs(struct ssp_data *data)
{
	initialize_factorytest(data);

	initialize_mcu_factorytest(data);

	data->batch_io_device.minor = MISC_DYNAMIC_MINOR;
	data->batch_io_device.name = "batch_io";
	data->batch_io_device.fops = &ssp_batch_fops;
	if (misc_register(&data->batch_io_device))
		goto err_batch_io_dev;

	return SUCCESS;

err_batch_io_dev:
	remove_mcu_factorytest(data);
#if 0
	remove_factorytest(data);
#endif
	return ERROR;
}

void remove_sysfs(struct ssp_data *data)
{
	misc_deregister(&data->batch_io_device);
	remove_factorytest(data);
	remove_mcu_factorytest(data);
	destroy_sensor_class();
}
