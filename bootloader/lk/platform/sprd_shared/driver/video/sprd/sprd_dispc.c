/*
 *  <sprd_dispc.c> - <sprd dispc>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2021-08-09> <pony.wu@unisoc.com>
 *
 *      The above copyright notice shall be
 *      included in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "sprd_dispc.h"
#include "sprd_reg_rw.h"


struct sprd_dispc dispc_device;

void sprd_dispc_run(struct sprd_dispc *dispc)
{
	struct dispc_context *ctx = &dispc->ctx;

	if (!ctx->is_inited) {
		pr_err("dispc is not initialized!\n");
		return;
	}

	if (dispc->core)
		dispc->core->run(ctx);
}

void sprd_dispc_stop(struct sprd_dispc *dispc)
{
	struct dispc_context *ctx = &dispc->ctx;

	if (!ctx->is_inited) {
		pr_err("dispc is not initialized!\n");
		return;
	}

	if (dispc->core)
		dispc->core->stop(ctx);
}

int32_t sprd_dispc_flip(struct sprd_dispc *dispc,
			struct sprd_restruct_config *config)
{
	struct dispc_context *ctx = &dispc->ctx;
	int wait_cnt = 0;

	if (!ctx->is_inited) {
		pr_err("dispc is not initialized!\n");
		return -1;
	}

	if (!dispc->core)
		return -1;

	dispc->core->flip(ctx, config);

	if (ctx->is_single_run) {
		if (dispc->core->check_dpu_stop) {
			while(!dispc->core->check_dpu_stop(ctx)) {
				mdelay(5);
				wait_cnt++;
				if (wait_cnt > 20) {
					pr_err("dispc wait dpu stop timeout, skip!\n");
					break;
				}
			}
		}
		dispc->core->run(ctx);
	} else if ((!ctx->is_stopped) || (dispc->ctx.if_type == SPRD_DISPC_IF_EDPI)) {
		dispc->core->run(ctx);
	}

	return 0;
}

int32_t sprd_dispc_bgcolor(struct sprd_dispc *dispc, u32 color)
{
	struct dispc_context *ctx = &dispc->ctx;

	if (!ctx->is_inited) {
		pr_err("dispc is not initialized!\n");
		return -1;
	}

	if (!dispc->core)
		return -1;

	dispc->core->bg_color(ctx, color);

	if (!ctx->is_stopped || (dispc->ctx.if_type == SPRD_DISPC_IF_EDPI))
		dispc->core->run(ctx);

	return 0;
}

static int32_t sprd_dispc_suspend(struct sprd_dispc *dispc)
{
	struct dispc_context *ctx = &dispc->ctx;

	if (!ctx->is_inited) {
		pr_err("dispc is not initialized\n");
		return -1;
	}

	if (dispc->core)
		dispc->core->uninit(ctx);

	if (dispc->glb) {
		dispc->glb->disable(ctx);
		dispc->glb->power(ctx, false);
	}

	ctx->is_inited = false;

	pr_err("dispc suspend OK\n");

	return 0;
}

static int32_t sprd_dispc_resume(struct sprd_dispc *dispc)
{
	struct dispc_context *ctx = &dispc->ctx;
	struct panel_info *panel = ctx->panel;

	if (ctx->is_inited) {
		pr_err("dispc has already initialized\n");
		return -1;
	}

	if (dispc->glb) {
		dispc->glb->power(ctx, true);
		dispc->glb->enable(ctx);
		dispc->glb->reset(ctx);
	}

	if (dispc->clk) {
		dispc->clk->init(ctx);
		dispc->clk->update(ctx, DISPC_CLK_ID_DPI, panel->pixel_clk);
	}

	if (dispc->core) {
		dispc->core->init(ctx);
		dispc->core->ifconfig(ctx);
		dispc->core->run(ctx);
	}

	ctx->is_inited = true;

	//sprd_iommu_restore(&dispc->dev);

	pr_info("dispc init OK\n");
	return 0;
}

static int dispc_context_init(struct sprd_dispc *dispc)
{
	struct dispc_context *ctx = &dispc->ctx;
	struct panel_info *panel = panel_info_attach();

	if (dispc->clk && dispc->clk->parse_dt)
		dispc->clk->parse_dt(&dispc->ctx);
	if (dispc->glb && dispc->glb->parse_dt)
		dispc->glb->parse_dt(&dispc->ctx);

	ctx->id = 0;
	ctx->is_stopped = true;
	ctx->panel = panel;

	switch (panel->type) {
	case SPRD_PANEL_TYPE_RGB:
	case SPRD_PANEL_TYPE_LVDS:
		ctx->if_type = SPRD_DISPC_IF_DPI;
		break;
	case SPRD_PANEL_TYPE_MIPI:
		if ((panel->work_mode == SPRD_MIPI_MODE_VIDEO)
			|| ((panel->work_mode == SPRD_MIPI_MODE_CMD) && panel->dsc_en))
			ctx->if_type = SPRD_DISPC_IF_DPI;
		else
			ctx->if_type = SPRD_DISPC_IF_EDPI;
		break;
	case SPRD_PANEL_TYPE_MCU:
		ctx->if_type = SPRD_DISPC_IF_EDPI;
		break;
	default:
		ctx->if_type = SPRD_DISPC_IF_DPI;
		break;
	}

	return 0;
}

int sprd_dispc_probe(void)
{
	struct sprd_dispc *dispc;

	dispc = &dispc_device;

	dispc->core = dispc_core_ops_attach();
	dispc->clk = dispc_clk_ops_attach();
	dispc->glb = dispc_glb_ops_attach();

	dispc_context_init(dispc);
	sprd_dispc_resume(dispc);

	return 0;
}
