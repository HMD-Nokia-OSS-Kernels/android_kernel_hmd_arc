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

//#include <asm/arch/sprd_reg.h>
//#include <sprd_glb.h>
#include "../sprd_dphy.h"
#include <lk/reg.h>

#define BIT(x) (1<<(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


static int __is_glb(u32 reg)
{
	//return rounddown(reg, SZ_64K) == rounddown(GREG_BASE, SZ_64K) || rounddown(reg, SZ_64K) == rounddown(AHB_GEN_CTL_BEGIN, SZ_64K);
	return 1;
}

static int sci_glb_set(u32 reg, u32 bit)
{
	if (__is_glb(reg))
		__raw_writel(__raw_readl(reg) | bit, reg);

	return 0;
}

static int sci_glb_write(u32 reg, u32 val, u32 msk)
{
	unsigned long flags, hw_flags;
	__raw_writel((__raw_readl(reg) & ~msk) | val, reg);
	return 0;
}

static int sci_glb_clr(u32 reg, u32 bit)
{
	if (__is_glb(reg))
		__raw_writel((__raw_readl(reg) & ~bit), reg);

	return 0;
}

static int dphy_glb_parse_dt(struct dphy_context *ctx)
{
	ctx->ctrlbase = 0x63100000;

	return 0;
}

static void dphy_glb_enable(struct dphy_context *ctx)
{
	sci_glb_set(0x402E00B0, BIT(24) | BIT(23));
}

static void dphy_glb_disable(struct dphy_context *ctx)
{
	sci_glb_clr(0x402E00B0, BIT(24) | BIT(23));
}

static void dphy_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		sci_glb_clr(0x402E0024, BIT(15));
		udelay(10);
		sci_glb_clr(0x402E0024, BIT(14));
		sci_glb_clr(0x402E0024, BIT(27));
	} else {
		sci_glb_set(0x402E0024, BIT(27));
		sci_glb_set(0x402E0024, BIT(14));
		udelay(10);
		sci_glb_set(0x402E0024, BIT(15));
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
