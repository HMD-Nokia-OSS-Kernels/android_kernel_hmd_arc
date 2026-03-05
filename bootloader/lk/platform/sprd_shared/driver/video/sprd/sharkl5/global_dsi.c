/*
 *  <global_dsi.c> - <global dsi>
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

#include <asm/arch/sprd_reg.h>
#include <sprd_glb.h>
#include "../sprd_dsi.h"
#include <lk/reg.h>

#define BIT(x) (1<<(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int dsi_glb_parse_dt(struct dsi_context *ctx)
{
	ctx->base = SPRD_DSI_PHYS;
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
	return &dsi_glb_ops;
}
