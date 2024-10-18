/*
 *
 * Zinitix ztm730 touchscreen driver
 *
 * Copyright (C) 2018 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */

#include "zinitix_ztm730.h"

void ztm730_delay(unsigned int ms)
{
	if (ms <= 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

s32 ztm730_read_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return -ENODEV;
	}

	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		info->comm_err_cnt++;
		input_err(true, &client->dev, "%s:failed to send. reg=0x%04x, ret:%d, try:%d\n",
				__func__, reg, ret, count + 1);
		return ret;
	}
	usleep_range(DELAY_FOR_TRANSCATION, DELAY_FOR_TRANSCATION);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		info->comm_err_cnt++;
		input_err(true, &client->dev, "%s:failed to recv. ret:%d\n", __func__, ret);
		return ret;
	}

	if (info->debug_flag & ZINITIX_TS_DEBUG_PRINT_I2C_READ_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: R:", __func__);
		for (i = 0; i < length; i++)
			pr_cont(" %02X", values[i]);
		pr_cont("\n");
	}

	return length;
}

#if TOUCH_POINT_MODE
s32 ztm730_read_data_only(struct i2c_client *client, u8 *values, u16 length)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	s32 ret;

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return -ENODEV;
	}

	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		info->comm_err_cnt++;
		input_err(true, &client->dev, "%s:failed to recv. ret:%d\n", __func__, ret);
		return ret;
	}
	usleep_range(DELAY_FOR_TRANSCATION, DELAY_FOR_TRANSCATION);

	if (info->debug_flag & ZINITIX_TS_DEBUG_PRINT_I2C_READ_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: R:", __func__);
		for (i = 0; i < length; i++)
			pr_cont(" %02X", values[i]);
		pr_cont("\n");
	}

	return length;
}
#endif

s32 ztm730_write_nvm_data(struct i2c_client *client)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	const u8 pkt[10] = {0x02, 0xCC, 0x1C, 0x00, 0x1F,0x00, 0xD0, 0x3B, 0x00, 0x00} ;
	s32 ret;


	// CC02 001C 001F 3BD0 0000

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return -ENODEV;
	}

	ret = i2c_master_send(client , pkt , 10);
	if (ret < 0) {
		info->comm_err_cnt++;
		input_err(true, &client->dev, "%s:failed to send(%d)\n",
					__func__, ret);
		return ret;
	}

	return 0;
}

s32 ztm730_write_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	u8 pkt[10];
	s32 ret;
	int count = 0;

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return -ENODEV;
	}

	pkt[0] = (reg) & 0xff;
	pkt[1] = (reg >> 8) & 0xff;
	memcpy((u8 *)&pkt[2], values, length);

	ret = i2c_master_send(client , pkt , length + 2);
	if (ret < 0) {
		info->comm_err_cnt++;
		input_err(true, &client->dev, "%s:failed to send. reg= 0x%04x, data= 0x%02x, ret:%d, try:%d\n",
					__func__, reg, *values, ret, count + 1);
		return ret;
	}

	if (info->debug_flag & ZINITIX_TS_DEBUG_PRINT_I2C_WRITE_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: W:", __func__);
		for (i = 0; i < length + ZINITIX_REG_ADDR_LENGTH; i++)
			pr_cont(" %02X", pkt[i]);
		pr_cont("\n");
	}

	return length;
}

s32 ztm730_write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	s32 ret;

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return -ENODEV;
	}

	ret = ztm730_write_data(client, reg, (u8 *)&value, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:failed to write reg 0x%04x. ret:%d\n",
			__func__, reg, ret);
		return I2C_FAIL;
	}

	return I2C_SUCCESS;
}

s32 ztm730_write_cmd(struct i2c_client *client, u16 reg)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	s32 ret;

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return -ENODEV;
	}

	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		info->comm_err_cnt++;
		input_err(true, &client->dev, "%s:failed to send reg 0x%04x. ret:%d\n",
			__func__, reg, ret);
		return ret;
	}

	if (info->debug_flag & ZINITIX_TS_DEBUG_PRINT_I2C_WRITE_CMD)
		pr_info("[sec_input] zinitix: %s: W: %04X\n", __func__, reg);

	return I2C_SUCCESS;
}

s32 ztm730_read_raw_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	s32 ret;

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return -ENODEV;
	}

	ret = ztm730_write_cmd(client, reg);
	if (ret  != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to send reg 0x%04x.\n",
			__func__, reg);
		return ret;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		info->comm_err_cnt++;
		input_err(true, &client->dev, "%s:failed to recv. ret:%d\n",
			__func__, ret);
		return ret;
	}

	if (info->debug_flag & ZINITIX_TS_DEBUG_PRINT_I2C_READ_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: R: %04X R:", __func__, reg);
		for (i = 0; i < length; i++)
			pr_cont(" %02X", values[i]);
		pr_cont("\n");
	}

	return length;
}

int  ztm730_send_deep_sleep_cmd(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	input_info(true, &client->dev, "%s\n", __func__);

	ret = ztm730_write_cmd(info->client, ZTM730_CLEAR_INT_STATUS_CMD);
	if (ret) {
		input_err(true, &client->dev,
			"%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n",
			__func__);
		return -1;
	}

	ret = ztm730_write_cmd(info->client, ZTM730_DEEP_SLEEP_CMD);
	if (ret) {
		input_err(true, &client->dev,
			"%s:failed to write ZTM730_DEEP_SLEEP_CMD\n",
			__func__);
		return -1;
	}

	return 0;
}

int  ztm730_write_wakeup_cmd(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret, i;

	input_info(true, &client->dev, "%s\n", __func__);

	for (i= 0; i < INIT_RETRY_CNT; i++) {
		ret = ztm730_write_cmd(info->client, ZTM730_WAKEUP_CMD);
		if (ret) {
			input_err(true, &client->dev,
				"%s:failed to write ZTM730_WAKEUP_CMD\n", __func__);
		}
		usleep_range(DELAY_FOR_POST_TRANSCATION,
			DELAY_FOR_POST_TRANSCATION);
	}
	msleep(11);

	return 0;
}

int ztm730_set_optional_mode(struct ztm730_info *info, bool force)
{
	int ret;

	if (info->m_prev_optional_mode == info->optional_mode && !force)
		return 0;

	ret = ztm730_write_reg(info->client, ZTM730_OPTIONAL_SETTING, info->optional_mode);
	if (ret != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
			"%s:Failed to write ZTM730_OPTIONAL_SETTING(%d)\n",
			__func__, info->optional_mode);
		return -I2C_FAIL;
	}

	info->m_prev_optional_mode = info->optional_mode;
	input_dbg(true, &info->client->dev, "%s:optional_mode[0x%04x]\n", __func__, info->optional_mode);
	return 0;
}

#ifdef USE_TSP_TA_CALLBACKS
void ztm730_chip_reset(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	down(&info->work_lock);
	info->device_enabled = false;
	info->work_state = SUSPEND;
	disable_irq(info->irq);
	ztm730_clear_report_data(info);

	ztm730_power_control(info, POWER_OFF);
	msleep(POWER_ON_DELAY);
	ztm730_power_control(info, POWER_ON);
	ztm730_power_control(info, POWER_ON_SEQUENCE);

	ret = ztm730_mini_init_touch(info);
	if (!ret) {
		input_err(true, &client->dev,
			"%s:Failed to mini_init_touch\n", __func__);
	}

	enable_irq(info->irq);
	info->device_enabled = true;
	info->work_state = NOTHING;
	up(&info->work_lock);
}

void ztm730_set_ta_status(struct ztm730_info *info)
{
	int ret;

	if (info->ta_connected)
		zinitix_bit_set(info->optional_mode,
			DEF_OPTIONAL_MODE_USB_DETECT_BIT);
	else
		zinitix_bit_clr(info->optional_mode,
			DEF_OPTIONAL_MODE_USB_DETECT_BIT);

	ret = ztm730_set_optional_mode(info, false);
	if (ret != I2C_SUCCESS)
		input_err(true, &client->dev, "%s:failed to set optional_mode\n", __func__);
}

void ztm730_ts_charger_status_cb(struct tsp_callbacks *cb, int status)
{
	struct ztm730_info *info =
		container_of(cb, struct ztm730_info, callbacks);
	struct i2c_client *client = info->client;

	info->ta_connected = !!status;

	if ((info->device_enabled) && (!info->gesture_enable) &&
	   (info->work_state == NOTHING)) {
		if (status) {
			ztm730_set_ta_status(info);
			input_info(true, &client->dev, "%s:TA %s.  device enable.\n", __func__,
				info->ta_connected ? "connected": "disconnected");
		} else {
			input_info(true, &client->dev,
				"%s:TA disconnected. TSP reset start\n", __func__);
			ztm730_chip_reset(info);
		}
	} else
		input_info(true, &client->dev, "%s:TA %sconnected. device disable.\n",
			__func__, info->ta_connected ? "": "dis");

}

extern struct tsp_callbacks *ztm730_charger_callbacks;
void ztm730_register_callback(struct tsp_callbacks *cb)
{
	ztm730_charger_callbacks = cb;
	pr_info("[sec_input] %s\n", __func__);
}
#endif

void ztm730_get_dqa_data(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	/* checksum */
	ret = read_fw_verify_result(info);
	if (ret < 0) {
		input_err(true, &client->dev,
			"%s:Failed to read checksum register\n", __func__);
	}
}

#ifdef ZINITIX_FILE_DEBUG
/* zinitix_rawdata_store */
#define ZINITIX_DEFAULT_UMS    "/sdcard/zinitix_rawdata.txt"
#define FILE_BUFF_SIZE	1500	// more than [(x_num * y_num + 32)*6 + 1]
void ztm730_store_raw_data(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
#ifdef SEC_FACTORY_TEST
//	struct tsp_factory_info *finfo = info->factory_info;
#endif
	int ret = 0;
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0;
	char buff[FILE_BUFF_SIZE];
	int i;
	char tmp[10];
	u32 total_node = info->cap_info.total_node_num;

	if (g_zini_file_debug_disable) return;

	/* Check for buffer overflow */
	if (total_node > (FILE_BUFF_SIZE/6 - 34)) {
		input_err(true, &client->dev, "%s: buffer overflow\n", __func__);
		return;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(ZINITIX_DEFAULT_UMS, O_RDWR, S_IRUSR | S_IWUSR);

	if (IS_ERR(fp)) {
		input_err(true, &client->dev, "%s:file(%s)\n",
			__func__, ZINITIX_DEFAULT_UMS);
		fp = filp_open(ZINITIX_DEFAULT_UMS, O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	}
	if (IS_ERR(fp)) {
		input_err(true, &client->dev, "%s:file(%s)\n",
			__func__, ZINITIX_DEFAULT_UMS);
#ifdef SEC_FACTORY_TEST
//		finfo->cmd_state = FAIL;
#endif
		set_fs(old_fs);
		return;
	}
	fsize = fp->f_path.dentry->d_inode->i_size;
	ret = default_llseek(fp, fsize, SEEK_SET);

	memset(buff, 0x0, FILE_BUFF_SIZE);
	for (i = 0; i < (total_node + 32); i++) {
		sprintf(tmp, "%05d\t", info->cur_data[i]);
		strncat(buff, tmp, strlen(tmp));
	}
	sprintf(tmp, "\n");
	strncat(buff, tmp, strlen(tmp));
	vfs_write(fp, (char __user *)buff, (total_node+32)*6+1, &fp->f_pos);
	filp_close(fp, current->files);
	set_fs(old_fs);
	input_err(true, &client->dev, "%s:rawdata stored.\n", __func__);

	return;
}
#endif

bool ztm730_get_raw_data(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	u32 total_node = info->cap_info.total_node_num;
	int sz;
	int i, ret;
	u16 temp_sz;

	if (down_trylock(&info->raw_data_lock)) {
		input_err(true, &client->dev, "%s:Failed to occupy sema\n", __func__);
		info->touch_info.status = 0;
		return true;
	}

#ifdef ZINITIX_FILE_DEBUG
	if (g_zini_file_debug_disable && g_zini_raw_data_size > 0)
		sz = (int)g_zini_raw_data_size;
	else
		sz = total_node * 2 + sizeof(struct point_info);
#else
	sz = total_node * 2 + sizeof(struct point_info);
#endif

	for (i = 0; sz > 0; i++) {
		temp_sz = I2C_BUFFER_SIZE;

		if (sz < I2C_BUFFER_SIZE)
			temp_sz = sz;

		ret = ztm730_read_raw_data(info->client, ZTM730_RAWDATA_REG + i,
			(char *)((u8*)(info->cur_data)+ (i * I2C_BUFFER_SIZE)), temp_sz);
		if (ret < 0) {
			input_err(true, &client->dev, "%s:Failed to read raw data\n", __func__);
			up(&info->raw_data_lock);
			return false;
		}
		sz -= I2C_BUFFER_SIZE;
	}

	info->update = 1;
#ifdef ZINITIX_FILE_DEBUG
	/* zinitix_rawdata_store */
	if (g_zini_file_debug_disable == 0)
	{
		ztm730_store_raw_data(info);
		info->update = 0;

		memcpy((u8 *)(&info->touch_info),
				(u8 *)&info->cur_data[total_node], sizeof(struct point_info));		
	}
	else
	{
		if (info->touch_mode == TOUCH_PROCESS_MODE || info->touch_mode == TOUCH_HYBRID_MODE)
		{
			u32 y_node = info->cap_info.y_node_num;
			u32 x_node = info->cap_info.x_node_num;

			memcpy((u8 *)(&info->touch_info),
					(u8 *)&info->cur_data[total_node + y_node + y_node + x_node],
					sizeof(struct point_info));
		}
		else
		{
			memcpy((u8 *)(&info->touch_info),
					(u8 *)&info->cur_data[total_node], sizeof(struct point_info));
		}
	}
#else
	memcpy((u8 *)(&info->touch_info),
			(u8 *)&info->cur_data[total_node], sizeof(struct point_info));
#endif


			
	up(&info->raw_data_lock);

	return true;
}

bool ztm730_clear_interrupt(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	ret = zinitix_bit_test(info->touch_info.status, BIT_MUST_ZERO);
	if (ret) {
		input_err(true, &client->dev, "%s:Invalid must zero bit(%04x)\n",
			__func__, info->touch_info.status);
		return -1;
	}

	ret = ztm730_write_cmd(info->client, ZTM730_CLEAR_INT_STATUS_CMD);
	if (ret) {
		input_err(true, &client->dev,
			"%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n",
			__func__);
		return -1;
	}

	return 0;
}

bool ztm730_ts_read_coord(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;
#if TOUCH_POINT_MODE
	int i;
#endif

	/* for  Debugging Tool */
	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (info->update == 0) {
			if (!ztm730_get_raw_data(info))
				return false;
		} else
			info->touch_info.status = 0;
		input_err(true, &client->dev, "%s:status = 0x%04X\n", __func__, info->touch_info.status);
		goto out;
	}

#if TOUCH_POINT_MODE
	memset(&info->touch_info, 0x0, sizeof(struct point_info));

	ret = ztm730_read_data_only(info->client, (u8 *)(&info->touch_info), 10);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:error read point info using i2c.-\n", __func__);
		return false;
	}

	if (info->touch_info.event_flag == 0 || info->touch_info.status == 0) {
		ret = ztm730_set_optional_mode(info, false);
		if (ret < 0) {
			input_err(true, &client->dev, "%s:error set_optional_mode.\n", __func__);
			return false;
		}
		ret = ztm730_read_data(info->client, ZTM730_OPTIONAL_SETTING, (u8 *)&status, 2);
		if (ret < 0) {
			input_err(true, &client->dev, "%s:error read noise mode.-\n", __func__);
			return false;
		}
		ret = ztm730_write_cmd(info->client, ZTM730_CLEAR_INT_STATUS_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
		}
		return true;
	}

	for (i = 1; i < info->cap_info.multi_fingers; i++) {
		if (zinitix_bit_test(info->touch_info.event_flag, i)) {
			usleep_range(20, 20);
			if (ztm730_read_data(info->client, ZTM730_POINT_STATUS_REG + 2 + (i * 4),
				(u8 *)(&info->touch_info.coord[i]), sizeof(struct coord)) < 0) {
				input_err(true, &client->dev, "%s:error read point info\n", __func__);
				return false;
			}
		}
	}

#else   /* TOUCH_POINT_MODE */
	ret = ztm730_read_data(info->client, ZTM730_POINT_STATUS_REG,
			(u8 *)(&info->touch_info), sizeof(struct point_info));
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed to read point info\n", __func__);
		return false;
	}
#endif	/* TOUCH_POINT_MODE */
	ret = ztm730_set_optional_mode(info, false);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:error set optional_mode.\n", __func__);
		return false;
	}

	if (zinitix_bit_test(info->touch_info.fw_status, DEF_DEVICE_STATUS_LPM))
		info->lpm_mode = true;
	else
		info->lpm_mode = false;

	if (zinitix_bit_test(info->touch_info.fw_status, DEF_DEVICE_STATUS_WATER_MODE)) {
		if (!info->wet_mode)
			info->wet_mode_cnt++;
		info->wet_mode = true;
	} else
		info->wet_mode = false;

	if (zinitix_bit_test(info->touch_info.fw_status, DEF_DEVICE_STATUS_NOISE_MODE)) {
		if (!info->noise_mode)
			info->noise_mode_cnt++;
		info->noise_mode = true;
	} else
		info->noise_mode = false;

	return true;

out:
	ret = ztm730_clear_interrupt(info);
	if (ret) {
		input_err(true, &client->dev, "%s:failed clear interrupt.\n", __func__);
		return false;
	}

	return true;
}

void set_gesture_mode(struct ztm730_info *info, bool enable)
{
	info->gesture_enable = enable;
	if (info->tsp_dev)
		sysfs_notify(&info->tsp_dev->kobj, NULL, "touch_active");
}

bool ztm730_power_sequence(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int retry = 0, ret;
	u16 chip_code;

retry_power_sequence:

	ret = ztm730_write_reg(client, 0xc000, 0x0001);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed to send power sequence(vendor cmd enable)\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	ret = ztm730_read_data(client, 0xcc00, (u8 *)&chip_code, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed to read chip code\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	ret = ztm730_write_cmd(client, 0xc004);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed to send power sequence(0xc004)\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	ret = ztm730_write_reg(client, 0xc002, 0x0001);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed to send power sequence(nvm init)\n", __func__);
		goto fail_power_sequence;
	}
	ztm730_delay(2);

	ret = ztm730_write_reg(client, 0xc001, 0x0001);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed to send power sequence(program start)\n", __func__);
		goto fail_power_sequence;
	}
	ztm730_delay(FIRMWARE_ON_DELAY);	/* wait for checksum cal */

	return true;

fail_power_sequence:
	input_err(true, &client->dev, "%s:fail_power_sequence\n", __func__);
	if (retry++ < 3) {
		ztm730_delay(CHIP_ON_DELAY);
		input_err(true, &client->dev, "%s:retry = %d\n", __func__, retry);
		goto retry_power_sequence;
	}

	input_err(true, &client->dev, "%s:Failed to send power sequence\n", __func__);
	return false;
}

int ztm730_regulator_init(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	struct zxt_ts_platform_data *pdata = info->pdata;

	if (!pdata->avdd_ldo)
		goto vddo;

	if (info->avdd == NULL) {
		info->avdd = regulator_get(&info->client->dev, pdata->avdd_ldo);
		if (IS_ERR_OR_NULL(info->avdd)) {
			input_err(true, &client->dev,
				"%s:could not get avdd_ldo, ret = %d\n",
				__func__, IS_ERR(info->avdd));
			return -EINVAL;
		}
	}

vddo:
	if (!pdata->dvdd_ldo)
		goto out;

	if (info->vddo == NULL) {
		info->vddo = regulator_get(&info->client->dev, pdata->dvdd_ldo);
		if (IS_ERR_OR_NULL(info->vddo)) {
			input_err(true, &client->dev,
				"%s:could not get dvdd_ldo, ret = %d\n",
				__func__, IS_ERR(info->vddo));
			return -EINVAL;
		}
	}

out:
	return 0;
}

int ztm730_soft_reset(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	input_dbg(true, &client->dev, "%s\n", __func__);

	ret = ztm730_write_cmd(client, ZTM730_SWRESET_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev,
			"%s:failed soft reset command[%d]\n", __func__, ret);
		goto out;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION,
		DELAY_FOR_POST_TRANSCATION);

out:
	return ret;
}

int ztm730_gpio_reset(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	struct zxt_ts_platform_data	*pdata = info->pdata;
	int ret = 0;

	if (!gpio_is_valid(pdata->gpio_reset)) {
		input_err(true, &client->dev, "%s:Invalid gpio_reset gpio [%d]\n", __func__, pdata->gpio_reset);
		return -ENXIO;
	}

	input_info(true, &client->dev, "%s:gpio_reset[%s]\n", __func__,
				gpio_get_value(pdata->gpio_reset) ? "high":"low");

	ret = gpio_request_one(pdata->gpio_reset, GPIOF_OUT_INIT_LOW, "ztm730_gpio_reset");
	if (ret < 0) {
		input_err(true, &client->dev, "%s:failed to request gpio_reset\n", __func__);
		return -EINVAL;
	}
	msleep(RESET_DELAY);

	input_dbg(true, &client->dev, "%s:gpio_reset[%s]\n", __func__,
				gpio_get_value(pdata->gpio_reset) ? "high":"low");

	ret = gpio_direction_input(pdata->gpio_reset);
	if (ret) {
		gpio_free(pdata->gpio_reset);
		input_err(true, &client->dev, "%s:unable to set_direction for gpio_reset [%d]\n",
			__func__, pdata->gpio_reset);
		return -ENODEV;
	}
	gpio_free(pdata->gpio_reset);
	msleep(RESET_DELAY);

	input_dbg(true, &client->dev, "%s:gpio_reset[%s]\n", __func__,
				gpio_get_value(pdata->gpio_reset) ? "high":"low");

	return ret;
}

int ztm730_hard_reset(struct ztm730_info *info)
{
	bool temp_gesture_enable = info->gesture_enable;

	set_gesture_mode(info, false);

	disable_irq_nosync(info->irq);

	ztm730_clear_report_data(info);

	if (!ztm730_power_control(info, POWER_ON_SEQUENCE))
		goto err_out;

	set_gesture_mode(info, false);

	enable_irq(info->irq);

	set_gesture_mode(info, temp_gesture_enable);

	return 0;

err_out:
	enable_irq(info->irq);
	return -ENXIO;
}

bool ztm730_regulator_on(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	input_info(true, &info->client->dev, "%s\n", __func__);

	if (info->avdd) {
		ret = regulator_enable(info->avdd);
		if (ret) {
			input_err(true, &client->dev, "%s:avdd enable failed(%d)\n", __func__, ret);
			return -EINVAL;
		}
	}

	if (info->vddo) {
		ret = regulator_enable(info->vddo);
		if (ret) {
			input_err(true, &client->dev, "%s:vddo enable failed(%d)\n", __func__, ret);
			return -EINVAL;
		}
	}

	ztm730_delay(CHIP_ON_DELAY);
	return 0;
}

bool ztm730_regulator_off(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	input_info(true, &info->client->dev, "%s\n", __func__);

	if (info->vddo) {
		ret = regulator_disable(info->vddo);
		if (ret) {
			input_err(true, &client->dev, "%s:vddo disable failed(%d)\n", __func__, ret);
			return -EINVAL;
		}
	}

	if (info->avdd) {
		ret = regulator_disable(info->avdd);
		if (ret) {
			input_err(true, &client->dev, "%s:avdd disable failed(%d)\n", __func__, ret);
			return -EINVAL;
		}
	}

	ztm730_delay(LPM_OFF_DELAY);
	return 0;
}

bool ztm730_power_control(struct ztm730_info *info, u8 ctl)
{
	int ret = 0;

	if (ctl == POWER_ON) {
		ret = ztm730_regulator_on(info);
		if (ret)
			return false;
		info->power_state = SEC_INPUT_STATE_POWER_ON;
	} else if (ctl == POWER_OFF) {
		ret = ztm730_regulator_off(info);
		if (ret)
			return false;
		info->power_state = SEC_INPUT_STATE_POWER_OFF;
	} else if (ctl == POWER_ON_SEQUENCE) {
		ret = ztm730_gpio_reset(info);
		if (ret) {
			input_err(true, &info->client->dev,
				"%s:Failed to ztm730_gpio_reset\n",
				__func__);
			return false;
		}

		ret = ztm730_power_sequence(info);
		if (!ret) {
			input_err(true, &info->client->dev,
				"%s:Failed to ztm730_power_sequence\n",
				__func__);
			return false;
		}
	}

	return true;
}

bool ztm730_check_need_upgrade(struct ztm730_info *info,
	u16 cur_version, u16 cur_minor_version, u16 cur_reg_version, u16 cur_hw_id)
{
	u16	new_version;
	u16	new_minor_version;
	u16	new_reg_version;
#if CHECK_HWID
	u16	new_hw_id;
#endif

	if (info->fw_data == NULL) {
		input_err(true, &info->client->dev, "%s:fw_data is NULL\n", __func__);
		return false;
	}

	new_version = (u16) (info->fw_data[52] | (info->fw_data[53]<<8));
	new_minor_version = (u16) (info->fw_data[56] | (info->fw_data[57]<<8));
	new_reg_version = (u16) (info->fw_data[60] | (info->fw_data[61]<<8));

#if CHECK_HWID
	new_hw_id = (u16) (info->fw_data[48] | (info->fw_data[49]<<8));
	input_info(true, &info->client->dev, "cur HW_ID = 0x%x, new HW_ID = 0x%x\n",
							cur_hw_id, new_hw_id);
	if (cur_hw_id != new_hw_id)
		return false;
#endif

	input_info(true, &info->client->dev, "%s:cur version = 0x%x, new version = 0x%x\n",
							__func__, cur_version, new_version);
	input_info(true, &info->client->dev, "%s:cur minor version = 0x%x, new minor version = 0x%x\n",
						__func__, cur_minor_version, new_minor_version);
	input_info(true, &info->client->dev, "%s:cur reg data version = 0x%x, new reg data version = 0x%x\n",
						__func__, cur_reg_version, new_reg_version);
	if (info->cal_mode) {
		input_info(true, &info->client->dev, "%s:didn't update TSP F/W in CAL MODE\n", __func__);
		return false;
	}

	if (info->pdata->bringup == 3 && (cur_version != new_version ||
			cur_minor_version != new_minor_version || cur_reg_version != new_reg_version))
		return true;

	if (cur_reg_version == 0xffff)
		return true;
	if (cur_version > 0xFF)
		return true;

	if (cur_version < new_version)
		return true;
	else if (cur_version > new_version)
		return false;

	if (cur_minor_version < new_minor_version)
		return true;
	else if (cur_minor_version > new_minor_version)
		return false;

	if (cur_reg_version < new_reg_version)
		return true;

	return false;
}

bool ztm730_upgrade_firmware(struct ztm730_info *info, const u8 *firmware_data, u32 size)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	u8 *verify_data;
	int i, ret;
	int page_sz = 128;
	u16 chip_code;
	int retry_cnt = 0;

	input_info(true, &client->dev, "%s\n", __func__);
	if (!firmware_data) {
		input_err(true, &client->dev, "%s:firmware is NULL\n", __func__);
		return false;
	}

	verify_data = devm_kzalloc(&client->dev, size, GFP_KERNEL);
	if (!verify_data) {
		input_err(true, &client->dev,
			"%s:cannot alloc verify buffer\n", __func__);
		return false;
	}

	ztm730_power_control(info, POWER_OFF);
	msleep(POWER_ON_DELAY);
	ztm730_power_control(info, POWER_ON);

retry_upgrade:
	ret = ztm730_gpio_reset(info);
	if (ret) {
		input_err(true, &info->client->dev,
			"%s:Failed to ztm730_gpio_reset\n",
			__func__);
		return false;
	}

	ret = ztm730_write_cmd(client, 0x01D5);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:power sequence error (Enter FU mode)\n", __func__);
		goto fail_upgrade;
	}
	ztm730_delay(11);

	ret = ztm730_write_reg(client, 0xc000, 0x0001);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:power sequence error (vendor cmd enable)\n", __func__);
		goto fail_upgrade;
	}
	usleep_range(10, 10);

	ret = ztm730_read_data(client, 0xcc00, (u8 *)&chip_code, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:failed to read chip code\n", __func__);
		goto fail_upgrade;
	}
	input_info(true, &client->dev, "%s:chip code:[0x%04x]\n", __func__, chip_code);
	usleep_range(10, 10);

	ret = ztm730_write_cmd(client, 0xc004);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev,
			"%s:power sequence error (intn clear)\n", __func__);
		goto fail_upgrade;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	ret = ztm730_write_reg(client, 0xc002, 0x0001);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:power sequence error (nvm init)\n", __func__);
		goto fail_upgrade;
	}
	ztm730_delay(5);

	input_info(true, &client->dev, "%s:init flash\n", __func__);
	ret = ztm730_write_reg(client, 0xc003, 0x0001);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write nvm vpp on\n", __func__);
		goto fail_upgrade;
	}


///add nvm timing
	ret = ztm730_write_reg(client, 0xC011, 0x0080);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to set osc sel\n", __func__);
		goto fail_upgrade;
	}
	ztm730_delay(5);

	ret = ztm730_write_nvm_data(client);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to set nvm timing\n", __func__);
		goto fail_upgrade;
	}
	ztm730_delay(5);//5ms

////

	ret = ztm730_write_cmd(client, ZTM730_INIT_FLASH);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to init flash\n", __func__);
		goto fail_upgrade;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	// Mass Erase
	ret = ztm730_write_cmd(client, 0x01DF);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to mass erase\n", __func__);
		goto fail_upgrade;
	}
	ztm730_delay(100);

	ret = ztm730_write_reg(client, 0x01DE, 0x0001);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to enter upgrade mode\n", __func__);
		goto fail_upgrade;
	}
	ztm730_delay(1);

	ret = ztm730_write_reg(client, 0x01D3, 0x0008);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to init upgrade mode\n", __func__);
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "%s:writing firmware data\n", __func__);
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz/TC_SECTOR_SZ; i++) {
			if (ztm730_write_data(client, ZTM730_WRITE_FLASH,
						(u8 *)&firmware_data[flash_addr], TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "%s:write zinitix tc firmware\n", __func__);
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
			usleep_range(100, 100);
		}
		ztm730_delay(8);	/*for fuzing delay*/
	}

	ret = ztm730_write_reg(client, 0xc003, 0x0000);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:nvm write vpp off\n", __func__);
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "%s:init flash\n", __func__);
	ret = ztm730_write_cmd(client, ZTM730_INIT_FLASH);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to init flash\n", __func__);
		goto fail_upgrade;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	input_info(true, &client->dev, "%s:read firmware data\n", __func__);
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz/TC_SECTOR_SZ; i++) {
			if (ztm730_read_raw_data(client, ZTM730_READ_FLASH,
						(u8 *)&verify_data[flash_addr], TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "%s:Failed to read firmware\n", __func__);
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
		}
	}

	/* verify */
	input_info(true, &client->dev, "%s:verify firmware data\n", __func__);
	ret = memcmp((u8 *)&firmware_data[0], (u8 *)&verify_data[0], size);
	if (!ret) {
		input_info(true, &client->dev, "%s:upgrade finished\n", __func__);
		devm_kfree(&client->dev, verify_data);

		ztm730_power_control(info, POWER_OFF);
		msleep(POWER_ON_DELAY);
		ztm730_power_control(info, POWER_ON);
		ztm730_power_control(info, POWER_ON_SEQUENCE);

		ret = ztm730_mini_init_touch(info);
		if (!ret) {
			input_err(true, &client->dev,
				"%s:Failed to mini_init_touch\n", __func__);
		}

		ztm730_delay(200);
		return true;
	} else
		input_err(true, &client->dev, "%s:Failed to verify firmware\n", __func__);

fail_upgrade:
	ztm730_power_control(info, POWER_OFF);
	msleep(POWER_ON_DELAY);
	ztm730_power_control(info, POWER_ON);

	if (retry_cnt++ < INIT_RETRY_CNT) {
		input_err(true, &client->dev, "upgrade failed: so retry... (%d)\n", retry_cnt);
		goto retry_upgrade;
	}
	devm_kfree(&client->dev, verify_data);
	input_info(true, &client->dev, "%s:Failed to upgrade\n", __func__);

	return false;
}

bool ztm730_hw_calibration(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	u16 chip_eeprom_info;
	int ret, time_out = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	ret = ztm730_write_reg(client, ZTM730_TOUCH_MODE, 0x07);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_TOUCH_MODE\n", __func__);
		return false;
	}
	ztm730_delay(10);

	ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
	}
	ztm730_delay(10);

	ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
	}
	ztm730_delay(50);

	ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
	}
	ztm730_delay(10);

	ret = ztm730_write_cmd(client, ZTM730_CALIBRATE_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_CALIBRATE_CMD\n", __func__);
		return false;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
		return false;
	}
	ztm730_delay(10);

	ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	/* wait for h/w calibration*/
	do {
		ztm730_delay(500);
		ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
		}
		usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

		ret = ztm730_read_data(client, ZTM730_EEPROM_INFO_REG, (u8 *)&chip_eeprom_info, 2);
		if (ret < 0) {
			input_err(true, &client->dev, "%s:failed to write ZTM730_EEPROM_INFO_REG\n", __func__);
			return false;
		}
		input_dbg(true, &client->dev, "touch eeprom info = 0x%04X\n", chip_eeprom_info);

		ret = zinitix_bit_test(chip_eeprom_info, 0);
		if (!ret)
			break;

		if (time_out++ == 4) {
			ret = ztm730_write_cmd(client, ZTM730_CALIBRATE_CMD);
			if (ret != I2C_SUCCESS) {
				input_err(true, &client->dev, "%s:failed to write ZTM730_CALIBRATE_CMD\n", __func__);
			}
			ztm730_delay(10);

			ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
			if (ret != I2C_SUCCESS) {
				input_err(true, &client->dev,
					"%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
			}
			usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
			input_err(true, &client->dev, "%s:h/w calibration retry timeout.\n", __func__);
		}

		if (time_out++ > 10) {
			input_err(true, &client->dev, "%s:h/w calibration timeout.\n", __func__);
			break;
		}
	} while (true);

	ret = ztm730_write_reg(client, ZTM730_TOUCH_MODE, TOUCH_POINT_MODE);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write TOUCH_POINT_MODE\n", __func__);
		return false;
	}

	if (info->cap_info.ic_int_mask) {
		ret = ztm730_write_reg(client, ZTM730_INT_ENABLE_FLAG, info->cap_info.ic_int_mask);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s:failed to write ZTM730_INT_ENABLE_FLAG\n", __func__);
			return false;
		}
	}
	usleep_range(100, 100);

	ret = ztm730_write_cmd(client, ZTM730_SAVE_CALIBRATION_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_SAVE_CALIBRATION_CMD\n", __func__);
		return false;
	}
	ztm730_delay(1000);

	return true;
}

int ztm730_get_ic_fw_size(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	u16 chip_code = 0;
	int ret = 0;

	ret = ztm730_read_data(client, 0xcc00, (u8 *)&chip_code, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed to read chip code\n", __func__);
		return ret;
	}
	info->chip_code = chip_code;
	input_info(true, &client->dev, "%s:chip code = 0x%x\n", __func__, info->chip_code);

	if (info->chip_code == ZTM730_IC_CHIP_CODE)
		cap->ic_fw_size = ZTM730_IC_FW_SIZE;
	else {
		input_err(true, &client->dev, "%s:Failed to get ic_fw_size\n", __func__);
		ret = -1;
	}

	return ret;
}

#if USE_CHECKSUM
bool crc_check(struct ztm730_info *info)
{
	u16 chip_check_sum;
	int ret;

	ret = ztm730_read_data(info->client, ZTM730_CHECKSUM_RESULT, (u8 *)&chip_check_sum, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s:Failed to read CHECKSUM_RESULT register\n", __func__);
		return false;
	}

	if (chip_check_sum != ZTM730_CHECK_SUM)
		return false;

	return true;
}
#endif

int ztm730_fw_update_work(struct ztm730_info *info, bool force_update)
{
	const struct firmware *tsp_fw = NULL;
	struct zxt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	char fw_path[SEC_TS_MAX_FW_PATH];
	bool need_update = false;
	int ret;

	if (pdata->bringup == 1) {
		input_info(true, &info->client->dev, "%s: bringup\n", __func__);
		return 0;
	}

	if (!pdata->fw_name) {
		input_err(true, &info->client->dev, "%s: firmware_name is NULL\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s: start\n", __func__);

	snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", info->pdata->fw_name);
	ret = request_firmware(&tsp_fw, fw_path, &info->client->dev);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s:failed to request_firmware %s\n",
			__func__, fw_path);
		goto fw_request_fail;
	}

	info->fw_data = (unsigned char *)tsp_fw->data;

	ret = ztm730_get_ic_fw_size(info);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed to get ic_fw_size\n", __func__);
		goto fw_request_fail;
	}

	need_update = ztm730_check_need_upgrade(info, cap->fw_version,
			cap->fw_minor_version, cap->reg_data_version, cap->hw_id);
#if USE_CHECKSUM
	if (!need_update) {
		if (!crc_check(info))
			need_update = true;
	}
#endif

	if (need_update == true || force_update == true) {
		ret = ztm730_upgrade_firmware(info, info->fw_data, cap->ic_fw_size);
		if (!ret) {
			input_err(true, &client->dev, "%s:Failed ztm730_upgrade_firmware[%d]\n", __func__, ret);
			ret = -1;
			goto fw_request_fail;
		}

		ret = ztm730_ic_version_check(info);
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: failed ic version check\n", __func__);
	}

	return true;

fw_request_fail:
	release_firmware(tsp_fw);
	info->fw_data = NULL;
	return ret;

}

int ztm730_ic_version_check(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	int ret = 0;

	ret = ztm730_read_data(client, ZTM730_CHIP_REVISION, (u8 *)&cap->ic_revision, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:failed to read chip revision\n", __func__);
		goto error;
	}

	ret = ztm730_read_data(client, ZTM730_HW_ID, (u8 *)&cap->hw_id, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed to read ZTM730_HW_ID\n", __func__);
		goto error;
	}

	ret = ztm730_read_data(client, ZTM730_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed ZTM730_FIRMWARE_VERSION[%d]\n", __func__, ret);
		goto error;
	}

	ret = ztm730_read_data(client, ZTM730_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed ZTM730_MINOR_FW_VERSION[%d]\n", __func__, ret);
		goto error;
	}

	ret = ztm730_read_data(client, ZTM730_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed ZTM730_DATA_VERSION_REG[%d]\n", __func__, ret);
		goto error;
	}

error:
	return ret;
}

bool ztm730_init_touch(struct ztm730_info *info)
{
	struct zxt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	int i,  ret;
	u16 reg_val;
	u16 chip_eeprom_info;

	info->lpm_mode = 0;
	info->wet_mode = 0;
	info->noise_mode = 0;
	info->ref_scale_factor = TSP_INIT_TEST_RATIO;

	for (i = 0; i < INIT_RETRY_CNT; i++) {
		if (ztm730_read_data(client, ZTM730_EEPROM_INFO_REG,
						(u8 *)&chip_eeprom_info, 2) < 0) {
			input_err(true, &client->dev, "%s:Failed to read eeprom info(%d)\n", __func__, i);
			ztm730_delay(10);
			continue;
		} else
			break;
	}
	input_info(true, &client->dev, "%s:ZTM730_EEPROM_INFO_REG:[0x%x]\n", __func__, chip_eeprom_info);

	if (i == INIT_RETRY_CNT)
		goto fail_init;

	ret = ztm730_soft_reset(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to write reset command\n", __func__);
		goto fail_init;
	}

	reg_val = 0;
	zinitix_bit_set(reg_val, BIT_PT_CNT_CHANGE);
	zinitix_bit_set(reg_val, BIT_DOWN);
	zinitix_bit_set(reg_val, BIT_MOVE);
	zinitix_bit_set(reg_val, BIT_UP);

#if SUPPORTED_PALM_TOUCH
	zinitix_bit_set(reg_val, BIT_PALM);
	zinitix_bit_set(reg_val, BIT_PALM_REJECT);
#endif

	cap->ic_int_mask = reg_val;
	ret = ztm730_write_reg(client, ZTM730_INT_ENABLE_FLAG, 0x0);
	if (ret != I2C_SUCCESS)
		goto fail_init;

	ret = ztm730_soft_reset(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to write reset command\n", __func__);
		goto fail_init;
	}

	/* get chip information */
	ret = ztm730_read_data(client, ZTM730_VENDOR_ID, (u8 *)&cap->vendor_id, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:failed to read vendor id\n", __func__);
		goto fail_init;
	}

	ret = ztm730_read_data(client, ZTM730_TOTAL_NUMBER_OF_X, (u8 *)&cap->x_node_num, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed ZTM730_TOTAL_NUMBER_OF_X[%d]\n", __func__, ret);
		goto fail_init;
	}

	ret = ztm730_read_data(client, ZTM730_TOTAL_NUMBER_OF_Y, (u8 *)&cap->y_node_num, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:Failed ZTM730_TOTAL_NUMBER_OF_Y[%d]\n", __func__, ret);
		goto fail_init;
	}

	cap->total_node_num = cap->x_node_num * cap->y_node_num;

	input_info(true, &client->dev, "%s: x_num:%d, y_num:%d, total_num:%d\n",
		__func__, cap->x_node_num, cap->y_node_num, cap->total_node_num);

#ifdef ZINITIX_FILE_DEBUG
	//legacy source raw data size
	g_zini_raw_data_size = cap->total_node_num * 2 + sizeof(struct point_info);
#endif

	ret = zinitix_bit_test(chip_eeprom_info, 0);
	if (ret) {
		/* disable chip interrupt */
		ret = ztm730_write_reg(client, ZTM730_INT_ENABLE_FLAG, 0);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s:Failed ZTM730_INT_ENABLE_FLAG[%d]\n", __func__, ret);
			goto fail_init;
		}
	}

	/* initialize */
	ret = ztm730_write_reg(client, ZTM730_X_RESOLUTION, (u16)pdata->x_resolution + ABS_PT_OFFSET);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed ZTM730_X_RESOLUTION[%d]\n", __func__, ret);
		goto fail_init;
	}

	ret = ztm730_write_reg(client, ZTM730_Y_RESOLUTION, (u16)pdata->y_resolution + ABS_PT_OFFSET);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed ZTM730_Y_RESOLUTION[%d]\n", __func__, ret);
		goto fail_init;
	}

	cap->MinX = (u32)0;
	cap->MinY = (u32)0;
	cap->MaxX = (u32)pdata->x_resolution;
	cap->MaxY = (u32)pdata->y_resolution;

	ret = ztm730_write_reg(client, ZTM730_SUPPORTED_FINGER_NUM, (u16)MAX_SUPPORTED_FINGER_NUM);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed ZTM730_SUPPORTED_FINGER_NUM[%d]\n", __func__, ret);
		goto fail_init;
	}

	cap->multi_fingers = MAX_SUPPORTED_FINGER_NUM;
	cap->gesture_support = 0;

	ret = ztm730_write_reg(client, ZTM730_INITIAL_TOUCH_MODE, TOUCH_POINT_MODE);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed ZTM730_INITIAL_TOUCH_MODE[%d]\n", __func__, ret);
		goto fail_init;
	}

	ret = ztm730_write_reg(client, ZTM730_TOUCH_MODE, info->touch_mode);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:Failed ZTM730_TOUCH_MODE[%d]\n", __func__, ret);
		goto fail_init;
	}

#ifdef USE_TSP_TA_CALLBACKS
	ztm730_set_ta_status(info);
#endif

	ret = ztm730_set_optional_mode(info, true);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to set optional_mode\n", __func__);
		goto fail_init;
	}

	ret = ztm730_write_reg(client, ZTM730_INT_ENABLE_FLAG, cap->ic_int_mask);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_INT_ENABLE_FLAG\n", __func__);
		goto fail_init;
	}

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev,
				"%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD[%d]\n", __func__, i);
		}
		usleep_range(10, 10);
	}

	if (info->touch_mode != TOUCH_POINT_MODE) { /* Test Mode */
		ret = ztm730_write_reg(client, ZTM730_DELAY_RAW_FOR_HOST, RAWDATA_DELAY_FOR_HOST);
		if ( ret != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s:Failed to set DELAY_RAW_FOR_HOST\n", __func__);
			goto fail_init;
		}
	}

	set_gesture_mode(info, false);

	input_info(true, &client->dev, "%s:initialize done!\n", __func__);

	return true;

fail_init:
	return false;
}

int ztm730_mode_change_recovery(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	ret = ztm730_hard_reset(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to hard reset\n", __func__);
		return -1;
	}

	ret = ztm730_mini_init_touch(info);
	if (!ret) {
		input_err(true, &client->dev, "%s:Failed to initialize\n", __func__);
		return -1;
	}

	return 0;
}

#if 0
int ztm730_hard_reset_recovery(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	input_info(true, &client->dev, "%s+++\n", __func__);

	ret = ztm730_hard_reset(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to hard reset\n", __func__);
		return -1;
	}

	ret = ztm730_init_touch(info);
	if (!ret) {
		input_err(true, &client->dev, "%s:Failed to initialize\n", __func__);
		return -1;
	}

	info->hard_recovery_cnt++;

	input_info(true, &client->dev, "%s---\n", __func__);

	return 0;
}
#endif

bool ztm730_mini_init_touch(struct ztm730_info *info)
{
	struct capa_info *cap_info = &info->cap_info;
	struct i2c_client *client = info->client;
	int i, ret;
#if USE_CHECKSUM
	u16 chip_check_sum;

	for (i = 0; i < INIT_RETRY_CNT; i++) {
		ret = ztm730_read_data(client, ZTM730_CHECKSUM_RESULT, (u8 *)&chip_check_sum, 2);
		if (ret >= 0) {
			if (chip_check_sum == ZTM730_CHECK_SUM) {
				break;
			} else
				input_err(true, &client->dev, "%s: Invalid chip_check_sum [0x%04x]\n",
					__func__, chip_check_sum);
		} else
			input_err(true, &client->dev, "%s:Failed read CHECKSUM_RESULT register\n", __func__);
		msleep(1);
	}

	if (i == INIT_RETRY_CNT) {
		input_err(true, &client->dev, "%s:Failed retry CHECKSUM_RESULT register\n", __func__);
		goto fail_mini_init;
	}
#endif

	info->lpm_mode = 0;
	info->wet_mode = 0;
	info->noise_mode = 0;
	info->ref_scale_factor = TSP_INIT_TEST_RATIO;

	ret = ztm730_soft_reset(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to write reset command\n", __func__);
		goto fail_mini_init;
	}

	/* initialize */
	ret = ztm730_write_reg(client, ZTM730_TOUCH_MODE, info->touch_mode);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_TOUCH_MODE\n", __func__);
		goto fail_mini_init;
	}

#ifdef USE_TSP_TA_CALLBACKS
	ztm730_set_ta_status(info);
#endif
	ret = ztm730_set_optional_mode(info, true);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to set optional_mode\n", __func__);
		goto fail_mini_init;
	}

	/* soft calibration */
	ret = ztm730_write_cmd(client, ZTM730_CALIBRATE_CMD);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_CALIBRATE_CMD\n", __func__);
		goto fail_mini_init;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	ret = ztm730_write_reg(client, ZTM730_INT_ENABLE_FLAG, info->cap_info.ic_int_mask);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed to write ZTM730_INT_ENABLE_FLAG\n", __func__);
		goto fail_mini_init;
	}

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
			goto fail_mini_init;
		}
		usleep_range(10, 10);
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	ret = ztm730_read_data(client, 0x0308, (u8 *)&chip_check_sum, 2);
	if (ret >= 0) {
		input_err(true, &client->dev, "%s: 0x0308:[0x%04x]\n", __func__, chip_check_sum);
	} else {
		input_err(true, &client->dev, "%s:Failed to read 0x0308 register\n", __func__);
		goto fail_mini_init;
	}

	ret = ztm730_read_data(client, 0x0115, (u8 *)&chip_check_sum, 2);
	if (ret >= 0) {
		input_err(true, &client->dev, "%s: 0x0115:[0x%04x]\n", __func__, chip_check_sum);
	} else {
		input_err(true, &client->dev, "%s:Failed to read 0x0115 register\n", __func__);
		goto fail_mini_init;
	}

	if (info->touch_mode != TOUCH_POINT_MODE) {
		ret = ztm730_write_reg(client, ZTM730_DELAY_RAW_FOR_HOST, RAWDATA_DELAY_FOR_HOST);
		if ( ret  != I2C_SUCCESS){
			input_err(true, &client->dev, "%s:Failed to set ZTM730_DELAY_RAW_FOR_HOST\n", __func__);
			goto fail_mini_init;
		}
	}

	set_gesture_mode(info, false);

	input_info(true, &client->dev, "%s:firmware version: 0x%02x%02x%02x\n", __func__,
				cap_info->fw_version,
				cap_info->fw_minor_version,
				cap_info->reg_data_version);

	input_dbg(true, &client->dev, "%s:Successfully mini initialized\n", __func__);

	return true;

fail_mini_init:
	input_info(true, &client->dev, "%s:Failed to initialize mini init\n", __func__);
	return false;
}

void ztm730_clear_report_data(struct ztm730_info *info)
{
	int i, ret;

	input_info(true, &info->client->dev, "%s\n", __func__);

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		ret = zinitix_bit_test(info->reported_touch_info.coord[i].sub_status, SUB_BIT_EXIST);
		if (ret) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &info->client->dev, "[RA] tID:%d dd:%d,%d mc:%d lx:%d ly:%d p:%d\n",
					i, info->touch_info.coord[i].x - info->event[i].x,
					info->touch_info.coord[i].y - info->event[i].y,
					info->event[i].move_cnt,
					info->touch_info.coord[i].x, info->touch_info.coord[i].y,
					info->palm_flag);
#else
			input_info(true, &info->client->dev, "[RA] tID:%d dd:%d,%d mc:%d p:%d\n",
					i, info->touch_info.coord[i].x - info->event[i].x,
					info->touch_info.coord[i].y - info->event[i].y,
					info->event[i].move_cnt,
					info->palm_flag);
#endif
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR, 0);

			input_report_abs(info->input_dev, ABS_MT_POSITION_X,
				info->reported_touch_info.coord[i].x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y,
				info->reported_touch_info.coord[i].x);
//			input_report_abs(info->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_sync(info->input_dev);
		}
		info->reported_touch_info.coord[i].sub_status = 0;
		info->event[i].move_cnt = 0;
	}

	info->finger_cnt = 0;

	// Reset keys
	input_report_key(info->input_dev, BTN_TOUCH, 0);
	input_report_key(info->input_dev, KEY_SLEEP, false);
	input_report_key(info->input_dev, ABS_MT_PALM, false);

	input_mt_report_pointer_emulation(info->input_dev, false);
	input_mt_slot(info->input_dev, 0);
	input_sync(info->input_dev);

#if SUPPORTED_PALM_TOUCH
	info->palm_flag = false;
#endif
}

void  ztm730_send_wakeup_event(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;

	input_info(true, &client->dev, "%s\n", __func__);

	input_mt_slot(info->input_dev, 0);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, 100);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, 100);
	input_report_key(info->input_dev, BTN_TOUCH, 1);
	input_sync(info->input_dev);

	msleep(20);

	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
	input_report_key(info->input_dev, BTN_TOUCH, 0);
	input_mt_report_pointer_emulation(info->input_dev, false);
	input_sync(info->input_dev);

	info->aod_touch_cnt++;
}

irqreturn_t ztm730_touch_work(int irq, void *data)
{
	struct ztm730_info *info = (struct ztm730_info *)data;
	struct i2c_client *client = info->client;
#if SUPPORTED_TOUCH_WHEEL
	struct ztm_bezel_ddata	*bezel_info = info->bezel_info;
#endif
	int i = 0, ret = 0, t;
	u32 x, y, w, maxX, maxY;
	u8 palm = 0, prev_sub_status, sub_status;

	if (info->power_state == SEC_INPUT_STATE_LPM) {
		/* run lpm interrupt handler */
		__pm_wakeup_event(info->wakelock, 500);

		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&info->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &info->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return IRQ_HANDLED;
		} else if (ret < 0) {
			input_err(true, &info->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return IRQ_HANDLED;
		}

		input_info(true, &info->client->dev, "%s: run LPM interrupt handler, %d\n", __func__, jiffies_to_msecs(ret));
	}

	ret = gpio_get_value(info->pdata->gpio_int);
	if (ret) {
		input_err(true, &info->client->dev, "%s:Invalid interrupt\n", __func__);
		return IRQ_HANDLED;
	}

	ret = down_trylock(&info->work_lock);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to occupy work lock. work:[%d] gesture:[%d] enabled:[%d]\n",
				__func__, info->work_state, info->gesture_enable, info->device_enabled);

		ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev,
				"%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
		}

		if ((info->work_state != SUSPEND) || !info->gesture_enable)
			usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

		return IRQ_HANDLED;
	}

	if ((info->gesture_enable) && (info->work_state == SUSPEND)) {
		input_info(true, &client->dev, "%s:waiting AP resume..\n", __func__);
		t = wait_event_timeout(info->wait_q, info->wait_until_wake,
			msecs_to_jiffies(I2C_RESUME_DELAY));
		if (!t) {
			input_err(true, &client->dev, "%s:Timed out I2C resume\n", __func__);
			ztm730_send_wakeup_event(info);
			goto err_out;
		}
	}

	if (info->work_state != NOTHING) {
		input_err(true, &client->dev, "%s:Other process occupied\n", __func__);
		usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);
		ret = gpio_get_value(info->pdata->gpio_int);
		if (!ret) {
			ret = ztm730_write_cmd(client, ZTM730_CLEAR_INT_STATUS_CMD);
			if (ret != I2C_SUCCESS) {
				input_err(true, &client->dev,
					"%s:failed to write ZTM730_CLEAR_INT_STATUS_CMD\n", __func__);
			}
			usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);
		}
		goto out;
	}
	info->work_state = NORMAL;

	ret = ztm730_ts_read_coord(info);
	if (!ret || info->touch_info.status == 0xffff || info->touch_info.status == 0x1) {
		for (i = 0; i < I2C_RETRY_TIMES; i++) {
			ret = ztm730_ts_read_coord(info);
			if (!(!ret || info->touch_info.status == 0xffff
			   || info->touch_info.status == 0x1))
				break;
		}

		if (i == I2C_RETRY_TIMES) {
			input_err(true, &client->dev, "%s:Failed to read info coord\n", __func__);

			ret = ztm730_hard_reset(info);
			if (ret) {
				input_err(true, &client->dev, "%s:Failed to hard reset\n", __func__);
			}

			ret = ztm730_mini_init_touch(info);
			if (!ret) {
				input_err(true, &info->client->dev,
					"%s:Failed to mini_init_touch\n",
					__func__);
			}
			goto out;
		}
	}

#ifdef ZINITIX_FILE_DEBUG
	if (g_zini_file_debug_disable == 0)
	{
		/* zinitix_rawdata_store */
		if (info->touch_mode != TOUCH_POINT_MODE && info->touch_info.status == 0x0) {
			input_err(true, &client->dev, "%s:touch_mode is not TOUCH_POINT_MODE[%d]\n",
				__func__, info->touch_mode);
			goto out;
		}
	}
#endif
	if (!info->touch_info.status) {
		input_err(true, &client->dev, "%s:maybe periodical repeated interrupt\n",
			__func__);
		goto out;
	}

	if (info->gesture_enable) {
		ret = zinitix_bit_test(info->touch_info.status, BIT_GESTURE_WAKE);
		if (ret)
			ztm730_send_wakeup_event(info);
		else
			input_err(true, &client->dev, "%s:Can't detect wakeup event (0x%04x)\n",
				__func__, info->touch_info.status);

		ret = ztm730_clear_interrupt(info);
		if (ret)
			input_err(true, &client->dev, "%s:failed clear interrupt.\n", __func__);
		goto out;
	}

	if (!info->cap_info.multi_fingers) {
		input_info(true, &client->dev, "%s:cap_info.multi_fingers is zero. work_state:[%d], gesture:[%s]\n",
			__func__, info->work_state, info->gesture_enable ? "enable":"disable");
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->touch_info.coord[i].sub_status;
		prev_sub_status = info->reported_touch_info.coord[i].sub_status;

		ret = zinitix_bit_test(sub_status, SUB_BIT_EXIST);
		if (ret) {
			x = info->touch_info.coord[i].x;
			y = info->touch_info.coord[i].y;
			w = info->touch_info.coord[i].width;

			maxX = info->cap_info.MaxX;
			maxY = info->cap_info.MaxY;

			if (x == 0 && y == 0) {
				input_report_key(info->input_dev, KEY_INT_CANCEL, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_INT_CANCEL, 0);
				input_sync(info->input_dev);
			}
			if (x > maxX || y > maxY) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_err(true, &client->dev,
					"%s:Invalid coord %d: x=%d, y=%d\n", __func__, i, x, y);
				continue;
#endif
			}

			info->touch_info.coord[i].x = x;
			info->touch_info.coord[i].y = y;

			if (zinitix_bit_test(sub_status, SUB_BIT_DOWN)) {
				info->all_touch_cnt++;
				info->finger_event_cnt++;
				info->finger_cnt++;
				info->event[i].x = x;
				info->event[i].y = y;
				if (i)
					info->multi_touch_cnt++;
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &client->dev, "[P] tID:%d,%d x:%d y:%d tc:%d wet:%d noise:%d lpm:%d\n",
					i, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
					info->event[i].x, info->event[i].y,
					info->finger_cnt, info->wet_mode,
					info->noise_mode, info->lpm_mode);
#else
				input_info(true, &client->dev, "[P] tID:%d,%d tc:%d tc:%d wet:%d noise:%d lpm:%d\n",
					i, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
					info->finger_cnt, info->wet_mode,
					info->noise_mode, info->lpm_mode);
#endif
			} else if (zinitix_bit_test(sub_status, SUB_BIT_MOVE)) {
				info->event[i].move_cnt++;
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_dbg(true, &client->dev, "[M] tID:%d,%d x=%3d y=%3d tc:%d wet:%d noise:%d lpm:%d\n",
					i, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
					x, y, info->finger_cnt, info->wet_mode,
					info->noise_mode, info->lpm_mode);
#else
				input_dbg(true, &client->dev, "[M] tID:%d,%d tc:%d wet:%d noise:%d lpm:%d\n",
					i, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
					info->finger_cnt, info->wet_mode,
					info->noise_mode, info->lpm_mode);
#endif
			}

			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
			input_report_key(info->input_dev, BTN_TOUCH, 1);
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR, (u32)w);

			/*
			z = info->touch_info.coord[i].real_width;
			if (z < 1)
				z = 1;
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, (u32)z);
			if (info->max_z_value < z)
				info->max_z_value = z;
			*/

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
		} else if (zinitix_bit_test(sub_status, SUB_BIT_UP) ||
			zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &client->dev,
				"[R] tID:%d dd:%d,%d mc=%d tc:%d lx:%d ly:%d p:%d mode:0x%04x\n",
				i, (info->touch_info.coord[i].x-info->event[i].x),
				(info->touch_info.coord[i].y-info->event[i].y),
				info->event[i].move_cnt, info->finger_cnt,
				info->event[i].x, info->event[i].y,
				info->palm_flag, info->optional_mode);
#else
			input_info(true, &client->dev, "[R] tID:%d dd:%d,%d mc=%d tc:%d p:%d mode:0x%04x\n",
				i, (info->touch_info.coord[i].x-info->event[i].x),
				(info->touch_info.coord[i].y-info->event[i].y),
				info->event[i].move_cnt, info->finger_cnt,
				info->palm_flag, info->optional_mode);
#endif
			info->event[i].move_cnt = 0;

			if (info->finger_cnt > 0)
				info->finger_cnt--;

			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
		} else {
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
		}

		if (info->finger_cnt == 0) {
			input_report_key(info->input_dev, BTN_TOUCH, 0);
		}
	}

#if SUPPORTED_TOUCH_WHEEL
	if (zinitix_bit_test(info->touch_info.status, BIT_WHEEL_EVENT)) {
#if defined(ROTARY_BOOSTER)
		rotary_booster_turn_on();
#endif
		if (zinitix_bit_test(info->touch_info.finger_cnt, WHEEL_BIT_CW)) {
			input_info(true, &client->dev, "[CW] [0x%04x][0x%04x]\n",
					__func__, info->touch_info.status, info->touch_info.finger_cnt);
#ifdef INVERT_TOUCH_WEEL
			input_report_rel(bezel_info->input_dev, REL_WHEEL, BZ_CC);
#else
			input_report_rel(bezel_info->input_dev, REL_WHEEL, BZ_CW);
#endif
			input_sync(bezel_info->input_dev);
		} else if (zinitix_bit_test(info->touch_info.finger_cnt, WHEEL_BIT_CCW)) {
			input_info(true, &client->dev, "[CCW] [0x%04x][0x%04x]\n",
					__func__, info->touch_info.status, info->touch_info.finger_cnt);
#ifdef INVERT_TOUCH_WEEL
			input_report_rel(bezel_info->input_dev, REL_WHEEL, BZ_CW);
#else
			input_report_rel(bezel_info->input_dev, REL_WHEEL, BZ_CC);
#endif
			input_sync(bezel_info->input_dev);
		}
	}
#endif
	input_sync(info->input_dev);

#if SUPPORTED_PALM_TOUCH
	if (zinitix_bit_test(info->touch_info.status, BIT_PALM))
		palm = 1;
	else if (zinitix_bit_test(info->touch_info.status, BIT_PALM_REJECT))
		palm = 2;
	else
		palm = 0;

	if (info->palm_flag != palm) {
		info->palm_flag = !!palm;

		/* Need to release all active fingers before going to sleep */
		if (info->palm_flag == 0) {
			for (i = 0; i < info->cap_info.multi_fingers; i++) {
				prev_sub_status = info->reported_touch_info.coord[i].sub_status;
				if (zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
					input_info(true, &client->dev,
						"%s:[%d][RP] x=%3d y=%3d\n",
						__func__, i, info->reported_touch_info.coord[i].x,
						info->reported_touch_info.coord[i].y);
#else
					input_info(true, &client->dev, "%s:[%d][RP][0x%02x%02x%02x]\n",
							__func__, i, info->cap_info.fw_version,
							info->cap_info.fw_minor_version,
							info->cap_info.reg_data_version);
#endif
					info->event[i].move_cnt = 0;
					if (info->finger_cnt > 0)
						info->finger_cnt--;
					memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
					input_mt_slot(info->input_dev, i);
					input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
				}
			}
			input_sync(info->input_dev);
		}

		input_report_key(info->input_dev, KEY_SLEEP, info->palm_flag);
		input_sync(info->input_dev);

		input_dbg(true, &client->dev, "%s:palm: [%d]\n", __func__, info->palm_flag);
	}

#endif
	/* save reported touch info */
	memcpy((char *)&info->reported_touch_info,
		(char *)&info->touch_info, sizeof(struct point_info));

out:

	ret = ztm730_clear_interrupt(info);
	if (ret)
		input_err(true, &client->dev, "%s:failed clear interrupt.\n", __func__);

	if (info->work_state == NORMAL)
		info->work_state = NOTHING;

err_out:
	up(&info->work_lock);

	return IRQ_HANDLED;
}

int ztm730_store_gesture_on(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0, i;
	u16 gesture_state = 0;

	input_info(true, &client->dev, "%s+++\n", __func__);

	down(&info->work_lock);

	ztm730_clear_report_data(info);

	info->finger_event_cnt = 0;

	ret = ztm730_mode_change_recovery(info);
	if (ret)
		input_err(true, &client->dev,
			"%s: Failed hard_reset_recovery\n", __func__);

	set_gesture_mode(info, true);

	for (i = 0; i < I2C_RETRY_TIMES; i++) {
		ret = ztm730_write_cmd(info->client, ZTM730_SLEEP_CMD);
		if (ret != I2C_SUCCESS) {
			input_err(true, &client->dev,
				"%s:Failed to write ZTM730_SLEEP_CMD\n", __func__);
			continue;
		}
		usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

		ret = ztm730_read_raw_data(info->client,
				ZTM730_GESTURE_STATE, (u8 *)&gesture_state, 2);
		if (ret < 0) {
			input_err(true, &client->dev,
				"%s:Failed to get ZTM730_GESTURE_STATE\n", __func__);
			continue;
		}

		if (info->pinctrl && info->gpio_wake) {
			if (pinctrl_select_state(info->pinctrl, info->gpio_wake))
				input_err(true, &client->dev,
					"%s:failed to turn on gpio_wake\n", __func__);
		} else
			input_err(true, &client->dev,
				"%s:pinctrl or gpio_wake is NULL\n", __func__);

		if (gesture_state) {
			ret = 0;
			break;
		}
		input_err(true, &client->dev, "%s:Failed to change gesture mode (0x%04x) retry:%d\n",
						__func__, gesture_state, i);
	}

	if (i == I2C_RETRY_TIMES) {
		input_err(true, &client->dev,
			"%s:Failed to change gesture mode(0x%04x)\n", __func__, gesture_state);
		ret = -EIO;
		goto out;
	}

out:
	up(&info->work_lock);

	input_info(true, &client->dev, "%s---\n", __func__);

	return ret;
}

int ztm730_store_gesture_off(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	input_info(true, &client->dev, "%s+++\n", __func__);

	ztm730_clear_report_data(info);

	down(&info->work_lock);

	ret = ztm730_write_wakeup_cmd(info);
	if (ret) {
		input_err(true, &client->dev,
			"%s:Failed wake cmd\n", __func__);
	}

	ret = ztm730_mini_init_touch(info);
	if (!ret)
		input_err(true, &client->dev, "%s:Failed to resume[%d]\n", __func__, ret);

	up(&info->work_lock);
	input_info(true, &client->dev, "%s---\n", __func__);

	return 0;
}

void ztm730_print_info(struct ztm730_info *info)
{
	if (!info)
		return;

	if (!info->client)
		return;

	info->print_info_cnt_open++;

	if (info->print_info_cnt_open > 0xfff0)
		info->print_info_cnt_open = 0;

	if (info->finger_cnt == 0)
		info->print_info_cnt_release++;

	input_info(true, &info->client->dev,
			"aot:%d glove:%d bezel:%d// v:%02X%02X%02X%02X // #%d %d\n",
			info->aot_enable, info->glove_mode, info->bezel_enable,
			info->cap_info.ic_revision, info->cap_info.fw_minor_version,
			info->cap_info.hw_id, info->cap_info.reg_data_version,
			info->print_info_cnt_open, info->print_info_cnt_release);
}

void ztm730_print_info_work(struct work_struct *work)
{
	struct ztm730_info *info = container_of(work, struct ztm730_info,
			work_print_info.work);

	ztm730_print_info(info);

	if (!info->shutdown_is_on_going)
		schedule_delayed_work(&info->work_print_info, msecs_to_jiffies(30000));
}

void ztm730_read_info_work(struct work_struct *work)
{
	struct ztm730_info *info = container_of(work, struct ztm730_info,
			work_read_info.work);

	info->info_work_done = true;

	//print raw data

	if (info->shutdown_is_on_going) {
		input_err(true, &info->client->dev, "%s done, do not run work\n", __func__);
		return;
	}
	schedule_work(&info->work_print_info.work);
}

int ztm730_ts_set_lowpowermode(struct ztm730_info *info, u8 mode)
{
	u8 prev_work_state;

	input_info(true, &info->client->dev, "%s: %s\n", __func__,
			mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT");

	prev_work_state = info->work_state;

	if (mode == TO_TOUCH_MODE) {
		info->work_state = SLEEP_MODE_OUT;
		ztm730_store_gesture_off(info);

		if (device_may_wakeup(&info->client->dev))
			disable_irq_wake(info->irq);

		info->power_state = SEC_INPUT_STATE_POWER_ON;
	} else {
		info->work_state = SLEEP_MODE_IN;
		ztm730_store_gesture_on(info);

		if (device_may_wakeup(&info->client->dev))
			enable_irq_wake(info->irq);

		info->power_state = SEC_INPUT_STATE_LPM;
	}

	info->work_state = prev_work_state;

	return 0;
}

int ztm730_ts_start(struct ztm730_info *info)
{
	int ret;

	if (info->power_state == SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &info->client->dev, "%s: already power on\n", __func__);
		return 0;
	}

	input_err(true, &info->client->dev, "%s: debug 1\n", __func__);

	down(&info->work_lock);
	input_err(true, &info->client->dev, "%s: debug 2\n", __func__);
#if 0
	if (info->work_state != RESUME && info->work_state != EALRY_SUSPEND) {
		input_err(true, &info->client->dev, "%s: invalid work proceedure (%d)\n",
			__func__, info->work_state);
		up(&info->work_lock);
		return 0;
	}
#endif

	input_err(true, &info->client->dev, "%s: debug 3\n", __func__);
	if (info->pdata->use_deep_sleep) {
		info->power_state = SEC_INPUT_STATE_POWER_ON;
		ret = ztm730_write_wakeup_cmd(info);
		if (ret)
			input_err(true, &info->client->dev,
				"%s:Failed wake cmd\n", __func__);
	} else {
		ztm730_power_control(info, POWER_ON);
		ztm730_power_control(info, POWER_ON_SEQUENCE);
	}

	input_err(true, &info->client->dev, "%s: debug 4\n", __func__);

	ret = ztm730_mini_init_touch(info);
	if (!ret) {
		input_err(true, &info->client->dev, "%s:Failed to resume\n", __func__);
		info->work_state = NOTHING;
	}

	info->device_enabled = true;
	info->work_state = NOTHING;

	enable_irq(info->irq);

	up(&info->work_lock);

	return 0;
}

int ztm730_ts_stop(struct ztm730_info *info)
{
	int ret;

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: already power off\n", __func__);
		return 0;
	}

	disable_irq(info->irq);
	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "%s: invalid work proceedure (%d)\n",
			__func__, info->work_state);
		up(&info->work_lock);
		enable_irq(info->irq);
		return 0;
	}

	info->device_enabled = false;
	info->work_state = EALRY_SUSPEND;

	ztm730_clear_report_data(info);

	info->finger_event_cnt = 0;

	if (info->pdata->use_deep_sleep) {
		ret = ztm730_mode_change_recovery(info);
		if (ret)
			input_err(true, &info->client->dev,
				"%s: Failed hard_reset_recovery\n", __func__);

		set_gesture_mode(info, false);

		ret = ztm730_send_deep_sleep_cmd(info);
		if (ret)
			input_err(true, &info->client->dev,
				"%s:Failed deep sleep cmd\n", __func__);

		if (info->pinctrl && info->gpio_off) {
			if (pinctrl_select_state(info->pinctrl, info->gpio_off))
				input_err(true, &info->client->dev, "%s:failed to turn on gpio_off\n", __func__);
		} else
			input_err(true, &info->client->dev, "%s:pinctrl or gpio_off is NULL\n", __func__);

		info->power_state = SEC_INPUT_STATE_POWER_OFF;
	} else {
		ztm730_power_control(info, POWER_OFF);
	}

	up(&info->work_lock);

	return 0;
}

int ztm730_input_open(struct input_dev *dev)
{
	struct ztm730_info *info = input_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct irq_desc *desc = irq_to_desc(info->irq);

	input_info(true, &client->dev, "%s\n", __func__);

#if 0
	if (!info->probe_done) {
		input_info(true, &client->dev, "%s:device is not initialized\n", __func__);
		return 0;
	}
#endif

	cancel_delayed_work_sync(&info->work_read_info);


	if (info->power_state == SEC_INPUT_STATE_LPM)
		ztm730_ts_set_lowpowermode(info, TO_TOUCH_MODE);
	else
		ztm730_ts_start(info);

	while (desc->depth > 0)
		enable_irq(info->irq);

	info->print_info_cnt_open = 0;
	info->print_info_cnt_release = 0;
	schedule_work(&info->work_print_info.work);

	return 0;
}

void ztm730_input_close(struct input_dev *dev)
{
	struct ztm730_info *info = input_get_drvdata(dev);
	struct i2c_client *client = info->client;

	input_info(true, &client->dev, "%s\n", __func__);

#if 0
	if (!info->probe_done) {
		input_info(true, &client->dev, "%s:device is not initialized\n", __func__);
		return;
	}
#endif

	cancel_delayed_work_sync(&info->work_read_info);
	cancel_delayed_work(&info->work_print_info);
	ztm730_print_info(info);

	//set lp/stop
	if (info->aot_enable)
		ztm730_ts_set_lowpowermode(info, TO_LOWPOWER_MODE);
	else
		ztm730_ts_stop(info);

	return;
}

#if IS_ENABLED(CONFIG_PM)
int ztm730_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ztm730_info *info = i2c_get_clientdata(client);

	if (info->work_state != SUSPEND) {
		input_err(true, &client->dev, "%s:already enabled. work_state:[%d], gesture:[%s]\n",
			__func__, info->work_state, info->gesture_enable ? "enable":"disable");
		return 0;
	}

	input_dbg(true, &client->dev, "%s:work_state:[%d], gesture:[%s]\n",
		__func__, info->work_state, info->gesture_enable ? "enable":"disable");

	complete_all(&info->resume_done);

	if (info->gesture_enable) {
		info->work_state = NOTHING;
		info->wait_until_wake = true;
		wake_up(&info->wait_q);
	}

	return 0;
}

int ztm730_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ztm730_info *info = i2c_get_clientdata(client);

	if (info->work_state == SUSPEND) {
		input_dbg(true, &client->dev, "%s:already disabled. gesture:[%s]\n",
			__func__, info->gesture_enable ? "enable":"disable");
		return 0;
	}

	input_dbg(true, &client->dev, "%s:work_state:[%d], gesture:[%s]\n",
		__func__, info->work_state, info->gesture_enable ? "enable":"disable");

	reinit_completion(&info->resume_done);

	if (info->gesture_enable) {
		info->wait_until_wake = false;
		info->work_state = SUSPEND;
	}
	return 0;
}
#endif

int ztm730_get_lcd_id(struct device *dev)
{
#if !IS_ENABLED(CONFIG_SMCDSD_PANEL)
	int lcdtype = 0;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_DPU30) || IS_ENABLED(CONFIG_MCD_PANEL)
	int connected;
#endif

#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	lcdtype = get_lcd_attached("GET");
	if (lcdtype == 0xFFFFFF) {
		input_err(true, dev, "%s: lcd is not attached(GET)\n", __func__);
		return -ENODEV;
	}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_DPU30) || IS_ENABLED(CONFIG_MCD_PANEL)
	connected = get_lcd_info("connected");
	if (connected < 0) {
		input_err(true, dev, "%s: Failed to get lcd info(connected)\n", __func__);
		return -EINVAL;
	}

	if (!connected) {
		input_err(true, dev, "%s: lcd is disconnected(connected)\n", __func__);
		return -ENODEV;
	}

	input_info(true, dev, "%s: lcd is connected\n", __func__);

	lcdtype = get_lcd_info("id");
	if (lcdtype < 0) {
		input_err(true, dev, "%s: Failed to get lcd info(id)\n", __func__);
		return -EINVAL;
	}
#endif
#if IS_ENABLED(CONFIG_SMCDSD_PANEL)
	if (!lcdtype) {
		input_err(true, dev, "%s: lcd is disconnected(lcdtype)\n", __func__);
		return -ENODEV;
	}
#endif

	input_info(true, dev, "%s: lcdtype 0x%08X\n", __func__, lcdtype);
	return lcdtype;
}

int ztm730_ts_parse_dt(struct device_node *np, struct device *dev,
					struct zxt_ts_platform_data *pdata)
{
	int lcdtype = 0;
	int ret;

	if (!np) {
		input_err(true, dev, "%s:np is NULL.\n", __func__);
		return -EINVAL;
	}

	lcdtype = ztm730_get_lcd_id(dev);
	if (lcdtype < 0) {
		input_err(true, dev, "lcd is not attached");
		return -ENODEV;
	}
	input_info(true, dev, "lcdtype=%X", lcdtype);

	/* gpio irq */
	pdata->gpio_int = of_get_named_gpio(np, "zinitix,irq-gpio", 0);
	if (pdata->gpio_int < 0) {
		input_err(true, dev, "%s:failed to get gpio_int\n", __func__);
		return -EINVAL;
	}

	/* gpio reset */
	pdata->gpio_reset = of_get_named_gpio(np, "zinitix,reset-gpio", 0);
	if (pdata->gpio_reset < 0) {
		input_err(true, dev, "%s:failed to get gpio_reset\n", __func__);
		pdata->gpio_reset = -1;
	}

	ret = of_property_read_string(np, "zinitix,fw_name", &pdata->fw_name);
	if (ret < 0) {
		input_err(true, dev, "%s:failed to get firmware path!\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "zinitix,x_resolution", &pdata->x_resolution);
	if (ret < 0) {
		input_err(true, dev, "%s:failed to get x_resolution\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "zinitix,y_resolution", &pdata->y_resolution);
	if (ret < 0) {
		input_err(true, dev, "%s:failed to get y_resolution\n", __func__);
		return ret;
	}

	/* gpio power enable */
	pdata->vdd_en = of_get_named_gpio(np, "zinitix,tsppwr_en", 0);
	if (pdata->vdd_en < 0)
		pdata->vdd_en = -1;

	ret = of_property_read_string(np, "avdd_regulator", &pdata->avdd_ldo);
	if (ret < 0) {
		pdata->avdd_ldo = 0;
		input_err(true, dev, "%s:failed to get  avdd_regulator %d\n", __func__, ret);
	}

	ret = of_property_read_string(np, "dvdd_regulator", &pdata->dvdd_ldo);
	if (ret < 0) {
		pdata->dvdd_ldo = 0;
		input_err(true, dev, "%s:failed to get  dvdd_regulator %d\n", __func__, ret);
	}

	pdata->regulator_boot_on = of_property_read_bool(np, "zinitix,regulator_boot_on");
	pdata->support_bezel_detent = of_property_read_bool(np, "support_bezel_detent");
	if (of_property_read_u32(np, "zinitix,bringup", &pdata->bringup) < 0)
		pdata->bringup = 0;

	pdata->use_deep_sleep = of_property_read_bool(np, "zinitix,use_deep_sleep");

	input_info(true, dev, "%s: max(%d/%d), reg_boot_on:%d, fw_name:%s, bringup:%d, vdd_en:%d, int:%d, reset:%d bezel:%d\n",
		__func__, pdata->x_resolution, pdata->y_resolution, pdata->regulator_boot_on,
		pdata->fw_name, pdata->bringup, pdata->vdd_en, pdata->gpio_int, pdata->gpio_reset,
		pdata->support_bezel_detent);

	return 0;

}

#if IS_ENABLED(CONFIG_OF)
int ztm730_pinctrl_configure(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	int retval = 0;

	info->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(info->pinctrl)) {
		if (PTR_ERR(info->pinctrl) == -EPROBE_DEFER) {
			input_err(true, &client->dev, "%s:failed to get pinctrl\n", __func__);
			retval = -ENODEV;
		}
		input_info(true, &client->dev, "%s:Target does not use pinctrl\n", __func__);
		info->pinctrl = NULL;
		goto out;
	}

	if (info->pinctrl) {
		info->gpio_wake = pinctrl_lookup_state(info->pinctrl, "wakeup");
		if (IS_ERR(info->gpio_wake)) {
			input_err(true, &client->dev, "%s:failed to get gpio_wake pin state\n", __func__);
			info->gpio_wake = NULL;
		}
	}

	if (info->pinctrl) {
		info->gpio_off = pinctrl_lookup_state(info->pinctrl, "off");
		if (IS_ERR(info->gpio_off)) {
			input_err(true, &client->dev, "%s:failed to get gpio_off pin state\n", __func__);
			info->gpio_off = NULL;
		}
	}

	if (info->pinctrl && info->gpio_off) {
		if (pinctrl_select_state(info->pinctrl, info->gpio_off))
			input_err(true, &client->dev, "%s:failed to turn on gpio_off\n", __func__);
	} else
		input_err(true, &client->dev, "%s:pinctrl or gpio_off is NULL\n", __func__);
out:
	return retval;
}
#endif

void ztm730_setup_input_device(struct ztm730_info *info)
{
	struct input_dev *input_dev = info->input_dev;

	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	set_bit(KEY_INT_CANCEL, input_dev->keybit);

	input_mt_init_slots(input_dev, info->cap_info.multi_fingers, 0);

	input_set_abs_params(input_dev, ABS_X,
		info->cap_info.MinX, info->cap_info.MaxX + ABS_PT_OFFSET, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
		info->cap_info.MinY, info->cap_info.MaxY + ABS_PT_OFFSET, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
		info->cap_info.MinX, info->cap_info.MaxX + ABS_PT_OFFSET, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
		info->cap_info.MinY, info->cap_info.MaxY + ABS_PT_OFFSET, 0, 0);

	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, MAX_SUPPORTED_FINGER_NUM, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_ORIENTATION, -128, 127, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, REAL_Z_MAX, 0, 0);

#if SUPPORTED_PALM_TOUCH
	input_set_capability(input_dev, EV_KEY, KEY_SLEEP);
#endif
}

int ztm730_ts_gpio_init(struct ztm730_info *info)
{
	struct i2c_client *client = info->client;
	struct zxt_ts_platform_data	*pdata = info->pdata;
	int ret;

#if IS_ENABLED(CONFIG_OF)
	ret = ztm730_pinctrl_configure(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed ztm730_pinctrl_configure\n", __func__);
		ret = -EINVAL;
		goto out;
	}
#endif

	if (gpio_is_valid(pdata->vdd_en)) {
		ret = gpio_request_one(pdata->vdd_en, GPIOF_OUT_INIT_LOW, "ztm730_vdd_en");
		if (ret < 0) {
			input_err(true, &client->dev, "%s:failed to request gpio_vdd_en\n", __func__);
			ret = -EINVAL;
			goto out;
		}
		ztm730_delay(CHIP_ON_DELAY);

		ret = gpio_direction_output(pdata->vdd_en, 1);
		if (ret) {
			input_err(true, &client->dev, "%s:unable to set_direction for zt_vdd_en [%d]\n",
				__func__, pdata->vdd_en);
			ret = -ENODEV;
			goto out;
		}
		ztm730_delay(CHIP_ON_DELAY);

		ret = gpio_direction_output(pdata->vdd_en, 0);
		if (ret) {
			input_err(true, &client->dev, "%s:unable to set_direction for zt_vdd_en [%d]\n",
				__func__, pdata->vdd_en);
			ret = -ENODEV;
			goto out;
		}
		ztm730_delay(CHIP_OFF_DELAY);
		gpio_free(pdata->vdd_en);
	}

	ret = gpio_request(pdata->gpio_int, "ztm730_irq");
	if (ret < 0) {
		input_err(true, &client->dev, "%s:failed to request gpio_irq\n", __func__);
		ret = -EINVAL;
		goto out;
	}
	gpio_direction_input(pdata->gpio_int);

out:
	return ret;
}

/*
void ztm730_debug_dwork(struct work_struct *work)
{
	struct ztm730_info *info = container_of(work,
				struct ztm730_info, debug_dwork.work);
	struct i2c_client *client = info->client;

	cancel_delayed_work(&info->debug_dwork);

	run_dnd_read(info);

	input_info(true, &client->dev, "%s\n", __func__);

	return;
}
*/

#if SUPPORTED_TOUCH_WHEEL
int ztm730_bezel_open(struct input_dev *dev)
{
	struct ztm730_info *info = input_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;

	input_info(true, &client->dev, "%s\n", __func__);

	zinitix_bit_set(info->optional_mode, DEF_OPTIONAL_MODE_WHEEL_ON_BIT);

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return 0;
	}

	ret = ztm730_set_optional_mode(info, true);
	if (ret) {
		input_err(true, &client->dev, "%s:failed ztm730_set_optional_mode\n", __func__);
		return -EIO;
	}

	return 0;
}

void ztm730_bezel_close(struct input_dev *dev)
{
	struct ztm730_info *info = input_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;

	input_info(true, &client->dev, "%s\n", __func__);

	zinitix_bit_clr(info->optional_mode, DEF_OPTIONAL_MODE_WHEEL_ON_BIT);

	if (info->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		return;
	}

	ret = ztm730_write_reg(info->client, ZTM730_OPTIONAL_SETTING, info->optional_mode);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev,
			"%s:Failed to write ZTM730_OPTIONAL_SETTING(%d)\n",
			__func__, info->optional_mode);
		return;
	}

	return;
}

int ztm730_setup_bezel_input(struct ztm730_info *info)
{
	struct ztm_bezel_ddata *ddata = info->bezel_info;
	struct i2c_client *client = info->client;
	int ret;

	ddata->input_dev = input_allocate_device();
	if (!ddata->input_dev) {
		input_err(true, &client->dev, "%s: Failed to allocate input device.\n", __func__);
		return -EPERM;
	}

	input_set_capability(ddata->input_dev, EV_REL, REL_WHEEL);

	ddata->input_dev->name = BEZEL_NAME;
	ddata->input_dev->id.bustype = BUS_VIRTUAL;
	ddata->input_dev->dev.parent = &client->dev;
	ddata->input_dev->phys = BEZEL_NAME;
	ddata->input_dev->id.vendor = 0x0001;
	ddata->input_dev->id.product = 0x0001;
	ddata->input_dev->id.version = 0x0100;

	input_set_drvdata(ddata->input_dev, info);

	ret = input_register_device(ddata->input_dev);
	if (ret) {
		input_err(true, &client->dev, "%s:Unable to register %s input device\n",
			__func__, ddata->input_dev->name);
		return -EPERM;
	}

//	ddata->input_dev->open = ztm730_bezel_open;
//	ddata->input_dev->close = ztm730_bezel_close;

	return 0;
}
#endif

int ztm730_probe(struct i2c_client *client, const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct zxt_ts_platform_data *pdata = client->dev.platform_data;
	struct ztm730_info *info;
	struct input_dev *input_dev;
	struct device_node *np = client->dev.of_node;
	bool force_update = false;
	int ret = -1;

	input_info(true, &client->dev, "%s\n", __func__);

#if 0
//#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (lpcharge == 1) {
		input_err(true, &client->dev, "%s : Do not load driver due to : lpm %d\n",
				__func__, lpcharge);
		return -ENODEV;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s:Not compatible i2c function\n", __func__);
		ret = -EIO;
		goto err_no_platform_data;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct zxt_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			input_err(true, &client->dev, "%s:Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		ret = ztm730_ts_parse_dt(np, &client->dev, pdata);
		if (ret) {
			input_err(true, &client->dev, "%s:Failed ztm730_ts_parse_dt\n", __func__);
			goto err_no_platform_data;
		}
	}

	info = devm_kzalloc(&client->dev, sizeof(struct ztm730_info), GFP_KERNEL);
	if (info == NULL) {
		input_err(true, &client->dev, "%s:Failed to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err_no_platform_data;
	}

	i2c_set_clientdata(client, info);
	info->client = client;
	info->pdata = pdata;
	g_ztm730_info = info;

	ret = ztm730_ts_gpio_init(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to ztm730_ts_gpio_init\n", __func__);
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev, "%s:Failed to allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_alloc;
	}

#if SUPPORTED_TOUCH_WHEEL
	info->bezel_info = devm_kzalloc(&client->dev,
		sizeof(struct ztm_bezel_ddata), GFP_KERNEL);
	if (!pdata) {
		input_err(true, &client->dev, "%s:Failed to allocate bezel_info\n", __func__);
		goto err_bezel_alloc;
	}

	ret = ztm730_setup_bezel_input(info);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed bezel device setup.\n", __func__);
		goto err_bezel_setup;
	}
#endif

	info->input_dev = input_dev;
	info->work_state = PROBE;

	/* power on */
	ret = ztm730_regulator_init(info);
	if (ret) {
		input_err(true, &info->client->dev,
			"%s:Failed to ztm730_regulator_init\n",
			__func__);
		goto err_power_sequence;
	}

	if (!ztm730_power_control(info, POWER_ON)) {
		input_err(true, &info->client->dev,
			"%s:Failed to ztm730_power_control\n",
			__func__);
		goto err_power_sequence;
	}

	if (!ztm730_power_control(info, POWER_ON_SEQUENCE)) {
		input_err(true, &info->client->dev,
			"%s:Failed to power on sequence\n",
			__func__);
		goto err_power_sequence;
	}

	memset(&info->reported_touch_info, 0x0, sizeof(struct point_info));
	sema_init(&info->work_lock, 1);

	/* init touch mode */
	info->touch_mode = TOUCH_POINT_MODE;

	/*version check*/
	if (ztm730_ic_version_check(info) < 0) {
		input_err(true, &info->client->dev,
			"%s: fail version check, ret:%d", __func__, ret);
		force_update = true;
	}

	if (pdata->bringup == 4)
		force_update = true;

	/*fw upgrade*/
	ret = ztm730_fw_update_work(info, force_update);
	if (ret < 0) {
		ret = -EPERM;
		input_err(true, &info->client->dev,
			"%s: fail update_work", __func__);
		goto err_fw_update;
	}

	ret = ztm730_init_touch(info);
	if (!ret) {
		input_err(true, &info->client->dev,
			"%s:Failed to ztm730_init_touch\n",
			__func__);
		goto err_init_touch;
	}

#ifdef USE_TSP_TA_CALLBACKS
	info->callbacks.inform_charger = ztm730_ts_charger_status_cb;
	ztm730_register_callback(&info->callbacks);
#endif

	snprintf(info->phys, sizeof(info->phys), "%s/input0", dev_name(&client->dev));
	input_dev->name = SEC_TSP_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->phys = info->phys;
	input_dev->dev.parent = &client->dev;

	ztm730_setup_input_device(info);

	init_waitqueue_head(&info->wait_q);
	device_init_wakeup(&client->dev, true);

	info->wakelock = wakeup_source_register(&client->dev, "tsp_wakelock");
	init_completion(&info->resume_done);
	complete_all(&info->resume_done);

	INIT_DELAYED_WORK(&info->work_read_info, ztm730_read_info_work);
	INIT_DELAYED_WORK(&info->work_print_info, ztm730_print_info_work);
	mutex_init(&info->modechange);

//	info->input_dev->open = ztm730_input_open;
//	info->input_dev->close = ztm730_input_close;

	input_set_drvdata(info->input_dev, info);

	ret = input_register_device(info->input_dev);
	if (ret) {
		input_err(true, &info->client->dev, "%s:unable to register input device\n", __func__);
		goto err_input_register_device;
	}

	info->work_state = NOTHING;
	info->finger_cnt = 0;

	info->irq = gpio_to_irq(pdata->gpio_int);
	if (info->irq < 0) {
		input_err(true, &client->dev, "%s:failed to get gpio_to_irq\n", __func__);
		ret = -ENODEV;
		goto err_gpio_irq;
	}
	ret = request_threaded_irq(info->irq, NULL, ztm730_touch_work,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT , SEC_TSP_NAME, info);

	if (ret) {
		input_err(true, &client->dev, "%s:failed to request irq.\n", __func__);
		goto err_request_irq;
	}

	ret = enable_irq_wake(info->irq);
	if (ret < 0) {
		input_err(true, &client->dev, "%s:failed to enable_irq_wake.\n", __func__);
		goto err_request_irq;
	}

	sema_init(&info->raw_data_lock, 1);

	ret = init_sec_factory(info);
	if (ret) {
		input_err(true, &client->dev, "%s:Failed to init sysfs node\n", __func__);
		goto err_kthread_create_failed;
	}

	info->wait_until_wake = true;
	info->device_enabled = true;
	info->probe_done = true;
	info->enabled = 1;

	ztm730_get_dqa_data(info);

//temp
//		INIT_DELAYED_WORK(&info->debug_dwork, ztm730_debug_dwork);
//		schedule_delayed_work(&info->debug_dwork, 0);

	input_info(true, &client->dev, "%s:done.\n", __func__);

	schedule_work(&info->work_read_info.work);

	return 0;

#ifdef SEC_FACTORY_TEST
err_kthread_create_failed:
	devm_kfree(&client->dev, info->raw_data);
#endif

err_request_irq:
	free_irq(info->irq, info);
err_gpio_irq:
	input_unregister_device(info->input_dev);
err_input_register_device:
	wakeup_source_unregister(info->wakelock);
#ifdef USE_TSP_TA_CALLBACKS
	ztm730_register_callback(NULL);
#endif
err_init_touch:
err_fw_update:
	ztm730_power_control(info, POWER_OFF);
#if SUPPORTED_TOUCH_WHEEL
err_bezel_setup:
	devm_kfree(&client->dev, info->bezel_info);
err_bezel_alloc:
#endif
err_power_sequence:
	if (gpio_is_valid(pdata->gpio_int) != 0)
		gpio_free(pdata->gpio_int);
	if (gpio_is_valid(pdata->vdd_en) != 0)
		gpio_free(pdata->vdd_en);
	input_free_device(info->input_dev);
err_alloc:
	devm_kfree(&client->dev, info);
	i2c_set_clientdata(client, NULL);
err_no_platform_data:
	if (IS_ENABLED(CONFIG_OF))
		devm_kfree(&client->dev, (void *)pdata);

	input_err(true, &client->dev, "%s:Failed to probe\n", __func__);
	return -EPERM;
}


int ztm730_remove(struct i2c_client *client)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
	struct zxt_ts_platform_data *pdata = info->pdata;

	input_info(true, &info->client->dev, "%s\n", __func__);

	info->shutdown_is_on_going = true;
	cancel_delayed_work_sync(&info->work_read_info);
	cancel_delayed_work_sync(&info->work_print_info);

	disable_irq(info->irq);
	down(&info->work_lock);

	info->work_state = REMOVE;
	wakeup_source_unregister(info->wakelock);

	remove_sec_factory(info);

	if (info->irq)
		free_irq(info->irq, info);

	if (gpio_is_valid(pdata->gpio_int) != 0)
		gpio_free(pdata->gpio_int);

	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	up(&info->work_lock);
	devm_kfree(&client->dev, info);

	return 0;
}

void ztm730_shutdown(struct i2c_client *client)
{
	struct ztm730_info *info = i2c_get_clientdata(client);
//	int ret;

	if (info == NULL) {
		input_err(true, &client->dev, "%s:info is NULL", __func__);
		return;
	}

	input_info(true, &client->dev, "%s\n", __func__);
	info->shutdown_is_on_going = true;

	down(&info->work_lock);

	if ((info->device_enabled) && (info->probe_done)) {
		disable_irq_wake(info->irq);
		disable_irq(info->irq);

		set_gesture_mode(info, false);
		info->probe_done = false;
		info->device_enabled = false;
		info->work_state = SUSPEND;
	}

	ztm730_power_control(info, POWER_OFF);

	up(&info->work_lock);
}

#if IS_ENABLED(CONFIG_OF)
const struct of_device_id tsp_dt_ids[] = {
	{ .compatible = "Zinitix,ztm730_ts", },
	{},
};
MODULE_DEVICE_TABLE(of, tsp_dt_ids);
#else
#define tsp_dt_ids NULL
#endif

struct i2c_device_id ztm730_idtable[] = {
	{ZTM730_TS_DEVICE, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, ztm730_idtable);

const struct dev_pm_ops ztm730_ts_pm_ops = {
#if IS_ENABLED(CONFIG_PM)
	.suspend = ztm730_suspend,
	.resume = ztm730_resume,
#endif
};

struct i2c_driver ztm730_ts_driver = {
	.probe	= ztm730_probe,
	.remove	= ztm730_remove,
	.shutdown = ztm730_shutdown,
	.id_table	= ztm730_idtable,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ZTM730_TS_DEVICE,
		.pm = &ztm730_ts_pm_ops,
		.of_match_table = tsp_dt_ids,
	},
};

int __init ztm730_init(void)
{
	int ret;

	pr_info("[sec_input] %s\n", __func__);

	ret = i2c_add_driver(&ztm730_ts_driver);
	if (ret) {
		pr_err("[sec_input] %s:device init failed.[%d]\n", __func__, ret);
	}

	return ret;
}

void __exit ztm730_exit(void)
{
	i2c_del_driver(&ztm730_ts_driver);
}

module_init(ztm730_init);
module_exit(ztm730_exit);

MODULE_DESCRIPTION("touch-screen device driver using i2c interface");
MODULE_LICENSE("GPL");

