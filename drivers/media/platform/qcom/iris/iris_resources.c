// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/interconnect.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include "iris_core.h"
#include "iris_resources.h"

#define BW_THRESHOLD 50000

int iris_set_icc_bw(struct iris_core *core, unsigned long icc_bw)
{
	unsigned long bw_kbps = 0, bw_prev = 0;
	const struct icc_info *icc_tbl;
	int ret = 0, i;

	icc_tbl = core->iris_platform_data->icc_tbl;

	for (i = 0; i < core->icc_count; i++) {
		if (!strcmp(core->icc_tbl[i].name, "video-mem")) {
			bw_kbps = icc_bw;
			bw_prev = core->power.icc_bw;

			bw_kbps = clamp_t(typeof(bw_kbps), bw_kbps,
					  icc_tbl[i].bw_min_kbps, icc_tbl[i].bw_max_kbps);

			if (abs(bw_kbps - bw_prev) < BW_THRESHOLD && bw_prev)
				return ret;

			core->icc_tbl[i].avg_bw = bw_kbps;

			core->power.icc_bw = bw_kbps;
			break;
		}
	}

	ret = icc_bulk_set_bw(core->icc_count, core->icc_tbl);
	if (ret)
		dev_err(core->dev, "failed to unset icc bw\n");

	return ret;
}

int iris_unset_icc_bw(struct iris_core *core)
{
	int ret, i;

	core->power.icc_bw = 0;

	for (i = 0; i < core->icc_count; i++) {
		core->icc_tbl[i].avg_bw = 0;
		core->icc_tbl[i].peak_bw = 0;
	}

	ret = icc_bulk_set_bw(core->icc_count, core->icc_tbl);
	if (ret)
		dev_err(core->dev, "failed to unset icc bw\n");

	return ret;
}

int iris_opp_set_rate(struct iris_core *core, u64 freq)
{
	int ret;

	ret = dev_pm_opp_set_rate(core->dev, freq);
	if (ret) {
		dev_err(core->dev, "failed to set rate\n");
		return ret;
	}

	return ret;
}

int iris_enable_power_domains(struct iris_core *core, struct device *pd_dev)
{
	int ret;

	ret = iris_opp_set_rate(core, ULONG_MAX);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(pd_dev);
	if (ret < 0)
		return ret;

	return ret;
}

int iris_disable_power_domains(struct iris_core *core, struct device *pd_dev)
{
	int ret;

	ret = iris_opp_set_rate(core, 0);
	if (ret)
		return ret;

	ret = pm_runtime_put_sync(pd_dev);
	if (ret)
		return ret;

	return ret;
}

static struct clk *iris_get_clk_by_type(struct iris_core *core, enum platform_clk_type clk_type)
{
	const struct platform_clk_data *clk_tbl;
	u32 clk_cnt;
	int i, j;

	clk_tbl = core->iris_platform_data->clk_tbl;
	clk_cnt = core->iris_platform_data->clk_tbl_size;

	for (i = 0; i < clk_cnt; i++) {
		if (clk_tbl[i].clk_type == clk_type) {
			for (j = 0; core->clock_tbl && j < core->clk_count; j++) {
				if (!strcmp(core->clock_tbl[j].id, clk_tbl[i].clk_name))
					return core->clock_tbl[j].clk;
			}
		}
	}

	return NULL;
}

int iris_prepare_enable_clock(struct iris_core *core, enum platform_clk_type clk_type)
{
	struct clk *clock;
	int ret = 0;

	clock = iris_get_clk_by_type(core, clk_type);
	if (!clock) {
		dev_err(core->dev, "failed to get clk: %d\n", clk_type);
		return -EINVAL;
	}

	ret = clk_prepare_enable(clock);
	if (ret) {
		dev_err(core->dev, "failed to enable clock %d\n", clk_type);
		return ret;
	}

	return ret;
}

int iris_disable_unprepare_clock(struct iris_core *core, enum platform_clk_type clk_type)
{
	struct clk *clock;

	clock = iris_get_clk_by_type(core, clk_type);
	if (!clock) {
		dev_err(core->dev, "failed to get clk: %d\n", clk_type);
		return -EINVAL;
	}

	clk_disable_unprepare(clock);

	return 0;
}

int iris_reset_ahb2axi_bridge(struct iris_core *core)
{
	u32 rst_tbl_size;
	int ret;

	rst_tbl_size = core->iris_platform_data->clk_rst_tbl_size;

	ret = reset_control_bulk_reset(rst_tbl_size, core->resets);
	if (ret)
		dev_err(core->dev, "failed to toggle resets: %d\n", ret);

	return ret;
}
