/*
 *  <global_dphy.c> - <global dphy>
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
#include "../sprd_dphy.h"
#include <lk/reg.h>

#define BIT(x) (1<<(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int dphy_glb_parse_dt(struct dphy_context *ctx)
{
	ctx->ctrlbase = SPRD_DSI_PHYS;

	return 0;
}

static void dphy_glb_enable(struct dphy_context *ctx)
{
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN,
		BIT_AP_AHB_DPHY_REF_CKG_EN | BIT_AP_AHB_DPHY_CFG_CKG_EN);
}

static void dphy_glb_disable(struct dphy_context *ctx)
{
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN,
		BIT_AP_AHB_DPHY_REF_CKG_EN | BIT_AP_AHB_DPHY_CFG_CKG_EN);
}

static void dphy_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		sci_glb_clr(REG_PMU_APB_ANALOG_PHY_PD_CFG, BIT_PMU_APB_DSI_PD_REG);
	} else {
		sci_glb_set(REG_PMU_APB_ANALOG_PHY_PD_CFG, BIT_PMU_APB_DSI_PD_REG);
	}
}

static struct dphy_glb_ops dphy_glb_ops = {
	.parse_dt = dphy_glb_parse_dt,
	.enable = dphy_glb_enable,
	.disable = dphy_glb_disable,
	.power = dphy_power_domain,
};

struct dphy_glb_ops *dphy_glb_ops_attach(void)
{
	return &dphy_glb_ops;
}
