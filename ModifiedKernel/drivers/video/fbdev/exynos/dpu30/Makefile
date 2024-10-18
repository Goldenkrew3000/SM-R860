#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#


#wear os 11 first release, T os: 13

ifeq ($(findstring 13, $(PLATFORM_VERSION)), 13)
ccflags-y += -DCONFIG_EXYNOS_DPU_LEGACY
endif

obj-$(CONFIG_EXYNOS_DPP) += dpp.o
dpp-y := dpp_drv.o
obj-$(CONFIG_EXYNOS_MIPI_DSIM) += dsim.o
dsim-y += dsim_drv.o
obj-$(CONFIG_SOC_S5E5515) += ./cal_5515/dsim_reg.o ./cal_5515/dpp_reg.o ./cal_5515/decon_reg.o
obj-$(CONFIG_EXYNOS_DPU30) += decon.o
obj-$(CONFIG_EXYNOS_BTS) += bts.o
decon-y += decon_core.o decon_dsi.o helper.o win_update.o decon_wb.o cursor.o fence.o event_log.o format.o
decon-$(CONFIG_MCD_PANEL) += mcd_decon.o
obj-y += panels/
