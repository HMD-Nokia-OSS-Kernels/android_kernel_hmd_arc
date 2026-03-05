/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */

#include <asm/arch/sprd_reg.h>
#include <sprd_glb.h>
#include "../sprd_dsi.h"

static int dsi_glb_parse_dt(struct dsi_context *ctx)
{
	ctx->base = SPRD_DSI_PHYS;
    pr_info("dsi_glb_parse_dt SPRD_DSI_PHYS=%d\n", SPRD_DSI_PHYS);
	return 0;
}

static void dsi_glb_enable(struct dsi_context *ctx)
{
	sci_glb_set(REG_AP_AHB_AHB_EB, BIT_AP_AHB_DSI_EB);
}

static void dsi_glb_disable(struct dsi_context *ctx)
{
	sci_glb_clr(REG_AP_AHB_AHB_EB, BIT_AP_AHB_DSI_EB);
}

static void dsi_reset(struct dsi_context *ctx)
{
	sci_glb_set(REG_AP_AHB_AHB_RST, BIT_AP_AHB_DSI_SOFT_RST);
	udelay(10);
	sci_glb_clr(REG_AP_AHB_AHB_RST, BIT_AP_AHB_DSI_SOFT_RST);
}

static void dsi_power_domain(struct dsi_context *ctx, int enable)
{
}

static struct dsi_glb_ops dsi_glb_ops = {
	.parse_dt = dsi_glb_parse_dt,
	.reset = dsi_reset,
	.enable = dsi_glb_enable,
	.disable = dsi_glb_disable,
	.power = dsi_power_domain,
};

struct dsi_glb_ops *dsi_glb_ops_attach(void)
{
    pr_info("dsi_glb_ops_attach pike2\n");
	return &dsi_glb_ops;
}

