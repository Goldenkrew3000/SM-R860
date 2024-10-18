/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Debug interface between LPD fw and LPD driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "lpd.h"

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

int lpd_log_level = 7;
int lpd_fwlog_level = 2;

enum {
	LPD_FW_LOG_LEVEL_ERR = 0,
	LPD_FW_LOG_LEVEL_WARN,
	LPD_FW_LOG_LEVEL_INFO,
	LPD_FW_LOG_LEVEL_DBG,
	MAX_LPD_FW_LOG_LEVEL,
};

#define LPD_FW_DEFAULT_LOG_LEVEL LPD_FW_LOG_LEVEL_INFO

static char fwlog_names[MAX_LPD_FW_LOG_LEVEL] = {
	'E',
	'W',
	'I',
	'D',
};

static int lpd_logbuf_regdump(struct lpd_device *lpd)
{
	struct lpd_fw_dump *dump = NULL;
	struct lpd_fw_dump_content *log;
	u32 index = 0;
	u32 len;
	int ret;

	dump = kzalloc(sizeof(struct lpd_fw_dump), GFP_ATOMIC);
	if (!dump)
		goto end;
	ret = lpd_sram_read(lpd, dump, sizeof(struct lpd_fw_dump), lpd->sram.off.dump);
	if (ret < 0)
		goto end;

	if (strncmp(dump->magic, LPD_FW_DUMP_MAGIC, sizeof(dump->magic)) != 0) {
		lpd_err("%s: invalid magic %s\n", __func__, dump->magic);
		goto end;
	}

	while (index < dump->lines) {
		log = &dump->log[index];
		len = strlen((char *)log);

		if (len > 0 && len <= DUMP_SIZE)
			lpd_info("%s",(char *)log->buf);
		else
			lpd_err("%s: size err:%d, idx:%d\n", __func__, len, index);
		index = (index + 1) % DUMP_NUM;
	}
end:
	if (dump)
		kfree(dump);
	
	return 0;
}

enum lpd_logbuf_wr {
	LOGBUF_WR_ENQUEUE 	= 1 << 0,
	LOGBUF_WR_DEQUE 	= 1 << 1,
	LOGBUF_WR_FULL		= 1 << 2,
	LOGBUF_WR_LOGLEVEL	= 1 << 3,
	LOGBUF_WR_REGDUMP	= 1 << 4,
};

int lpd_init_logbuf(struct lpd_device *lpd)
{
	int ret;
	size_t size = 0;
	void *buf = NULL;
	unsigned int offset = 0;
	struct lpd_fw_logbuf_header header;
	unsigned int magic_len = 0;

	ret = lpd_sram_read(lpd, &header, sizeof(struct lpd_fw_logbuf_header), lpd->sram.off.logbuf);
	if (ret < 0) {
		lpd_err("ERR:%s sram read fail\n", __func__);
		return ret;
	}

	magic_len = strlen(LPD_FW_LOGBUF_MAGIC);
	if (magic_len > LPD_FW_MAGIC_SIZE) {
		lpd_err("ERR:%s: exceed log buf magic size: %d\n magic_len\n", __func__, magic_len);
		magic_len = LPD_FW_MAGIC_SIZE;
	}

	if (strncmp(header.magic, LPD_FW_LOGBUF_MAGIC, magic_len) != 0) {
		lpd_info("%s: init logbuf header: loglevel: %d->%d\n", __func__, header.loglevel, lpd_fwlog_level);

		buf = &header;
		offset = lpd->sram.off.logbuf;
		size = sizeof(struct lpd_fw_logbuf_header);

		memset(&header, 0, size);
		memcpy(header.magic, LPD_FW_LOGBUF_MAGIC, magic_len);
		header.loglevel = lpd_fwlog_level;
	} else {
		buf = &header.seq_num;
		size = sizeof(header.seq_num);
		offset = lpd->sram.off.logbuf + offsetof(struct lpd_fw_logbuf_header, seq_num);

		header.seq_num = header.seq_num + 1;
	}

	ret = lpd_sram_write(lpd, buf, size, offset);
	if (ret != size) {
		lpd_err("ERR: %s: failed write logbuf data to sram: %d\n", __func__, ret);
		return ret;
	}

	lpd_info("%s: seq_num %d\n", __func__, header.seq_num);
	return 0;
}

int lpd_read_cm55_fault_status(struct lpd_device *lpd)
{
	int ret;
	struct lpd_fw_logbuf_header header;
	unsigned int magic_len;

	if (lpd == NULL) {
		lpd_err("ERR: %s: invalid argument\n", __func__);
		return -EINVAL;
	}

	ret = lpd_sram_read(lpd, &header, sizeof(struct lpd_fw_logbuf_header), lpd->sram.off.logbuf);
	if (ret != sizeof(struct lpd_fw_logbuf_header)) {
		lpd_err("ERR:%s sram read fail\n", __func__);
		return ret;
	}

	magic_len = strlen(LPD_FW_LOGBUF_MAGIC);
	if (magic_len > LPD_FW_MAGIC_SIZE) {
		lpd_err("ERR:%s exceed magic code len: %d\n", __func__, magic_len);
		magic_len = LPD_FW_MAGIC_SIZE;
	}

	if (strncmp(header.magic, LPD_FW_LOGBUF_MAGIC, magic_len) != 0) {
		lpd_err("ERR:%s: invalid magic code\n", __func__);
		return -EINVAL;
	}

	lpd_info("%s: lpd fault status: %x\n", __func__, header.lpd_fault_status);

	if (header.lpd_fault_status != 0) {
		lpd_err("ERR: %s found cm55 fault: %x\n", __func__, header.lpd_fault_status);
		BUG();
	}

	return 0;
}

static void print_fw_log(struct lpd_device *lpd, struct lpd_fw_logbuf_content *log)
{
	size_t len;
	char logbuf[LOGBUF_BUF_SIZE + 1];
	u32 fw_log_level = LPD_FW_DEFAULT_LOG_LEVEL;

	if (!lpd) {
		lpd_err("null lpd\n");
		return;
	}

	if (!log) {
		lpd_err("null log\n");
		return;
	}

	len = strlen((char *)log->buf);
	if (len > LOGBUF_BUF_SIZE) {
		lpd_warn("Exceed firmware log length (MAX:%d): %d\n", LOGBUF_BUF_SIZE, len);
		len = LOGBUF_BUF_SIZE;
	}

	strncpy(logbuf, (char *)log->buf, len);
	logbuf[len] = 0;

	if (log->level < MAX_LPD_FW_LOG_LEVEL)
		fw_log_level = (u8)log->level;
	else
		lpd_warn("Invalid fw log level: %d\n", log->level);

	lpd_fw_log("%02d [%6llu.%06llu]%c %s", log->size % 100, (log->timestamp) / USEC_PER_SEC,
		(log->timestamp) % USEC_PER_SEC, fwlog_names[fw_log_level], logbuf);

#ifdef CONFIG_EXYNOS_MEMORY_LOGGER
	if (lpd->debug_info.mlog.memlog_printf)
		memlog_write_printf(lpd->debug_info.mlog.memlog_printf, MEMLOG_LEVEL_ERR, "%s", logbuf);
#endif

}


static int update_fw_log_header(struct lpd_device *lpd, struct lpd_fw_logbuf_header *header, u32 update_flag)
{
	int ret = 0;
	u32 offset;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	if (!header) {
		lpd_err("null header\n");
		return -EINVAL;
	}

	if (update_flag & LOGBUF_WR_ENQUEUE) {
		offset = lpd->sram.off.logbuf + offsetof(struct lpd_fw_logbuf_header, eq);
		ret = lpd_sram_write(lpd, &header->eq, sizeof(header->eq), offset);
	}

	if (update_flag & LOGBUF_WR_DEQUE) {
		offset = lpd->sram.off.logbuf + offsetof(struct lpd_fw_logbuf_header, dq);
		ret = lpd_sram_write(lpd, &header->dq, sizeof(header->dq), offset);
	}

	if (update_flag & LOGBUF_WR_FULL) {
		offset = lpd->sram.off.logbuf + offsetof(struct lpd_fw_logbuf_header, full);
		ret = lpd_sram_write(lpd, &header->full, sizeof(header->full), offset);
	}

	if (update_flag & LOGBUF_WR_LOGLEVEL) {
		offset = lpd->sram.off.logbuf + offsetof(struct lpd_fw_logbuf_header, loglevel);
		ret = lpd_sram_write(lpd, &header->loglevel, sizeof(header->loglevel), offset);
	}

	if (update_flag & LOGBUF_WR_REGDUMP) {
		offset = lpd->sram.off.logbuf + offsetof(struct lpd_fw_logbuf_header, regdump);
		ret = lpd_sram_write(lpd, &header->regdump, sizeof(header->regdump), offset);
	}

	return ret;
}

int lpd_logbuf_outprint(struct lpd_device *lpd)
{
	u32 eq;
	int ret;
	u32 sram_update = 0;
	struct lpd_fw_logbuf_content *log;
	struct lpd_fw_logbuf_header *header;
	struct lpd_fw_logbuf *fw_logbuf;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	fw_logbuf = lpd->sram_logbuf;
	if (!fw_logbuf) {
		lpd_err("null logbuf\n");
		return -EINVAL;
	}

	mutex_lock(&lpd->debug_info.logbuf_lock);

	ret = lpd_sram_read(lpd, fw_logbuf, sizeof(struct lpd_fw_logbuf), lpd->sram.off.logbuf);
	if (ret < 0) {
		lpd_err("Failed to read firmware log buffer to sram\n");
		goto done;
	}

	header = &fw_logbuf->header;
	if (strncmp(header->magic, LPD_FW_LOGBUF_MAGIC, sizeof(header->magic)) != 0) {
		lpd_err("Invalid magic %s eq %d dq %d\n", header->magic, header->eq, header->dq);
		goto done;
	}

	eq = header->eq;
	if (eq >= LOGBUF_NUM || header->dq >= LOGBUF_NUM) {
		lpd_err("Invalid queue value (MAX:%d)  eq:%d, dq:%d\n", LOGBUF_NUM, eq, header->dq);
		header->eq = 0;
		header->dq = 0;
		sram_update |= (LOGBUF_WR_ENQUEUE|LOGBUF_WR_DEQUE);
		goto done;
	}

	if (header->full) {
		header->full = 0;
		header->dq = (eq + 1) % LOGBUF_NUM;
		sram_update |= (LOGBUF_WR_FULL | LOGBUF_WR_DEQUE);
	}

	if (header->loglevel != lpd_fwlog_level) {
		lpd_info("change lpd firmware log level: %d -> %d\n", header->loglevel, lpd_fwlog_level);
		header->loglevel = lpd_fwlog_level;
		sram_update |= LOGBUF_WR_LOGLEVEL;
	}

	while (eq != header->dq) {
		log = &fw_logbuf->log[header->dq];
		print_fw_log(lpd, log);

		header->dq = (header->dq + 1) % LOGBUF_NUM;
		sram_update |= LOGBUF_WR_DEQUE;
	}

	if (header->regdump > 0) {
		lpd_logbuf_regdump(lpd);
		header->regdump = 0;
		sram_update |= LOGBUF_WR_REGDUMP;
	}

done:
	if (sram_update) {
		ret = update_fw_log_header(lpd, header, sram_update);
		if (ret < 0)
			lpd_err("failed to update fw log header\n");
	}

	mutex_unlock(&lpd->debug_info.logbuf_lock);

	return 0;
}

#if defined(TEST_M55_NOTI_AP_STATE)
int lpd_logbuf_noti_apstate(struct lpd_device *lpd, u8 apState)
{
	int ret;
	u32 offset;

	offset = lpd->sram.off.logbuf + offsetof(struct lpd_fw_logbuf_header, apstate);

	ret = lpd_sram_write(lpd, &apState, sizeof(apState), offset);
	if (ret < 0)
		lpd_err("failed to write apstate to sram\n");

	return ret;
}
#endif

#define LOG_FW_LOG_THREAD_PERIOD	1000 //1000msec

static int lpd_logbuf_thread(void *arg)
{
	struct lpd_device *lpd;

	lpd = (struct lpd_device *)arg;
	if (!lpd) {
		lpd_err("null lpd\n");
		return 0;
	}

	lpd_info("start firmware log buffer thread\n");

	while (!kthread_should_stop()) {
		lpd_logbuf_outprint(lpd);
		msleep(LOG_FW_LOG_THREAD_PERIOD);
	}

	return 0;
}

static int lpd_logbuf_start(struct lpd_device *lpd)
{
	NULL_CHECK(lpd);

	lpd_info("%s\n", __func__);

	if (lpd->debug_info.logbuf_thread == NULL)
		lpd->debug_info.logbuf_thread = kthread_run(lpd_logbuf_thread,
			lpd, "lpd_logbuf_thread");

	return 0;
}

int lpd_logbuf_stop(struct lpd_device *lpd)
{
	NULL_CHECK(lpd);

	lpd_info("%s\n", __func__);

	if (lpd->debug_info.logbuf_thread)
		kthread_stop(lpd->debug_info.logbuf_thread);

	lpd->debug_info.logbuf_thread = NULL;

	return 0;
}

static int lpd_logbuf_enable_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "lpd firmware log level: %u\n", lpd_fwlog_level);

	return 0;
}

static ssize_t lpd_logbuf_enable_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	int rc;
	int res;
	struct seq_file *s;
	struct lpd_device *lpd;
	struct lpd_debug_file *debug_file;

	s = file->private_data;
	debug_file = s->private;
	lpd = debug_file->private;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	if (!count)
		return count;

	rc = kstrtoint_from_user(buf, count, 10, &res);
	if (rc)
		return rc;

	lpd_info("res : %d\n", res);

	if (res) {
		lpd_fwlog_level = res;
		lpd_logbuf_start(lpd);
	} else {
		lpd_logbuf_stop(lpd);
	}
	return count;

}

static int lpd_dbg_level_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "lpd log level: %u\n", lpd_log_level);

	return 0;
}

static ssize_t lpd_dbg_level_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	int rc = 0;
	int res;

	if (!count)
		return count;

	rc = kstrtoint_from_user(buf, count, 10, &res);
	if (rc)
		return rc;

	lpd_info("change lpd log level : %d -> %d\n", lpd_log_level, res);

	lpd_log_level = res;

	return count;
}


static int lpd_debug_show(struct seq_file *s, void *unused)
{
	int ret = 0;
	struct lpd_debug_file *debug_file = s->private;

	switch (debug_file->id) {
	case LPD_DEBUGFILE_LEVEL:
		lpd_info("%s: LEVEL\n", __func__);
		ret = lpd_dbg_level_show(s, unused);
		break;
	case LPD_DEBUGFILE_ENABLE:
		lpd_info("%s: ENABLE\n", __func__);
		ret = lpd_logbuf_enable_show(s, unused);
		break;
	}
	return ret;
}


static int lpd_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, lpd_debug_show, inode->i_private);
}

static ssize_t lpd_dbg_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	ssize_t ret = 0;
	struct seq_file *s;
	struct lpd_debug_file *debug_file;

	s = file->private_data;
	debug_file = s->private;

	switch (debug_file->id) {
	case LPD_DEBUGFILE_LEVEL:
		lpd_info("%s: LEVEL\n", __func__);
		ret = lpd_dbg_level_write(file, buf, count, f_ops);
		break;
	case LPD_DEBUGFILE_ENABLE:
		lpd_info("%s: ENABLE\n", __func__);
		ret = lpd_logbuf_enable_write(file, buf, count, f_ops);
		break;
	}

	return ret;

}

static const struct file_operations lpd_debug_fops = {
	.open = lpd_dbg_open,
	.write = lpd_dbg_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

#define LPD_ROOT_DIR_NAME "lpd"

static char *lpd_debugfile_name[] = {
	[LPD_DEBUGFILE_LEVEL] = "dbg_level",
	[LPD_DEBUGFILE_ENABLE] = "logbuf_enable",
};


int lpd_dbg_create_debugfs(struct lpd_device *lpd)
{
	int i;
	struct lpd_debug_info *debug_info;
	struct lpd_debug_fs *debug_fs;
	struct lpd_debug_file *debug_file;

	NULL_CHECK(lpd);

	debug_info = &lpd->debug_info;
	debug_fs = &debug_info->debug_fs;

	debug_fs->root_dir = debugfs_create_dir(LPD_ROOT_DIR_NAME, NULL);
	if (!debug_fs->root_dir) {
		lpd_err("failed to create root debugfs dir(%s)", LPD_ROOT_DIR_NAME);
		return -ENOENT;
	}

	for (i = 0; i < MAX_LPD_DEBUFS; i++) {
		debug_file = kmalloc(sizeof(struct lpd_debug_file), GFP_KERNEL);
		if (!debug_file) {
			lpd_err("failed to alloc memory for debug file(%s)", lpd_debugfile_name[i]);
			return -ENOMEM;
		}
		debug_file->private = lpd;
		debug_file->id = i;
		debug_file->file = debugfs_create_file(lpd_debugfile_name[i], 0660,
				debug_fs->root_dir, debug_file, &lpd_debug_fops);
		if (!debug_file->file) {
			lpd_err("failed to create debug fs(%s)", lpd_debugfile_name[i]);
			kfree(debug_file);
			return -EINVAL;
		}
		debug_fs->debug_file[i] = debug_file;
	}

	return 0;
}

int lpd_dbg_destroy_debugfs(struct lpd_device *lpd)
{
	int i;
	struct lpd_debug_info *debug_info;
	struct lpd_debug_fs *debug_fs;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	debug_info = &lpd->debug_info;
	debug_fs = &debug_info->debug_fs;

	debugfs_remove_recursive(debug_fs->root_dir);

	for (i = 0; i < MAX_LPD_DEBUFS; i++) {
		kfree(debug_fs->debug_file[i]);
		debug_fs->debug_file[i] = NULL;
	}

	return 0;
}

static int lpd_memlog_init(struct lpd_device *lpd)
{

#ifdef CONFIG_EXYNOS_MEMORY_LOGGER
	int ret;

	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	ret = memlog_register("LPD", lpd->dev, &lpd->debug_info.mlog.memlog_lpd);

	if (!lpd->debug_info.mlog.memlog_lpd) {
		lpd_err("memlog lpd registration fail ret:%d\n", ret);
		return -1;
	}

	lpd->debug_info.mlog.memlog_printf_file = memlog_alloc_file(lpd->debug_info.mlog.memlog_lpd,
		"log-fil", SZ_256K, SZ_256K, 1000, 3);
	if (lpd->debug_info.mlog.memlog_printf_file) {
		//memlog_obj_set_sysfs_mode(lpd->d.mlog.memlog_printf_file, true);
		lpd->debug_info.mlog.memlog_printf = memlog_alloc_printf(lpd->debug_info.mlog.memlog_lpd, SZ_256K,
			lpd->debug_info.mlog.memlog_printf_file, "log-mem", 0);
	}

	lpd->debug_info.mlog.memlog_sram_file = memlog_alloc_file(lpd->debug_info.mlog.memlog_lpd,
		"srm-fil", lpd->sram.size, lpd->sram.size, 1000, 3);

	if (lpd->debug_info.mlog.memlog_sram_file) {
		lpd->debug_info.mlog.memlog_sram = memlog_alloc_dump(lpd->debug_info.mlog.memlog_lpd,
			lpd->sram.size, lpd->sram.base, false, lpd->debug_info.mlog.memlog_sram_file, "srm-dmp");
	}

	lpd_info("memlog printf %s sram %s\n",
		lpd->debug_info.mlog.memlog_printf ? "pass" : "fail",
		lpd->debug_info.mlog.memlog_sram ? "pass" : "fail");
#endif
	return 0;
}

int lpd_dbg_init(struct lpd_device *lpd)
{
	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	mutex_init(&lpd->debug_info.logbuf_lock);

	lpd_dbg_create_debugfs(lpd);
	lpd_memlog_init(lpd);

	lpd->debug_info.pm_refcnt = 0;

	return 0;
}

int lpd_dbg_uninit(struct lpd_device *lpd)
{
	if (!lpd) {
		lpd_err("null lpd\n");
		return -EINVAL;
	}

	lpd_dbg_destroy_debugfs(lpd);
	mutex_destroy(&lpd->debug_info.logbuf_lock);

	return 0;
}

