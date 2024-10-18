/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * notifer and sysevent interface to communicate with other drivers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "lpd.h"
#include <soc/samsung/exynos-lpd.h>


#if IS_ENABLED(CONFIG_LPD_AUTO_BR)

static BLOCKING_NOTIFIER_HEAD(lpd_config_notifier_list);

int lpd_config_notifier_register(struct notifier_block *nb)
{
	if (nb == NULL) {
		lpd_err("nb is null\n");
		return -EINVAL;
	}
	return blocking_notifier_chain_register(&lpd_config_notifier_list, nb);
}
EXPORT_SYMBOL(lpd_config_notifier_register);


int lpd_config_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lpd_config_notifier_list, nb);
}
EXPORT_SYMBOL(lpd_config_notifier_unregister);


int lpd_config_notifier_call(u32 action, void *data)
{
	return blocking_notifier_call_chain(&lpd_config_notifier_list, action, data);
}
EXPORT_SYMBOL(lpd_config_notifier_call);

#endif

