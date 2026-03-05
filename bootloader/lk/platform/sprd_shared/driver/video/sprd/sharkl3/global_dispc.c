/*
 *  <global_dispc.c> - <global dispc>
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
#include "../sprd_dispc.h"
#include <lk/reg.h>

#define BIT(x) (1<<(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


static uint32_t dpu_core_clk[] = {
	153600000,
	192000000,
	256000000,
	384000000
};

static uint32_t dpi_clk_src[] = {
	128000000,
	153600000,
	192000000
};

static uint32_t dpi_src_val;

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

static uint8_t calc_dpu_core_clk(uint32_t pclk)
{
	return ARRAY_SIZE(dpu_core_clk) - 1;
}

static uint8_t calc_dpi_clk_src(uint32_t pclk)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpi_clk_src); i++) {
		if ((dpi_clk_src[i] % pclk) == 0)
			return i;
	}

	pr_err("calc DPI_CLK_SRC failed, use default\n");
	return 0;
}

static int dispc_clk_init(struct dispc_context *ctx)
{
	uint8_t core_sel = calc_dpu_core_clk(ctx->panel->pixel_clk);
	uint8_t dpi_sel = calc_dpi_clk_src(ctx->panel->pixel_clk);

	pr_info("DPU_CORE_CLK = %u\n", dpu_core_clk[core_sel]);
	pr_info("DPI_CLK_SRC = %u\n", dpi_clk_src[dpi_sel]);

	sci_glb_write(0x402D02F4,
			core_sel, BIT(0) | BIT(1));

	sci_glb_write(0x402D02F8,
			dpi_sel, BIT(0) | BIT(1));

	dpi_src_val = dpi_clk_src[dpi_sel];

	return 0;
}

static int dispc_clk_update(struct dispc_context *ctx, int clk_id, int val)
{
	uint32_t div;

	div = dpi_src_val / val;
	if (dpi_src_val - div * val > (val / 2))
		div++;
	if ((div == 0) || (div > 0x10)) {
		pr_err("invalid dpi clk dividor (%d)\n", div);
		return -1;
	}

	sci_glb_write(0x402D02F8, (div - 1) << 8, (0xF << 8));

	pr_info("the actual dpi_clk = %d\n", dpi_src_val / div);
	return 0;
}

static int dispc_glb_parse_dt(struct dispc_context *ctx)
{
	//ctx->base = SPRD_DPU_PHYS;
	ctx->base = 0x63000000;

	return 0;
}

static void dispc_glb_enable(struct dispc_context *ctx)
{
	sci_glb_set(0x402E0004, BIT(11));
	sci_glb_set(0x402E0050, BIT(2));
}

static void dispc_glb_disable(struct dispc_context *ctx)
{
	sci_glb_clr(0x402E0050, BIT(2));
	sci_glb_clr(0x402E0004, BIT(11));
}

static void dispc_reset(struct dispc_context *ctx)
{
//after ECO
	sci_glb_set(0x402E000C, BIT(20));
	udelay(10);
	sci_glb_clr(0x402E000C, BIT(20));
}

static void dispc_power_domain(struct dispc_context *ctx, int enable)
{
	if (enable)
		sci_glb_clr(0x402B0058, BIT(25));
	else
		sci_glb_set(0x402B0058, BIT(25));
}

static struct dispc_clk_ops dispc_clk_ops = {
	.init = dispc_clk_init,
	.update = dispc_clk_update,
};

static struct dispc_glb_ops dispc_glb_ops = {
	.parse_dt = dispc_glb_parse_dt,
	.reset = dispc_reset,
	.enable = dispc_glb_enable,
	.disable = dispc_glb_disable,
	.power = dispc_power_domain,
};

struct dispc_clk_ops *dispc_clk_ops_attach(void)
{
	return &dispc_clk_ops;
}

struct dispc_glb_ops *dispc_glb_ops_attach(void)
{
	return &dispc_glb_ops;
}
