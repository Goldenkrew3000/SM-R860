#include "zinitix_ztm730.h"

#ifdef ZINITIX_MISC_DEBUG
static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_misc_fops_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct raw_ioctl raw_ioctl;
	struct reg_ioctl reg_ioctl;
	static int m_ts_debug_mode = ZINITIX_DEBUG;
	u8 *u8Data;
	int ret = 0;
	size_t sz = 0;
	u16 mode = 0;
	u16 val;
	int nval = 0;
	u32 size;
	u8 *buff = NULL;
#if IS_ENABLED(CONFIG_COMPAT)
	void __user *argp = compat_ptr(arg);
#else
	void __user *argp = (void __user *)arg;
#endif
	if (misc_info == NULL) {
		input_err(true, &misc_info->client->dev, "%s:misc device NULL?\n", __func__);
		return -1;
	}

	switch (cmd) {
	case TOUCH_IOCTL_GET_DEBUGMSG_STATE:
		ret = m_ts_debug_mode;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_SET_DEBUGMSG_STATE:
		if (copy_from_user(&nval, argp, sizeof(nval))) {
			input_err(true, &misc_info->client->dev, "%s:copy_from_user\n", __func__);
			return -1;
		}
		if (nval)
			input_err(true, &misc_info->client->dev, "%s:on debug mode (%d)\n", __func__, nval);
		else
			input_err(true, &misc_info->client->dev, "%s:off debug mode (%d)\n", __func__, nval);
		m_ts_debug_mode = nval;
		break;

	case TOUCH_IOCTL_GET_CHIP_REVISION:
		ret = misc_info->cap_info.ic_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = misc_info->cap_info.fw_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = misc_info->cap_info.reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;

		input_info(true, &misc_info->client->dev, "%s:firmware size = %d\n", __func__, (int)sz);
		if (misc_info->cap_info.ic_fw_size != sz) {
			input_err(true, &misc_info->client->dev, "%s:firmware size error\n", __func__);
			return -1;
		}
		break;
/*
	case TOUCH_IOCTL_VARIFY_UPGRADE_DATA:
		if (copy_from_user(m_firmware_data,
			argp, info->cap_info.ic_fw_size))
			return -1;

		version = (u16) (m_firmware_data[52] | (m_firmware_data[53]<<8));

		input_err(true, &info->client->dev, "%s:firmware version = %x\n", __func__, version);

		if (copy_to_user(argp, &version, sizeof(version)))
			return -1;
		break;
*/
	case TOUCH_IOCTL_START_UPGRADE:
	{
		if (misc_info == NULL) {
			input_err(true, &misc_info->client->dev, "%s:misc device NULL?\n", __func__);
			return -1;
		}

		down(&misc_info->raw_data_lock);
		if (misc_info->update == 0) {
			up(&misc_info->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
			argp, sizeof(struct raw_ioctl))) {
			up(&misc_info->raw_data_lock);
			input_err(true, &misc_info->client->dev, "%s:copy_from_user(2)\n", __func__);
			return -1;
		}

		size = misc_info->cap_info.ic_fw_size;

		buff = kmalloc(size, GFP_KERNEL);

#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_from_user((u8 *)buff, compat_ptr(raw_ioctl.buf), size)) {
#else
		if (copy_from_user((u8 *)buff, (void __user *)(raw_ioctl.buf), size)) {
#endif
			misc_info->work_state = NOTHING;
			if (buff != NULL)
				kfree(buff);
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "%s:copy_from_user(3)\n", __func__);
			return -1;
		}

		ret = ztm730_upgrade_sequence(misc_info, (u8 *)buff);

		if (buff != NULL)
			kfree(buff);

		up(&misc_info->raw_data_lock);
		return ret;
	}

	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = misc_info->pdata->x_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = misc_info->pdata->y_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = misc_info->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = misc_info->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = misc_info->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(misc_info->irq);
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "%s:other process occupied.. (%d)\n",
				__func__, misc_info->work_state);
			enable_irq(misc_info->irq);
			up(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = HW_CALIBRAION;
		ztm730_delay(100);

		/* h/w calibration */
		if (ztm730_hw_calibration(misc_info))
			ret = 0;

		mode = misc_info->touch_mode;
		ret = ztm730_write_reg(misc_info->client, ZTM730_TOUCH_MODE, mode);
		if (ret != I2C_SUCCESS) {
			input_err(true, &misc_info->client->dev, "%s:failed to set touch mode %d.\n", __func__, mode);
			goto fail_hw_cal;
		}

		ret = ztm730_soft_reset(misc_info);
		if (ret) {
			input_err(true, &misc_info->client->dev, "%s:Failed to write reset command\n", __func__);
			goto fail_hw_cal;
		}

		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;
fail_hw_cal:

		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (misc_info == NULL) {
			input_err(true, &misc_info->client->dev, "%s:misc device NULL?\n", __func__);
			return -1;
		}
		if (copy_from_user(&nval, argp, 4)) {
			input_err(true, &misc_info->client->dev, "%s:copy_from_user\n", __func__);
			misc_info->work_state = NOTHING;
			return -1;
		}

		ret = ztm730_set_touchmode(misc_info, (u16)nval);
		if (ret) {
			input_err(true, &misc_info->client->dev, "%s:Failed to set POINT_MODE\n", __func__);
			misc_info->work_state = NOTHING;
			return -1;
		}

		return 0;

#ifdef ZINITIX_FILE_DEBUG
	case TOUCH_IOCTL_ZI_FILE_DEBUG_DISABLE:

		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "%s:other process occupied.. (%d)\n",
				__func__, misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl,
				argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "%s:TOUCH_IOCTL_ZI_FILE_DEBUG_DISABLE(1)\n", __func__);
			return -1;
		}

#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_from_user(&val, compat_ptr(reg_ioctl.val), sizeof(val))) {
#else
		if (copy_from_user(&val, (void __user *)(reg_ioctl.val), sizeof(val))) {
#endif
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "%s:TOUCH_IOCTL_ZI_FILE_DEBUG_DISABLE(2)\n", __func__);
			return -1;
		}

		g_zini_file_debug_disable = val;

		input_err(true, &misc_info->client->dev, "%s:TOUCH_IOCTL_ZI_FILE_DEBUG_DISABLE = %d\n", __func__, val);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return 0;
#endif

	case TOUCH_IOCTL_GET_REG:
		if (misc_info == NULL) {
			input_err(true, &misc_info->client->dev, "%s:misc device NULL?\n", __func__);
			return -1;
		}
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "%s:other process occupied.. (%d)\n",
				__func__, misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;

		if (copy_from_user(&reg_ioctl,
			argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "%s:copy_from_user(1)\n", __func__);
			return -1;
		}

		if (ztm730_read_data(misc_info->client,
			(u16)reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;


		nval = (int)val;

#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_to_user(compat_ptr(reg_ioctl.val), (u8 *)&nval, 4)) {
#else
		if (copy_to_user((void __user *)(reg_ioctl.val), (u8 *)&nval, 4)) {
#endif
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "%s:copy_to_user(2)\n", __func__);
			return -1;
		}

		input_err(true, &misc_info->client->dev, "%s:reg addr = 0x%x, val = 0x%x\n",
			__func__, reg_ioctl.addr, nval);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_SET_REG:

		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "%s:other process occupied.. (%d)\n",
				__func__, misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl,
				argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "%s:copy_from_user(1)\n", __func__);
			return -1;
		}

#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_from_user(&val, compat_ptr(reg_ioctl.val), sizeof(val))) {
#else
		if (copy_from_user(&val, (void __user *)(reg_ioctl.val), sizeof(val))) {
#endif
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "%s:copy_from_user(2)\n", __func__);
			return -1;
		}

		ret = ztm730_write_reg(misc_info->client, (u16)reg_ioctl.addr, val);
		if (ret != I2C_SUCCESS) {
			input_err(true, &misc_info->client->dev, "%s:failed to set touch mode %d.\n", __func__, mode);
			ret = -1;
		}

		input_err(true, &misc_info->client->dev, "%s:write: reg addr = 0x%x, val = 0x%x\n",
			__func__, reg_ioctl.addr, val);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_DONOT_TOUCH_EVENT:
		if (misc_info == NULL) {
			input_err(true, &misc_info->client->dev, "%s:misc device NULL?\n", __func__);
			return -1;
		}
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "%s:other process occupied.. (%d)\n",
				__func__, misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		ret = ztm730_write_reg(misc_info->client, ZTM730_INT_ENABLE_FLAG, 0);
		if (ret != I2C_SUCCESS) {
			input_err(true, &misc_info->client->dev,
				"%s:failed to set ZTM730_INT_ENABLE_FLAG.\n", __func__);
			ret = -1;
		}
		input_err(true, &misc_info->client->dev, "%s:write: reg addr = 0x%x, val = 0x0\n",
			__func__, ZTM730_INT_ENABLE_FLAG);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);

		return ret;

	case TOUCH_IOCTL_SEND_SAVE_STATUS:
		if (misc_info == NULL) {
			input_err(true, &misc_info->client->dev, "%s:misc device NULL?\n", __func__);
			return -1;
		}
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "%s:other process occupied..(%d)\n",
				__func__, misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = SET_MODE;
		ret = 0;

		ret = ztm730_write_cmd(misc_info->client, ZTM730_SAVE_STATUS_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &misc_info->client->dev,
				"%s:failed to write ZTM730_SAVE_STATUS_CMD\n", __func__);
			ret =  -1;
		}
		ztm730_delay(1000);	/* for fusing eeprom */

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);

		return ret;

	case TOUCH_IOCTL_GET_RAW_DATA:
		if (misc_info == NULL) {
			input_err(true, &misc_info->client->dev, "%s:TOUCH_IOCTL_GET_RAW_DATA (1)\n", __func__);
			return -1;
		}

		if (misc_info->touch_mode == TOUCH_POINT_MODE) {
			input_err(true, &misc_info->client->dev, "%s:TOUCH_IOCTL_GET_RAW_DATA (2)\n", __func__);
			return -1;
		}

		down(&misc_info->raw_data_lock);
		if (misc_info->update == 0) {
			up(&misc_info->raw_data_lock);
			input_err(true, &misc_info->client->dev, "%s:TOUCH_IOCTL_GET_RAW_DATA (3)\n", __func__);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
			argp, sizeof(struct raw_ioctl))) {
			up(&misc_info->raw_data_lock);
			input_err(true, &misc_info->client->dev, "%s:TOUCH_IOCTL_GET_RAW_DATA (4)\n", __func__);
			return -1;
		}

		misc_info->update = 0;

		u8Data = (u8 *)&misc_info->cur_data[0];
		if (raw_ioctl.sz > MAX_TRAW_DATA_SZ * 2)	//36x22 + 4x2 + 2 = 802
			raw_ioctl.sz = MAX_TRAW_DATA_SZ * 2;

#ifdef ZINITIX_FILE_DEBUG
		if (g_zini_file_debug_disable)
			g_zini_raw_data_size = raw_ioctl.sz;
#endif

#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_to_user(compat_ptr(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#else
		if (copy_to_user((void __user *)(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#endif
			up(&misc_info->raw_data_lock);
			input_err(true, &misc_info->client->dev,
				"%s:TOUCH_IOCTL_GET_RAW_DATA (5)\n", __func__);
			return -1;
		}

		up(&misc_info->raw_data_lock);

		return 0;

	default:
		break;
	}
	return 0;
}

const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
	.compat_ioctl = ts_misc_fops_ioctl,
};

struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};
#endif

int read_fw_verify_result(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	s32 ret;
	u16 val;

	ret = ztm730_read_data(info->client,
		ZTM730_CHECKSUM_RESULT, (u8 *)&val, CHECKSUM_VAL_SIZE);
	if (ret < 0) {
		input_err(true, &client->dev,
			"%s:Failed to read CHECKSUM_RESULT register\n", __func__);
		goto out;
	}

	if (val != ZTM730_CHECK_SUM) {
		input_err(true, &client->dev,
			"%s:Failed to check ZTM730_CHECKSUM_RESULT[0x%04x]\n", __func__, val);
		goto out;
	}

	ret = ztm730_read_data(info->client,
		ZTM730_FW_CHECKSUM_REG, (u8 *)&val, CHECKSUM_VAL_SIZE);
	if (ret < 0) {
		input_err(true, &client->dev,
			"%s:Failed to read ZTM730_FW_CHECKSUM_VAL register\n", __func__);
		goto out;
	}

	info->checksum = val;
out:
	return ret;
}

int ztm730_upgrade_sequence(struct ztm730_info *info, const u8 *firmware_data)
{
	struct i2c_client *client = info->client;
	bool ret;

	disable_irq(info->irq);
	down(&info->work_lock);
	info->work_state = UPGRADE;

	ztm730_clear_report_data(info);

	input_info(true, &client->dev, "%s:start upgrade firmware\n", __func__);

	ret = ztm730_upgrade_firmware(info, firmware_data, info->cap_info.ic_fw_size);
	if (!ret)
		input_err(true, &client->dev, "%s:Failed update firmware\n", __func__);

	ztm730_init_touch(info);

	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);

	return (ret) ? 0 : -1;
}

bool ztm730_set_touchmode(struct ztm730_info *info, u16 value)
{
	int i, ret = 0;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "%s:other process occupied.\n", __func__);
		enable_irq(info->irq);
		up(&info->work_lock);
		return -1;
	}

	info->work_state = SET_MODE;

	if (value == TOUCH_SEC_MODE)
		info->touch_mode = TOUCH_POINT_MODE;
	else
		info->touch_mode = value;

	input_info(true, &info->client->dev, "%s:%d\n",
			__func__, info->touch_mode);

	if (info->touch_mode == TOUCH_CND_MODE) {
		ret = ztm730_write_reg(info->client, ZTM730_M_U_COUNT, SEC_M_U_COUNT);
		if (ret != I2C_SUCCESS) {
			input_err(true, &info->client->dev,
				"%s:Fail to set U Count [%d]\n",
				__func__, info->touch_mode);

				goto out;
		}

		ret = ztm730_write_reg(info->client, ZTM730_M_N_COUNT, SEC_M_N_COUNT);
		if (ret != I2C_SUCCESS) {
			input_err(true, &info->client->dev,
				"%s:Fail to set N Count [%d]\n",
				__func__, info->touch_mode);

				goto out;
		}

		ret = ztm730_write_reg(info->client, ZTM730_AFE_FREQUENCY, SEC_M_FREQUENCY);
		if (ret != I2C_SUCCESS) {
			input_err(true, &info->client->dev,
				"%s:Fail to set AFE Frequency [%d]\n",
				__func__, info->touch_mode);

				goto out;
		}

		ret = ztm730_write_reg(info->client, ZTM730_M_RST0_TIME, SEC_M_RST0_TIME);
		if (ret != I2C_SUCCESS) {
			input_err(true, &info->client->dev,
				"%s:Fail to set RST0 Time [%d]\n",
				__func__, info->touch_mode);

				goto out;
		}
	}

	ret = ztm730_write_reg(info->client, ZTM730_TOUCH_MODE, info->touch_mode);
	if (ret != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
			"%s:Fail to set ZINITX_TOUCH_MODE [%d]\n",
			__func__, info->touch_mode);

			goto out;
	}

	ret = ztm730_soft_reset(info);
	if (ret) {
		input_err(true, &info->client->dev,
			"%s:Failed to write reset command\n", __func__);
		goto out;
	}
	ztm730_delay(400);

	/* clear garbage data */
	for (i = 0; i <= INT_CLEAR_RETRY; i++) {
		ztm730_delay(20);
		ret = ztm730_write_cmd(info->client, ZTM730_CLEAR_INT_STATUS_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &info->client->dev,
				"%s:Failed to clear garbage data[%d/INT_CLEAR_RETRY]\n", __func__, i);

		}
		usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	}

out:
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return ret;
}

static ssize_t support_feature_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	u32 features = 0;

	features |= (1 << 0);

	input_info(true, &info->client->dev, "%s: festures: 0x%X\n", __func__, features);

	return snprintf(buf, PAGE_SIZE, "%d", features);
}

ssize_t enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__, info->enabled);
	return scnprintf(buf, PAGE_SIZE, "%d", info->enabled);
}

ssize_t enabled_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	int value;
	int err;

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		input_info(true, &info->client->dev, "%s: invalid string: %d\n", __func__, err);
		return err;
	}
	//temp
	input_info(true, &info->client->dev, "%s: val:%d, enabled:%d\n", __func__, value, info->enabled);

#if 0
	if (!info->info_work_done) {
		input_info(true, &info->client->dev, "%s: not finished info work\n", __func__);
		return count;
	}
#endif

	if (info->enabled == value) {
		input_info(true, &info->client->dev, "%s: already %s\n", __func__, value ? "enabled" : "disabled");
		return count;
	}

	if (value > 0) {
		ztm730_input_open(info->input_dev);
		ztm730_bezel_open(info->input_dev);
		info->enabled = 1;
	} else {
		ztm730_input_close(info->input_dev);
		ztm730_bezel_close(info->input_dev);
		info->enabled = 0;
	}

//	info->enabled = value;

	return count;
}

static DEVICE_ATTR(support_feature, 0444, support_feature_show, NULL);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);

static struct attribute *sysfs_attributes[] = {
	&dev_attr_support_feature.attr,
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group sysfs_attr_group = {
	.attrs = sysfs_attributes,
};

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	struct zxt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	char fw_path[SEC_TS_MAX_FW_PATH+1];
	char result[16] = {0};
	const struct firmware *tsp_fw = NULL;
	int ret;
#if 0
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP) && IS_ENABLED(CONFIG_SPU_VERIFY)
	int ori_size;
	int spu_ret;
#endif
#endif

	sec_cmd_set_default_result(sec);

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(result, sizeof(result), "NG");
		sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		if (!pdata->fw_name) {
			input_err(true, &client->dev, "%s: firmware_name is NULL\n", __func__);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
		}

		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", pdata->fw_name);

		ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
		if (ret) {
			input_err(true, &client->dev,
				"%s: Firmware image %s not available\n", __func__, fw_path);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
 		}

		ret = ztm730_upgrade_sequence(info, tsp_fw->data);
		release_firmware(tsp_fw);
		if (ret < 0) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
		}
		break;
	case UMS:
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_PATH_EXTERNAL_FW);
#else
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_PATH_EXTERNAL_FW_SIGNED);
#endif
		ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
		if (ret) {
			input_err(true, &client->dev,
				"%s: Firmware image %s not available\n", __func__, fw_path);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
		}

#if 0
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP) && IS_ENABLED(CONFIG_SPU_VERIFY)
		ori_size = tsp_fw->size - SPU_METADATA_SIZE(TSP);

		spu_ret = spu_firmware_signature_verify("TSP", tsp_fw->data, tsp_fw->size);
		if (ori_size != spu_ret) {
			input_err(true, &client->dev, "%s: signature verify failed, ori:%d, fsize:%ld\n",
					__func__, ori_size, tsp_fw->size);
			release_firmware(tsp_fw);
			goto err;
		}
#endif
#endif
		ret = ztm730_upgrade_sequence(info, tsp_fw->data);
		release_firmware(tsp_fw);
		if (ret < 0) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
		}
		break;
	default:
		input_err(true, &client->dev, "%s: invalid fw file type!!\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(result, sizeof(result), "%s", "NG");
		goto err;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(result, sizeof(result), "%s", "OK");

err:
	sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

int read_fw_ver_bin(struct ztm730_info *info)
{
	const struct firmware *tsp_fw = NULL;
	char fw_path[SEC_TS_MAX_FW_PATH];
	int ret;

	if (info->fw_data == NULL) {
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", info->pdata->fw_name);
		ret = request_firmware(&tsp_fw, fw_path, &info->client->dev);
		if (ret) {
			input_err(true, &info->client->dev, "%s:failed to request_firmware %s\n",
						__func__, fw_path);
			return -1;
		}
		info->fw_data = (unsigned char *)tsp_fw->data;
	}

	info->img_version_of_bin[0] = (u16)(info->fw_data[68] | (info->fw_data[69] << 8));
	info->img_version_of_bin[1] = (u16)(info->fw_data[56] | (info->fw_data[57]<<8));
	info->img_version_of_bin[2] = (u16)(info->fw_data[48] | (info->fw_data[49] << 8));
	info->img_version_of_bin[3] = (u16)(info->fw_data[60] | (info->fw_data[61] << 8));

	release_firmware(tsp_fw);
	info->fw_data = NULL;

	return 0;
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[16] = { 0 };
	u32 version;
	int ret;

	sec_cmd_set_default_result(sec);

	ret = read_fw_ver_bin(info);
	if (ret) {
		input_err(true, &info->client->dev, "%s: binary fw info is not read\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto NG;
	}

	version = (u32)(info->img_version_of_bin[0] << 24) | (info->img_version_of_bin[1] << 16) |
			(info->img_version_of_bin[2] << 8) | info->img_version_of_bin[3];

	snprintf(buff, sizeof(buff), "ZI%08X", version);
	sec->cmd_state = SEC_CMD_STATUS_OK;

NG:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[16] = { 0 };
	char model[16] = { 0 };
	u16 vendor_id;
	u32 version, length;
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = ztm730_ic_version_check(info);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: firmware version read error\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	vendor_id = ntohs(info->cap_info.vendor_id);
	version = (u32)((u32)(info->cap_info.ic_revision & 0xff) << 24)
		| ((info->cap_info.fw_minor_version & 0xff) << 16)
		| ((info->cap_info.hw_id & 0xff) << 8) | (info->cap_info.reg_data_version & 0xff);

	length = sizeof(vendor_id);
	snprintf(buff, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(buff + length, sizeof(buff) - length, "%08X", version);
	snprintf(model, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(model + length, sizeof(model) - length, "%04X", version >> 16);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", ZTM730_VENDOR_NAME);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	strncpy(buff, "ZTM730", sizeof(buff));
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[16] = { 0 };
	u16 threshold = 0;
	int ret;


	sec_cmd_set_default_result(sec);

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	disable_irq(info->irq);
	ret = ztm730_read_data(info->client, ZTM730_CONNECTED_REG, (char *)&threshold, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s:Failed to read ZTM730_CONNECTED_REG\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	if (threshold >= TSP_CONNECTED_VALID) {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "OK");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	}

	input_info(true, &info->client->dev, "%s:trehshold = %d\n", __func__, threshold);

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	enable_irq(info->irq);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.x_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.y_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

#ifndef CONFIG_SEC_FACTORY
static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	info->aot_enable = !!sec->cmd_param[0];

	mutex_lock(&info->modechange);

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF && info->aot_enable == 1) {
		ztm730_ts_start(info);
		ztm730_ts_set_lowpowermode(info, TO_LOWPOWER_MODE);
	} else if (info->power_state == SEC_INPUT_STATE_LPM && info->aot_enable == 0) {
		ztm730_ts_stop(info);
	}

	mutex_unlock(&info->modechange);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);
}
#endif

static void bezel_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->bezel_enable = sec->cmd_param[0];
		if (info->bezel_enable)
			zinitix_bit_set(info->optional_mode, DEF_OPTIONAL_MODE_WHEEL_ON_BIT);
		else
			zinitix_bit_clr(info->optional_mode, DEF_OPTIONAL_MODE_WHEEL_ON_BIT);

		ret = ztm730_set_optional_mode(info, true);
		if (ret) {
			input_err(true, &info->client->dev,
				"%s:failed ztm730_set_optional_mode\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if (val)
		zinitix_bit_clr(info->optional_mode, DEF_OPTIONAL_MODE_EDGE_SELECT);
	else
		zinitix_bit_set(info->optional_mode, DEF_OPTIONAL_MODE_EDGE_SELECT);

	ztm730_set_optional_mode(info, false);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s cmd_param: %d\n", __func__, buff, sec->cmd_param[0]);
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	struct i2c_client *client = info->client;

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &client->dev, "%s: %d%s\n", __func__, sec->item_count,
			sec->cmd_result_all);
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ztm730_info *info = container_of(sec, struct ztm730_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->debug_flag = sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},

#ifndef CONFIG_SEC_FACTORY
	{SEC_CMD_H("aot_enable", aot_enable),},
#endif
	{SEC_CMD_H("bezel_enable", bezel_enable),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int init_sec_factory(struct ztm730_info *info)
{
	int retval = 0;

	retval = sec_cmd_init(&info->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&info->sec.fac_dev->kobj,
			&sysfs_attr_group);
	if (retval < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		goto exit;
	}

	retval = sysfs_create_link(&info->sec.fac_dev->kobj,
			&info->input_dev->dev.kobj, "input");
	if (retval < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to create input symbolic link\n",
				__func__);
		goto exit;
	}


#ifdef ZINITIX_MISC_DEBUG
	misc_info = info;

	retval = misc_register(&touch_misc_device);
	if (retval) {
		input_err(true, &info->client->dev, "%s:Failed to register touch misc device\n", __func__);
		goto exit;
	}
#endif

	return 0;

exit:
	return retval;
}

void remove_sec_factory(struct ztm730_info *info)
{
	input_err(true, &info->client->dev, "%s\n", __func__);

	sysfs_remove_link(&info->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&info->sec.fac_dev->kobj,
			&sysfs_attr_group);

#ifdef ZINITIX_MISC_DEBUG
	misc_deregister(&touch_misc_device);
#endif
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
}