/*
 *  <sprd_dphy.c> - <sprd dphy>
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

#include "sprd_dphy.h"

struct sprd_dphy dphy_device;
struct sprd_dphy dphy_slave_device;

int sprd_regmap_write(void *context, unsigned int reg,
				       unsigned int val)
{
	struct sprd_dphy *dphy = context;

	if (val > 0xff || reg > 0xff)
		return -EINVAL;

	mipi_dphy_test_write(dphy, reg, val);

	return 0;
}

int sprd_regmap_read(void *context, unsigned int reg,
				      unsigned int *val)
{
	struct sprd_dphy *dphy = context;
	int ret;

	if (reg > 0xff)
		return -EINVAL;

	ret = mipi_dphy_test_read(dphy, reg);
	if (ret < 0)
		return ret;

	*val = ret;

	return 0;
}

static int sprd_dphy_resume(struct sprd_dphy *dphy)
{
	int ret;

	if (dphy->glb && dphy->glb->power)
		dphy->glb->power(&dphy->ctx, true);
	if (dphy->glb && dphy->glb->enable)
		dphy->glb->enable(&dphy->ctx);

	ret = mipi_dphy_configure(dphy);
	if (ret) {
		pr_err("sprd dphy init failed\n");
		return -EINVAL;
	}

	pr_info("dphy init OK\n");

	return ret;
}

int sprd_dphy_suspend(struct sprd_dphy *dphy)
{
	int ret;

	mipi_dphy_data_ulps_en(dphy, true);
	mipi_dphy_clk_ulps_en(dphy, true);

	ret = mipi_dphy_close(dphy);
	if (ret)
		pr_err("sprd dphy close failed\n");

	if (dphy->glb && dphy->glb->disable)
		dphy->glb->disable(&dphy->ctx);
	if (dphy->glb && dphy->glb->power)
		dphy->glb->power(&dphy->ctx, false);

	pr_info("dphy uninit OK\n");
	return ret;
}

static int dphy_context_init(struct sprd_dphy *dphy)
{
	if (dphy->glb && dphy->glb->parse_dt)
		dphy->glb->parse_dt(&dphy->ctx);

	dphy->ctx.regmap = dphy;
	dphy->ctx.freq = dphy->panel->phy_freq;
	dphy->ctx.lanes = dphy->panel->lane_num;

	return 0;
}

int sprd_dphy_probe(void)
{
	struct sprd_dphy *dphy;

	dphy = &dphy_device;
	dphy->panel = panel_info_attach();
	dphy->ppi = dphy_ppi_ops_attach();
	dphy->pll = dphy_pll_ops_attach();
	dphy->glb = dphy_glb_ops_attach();

	dphy_context_init(dphy);
	sprd_dphy_resume(dphy);

	return 0;
}

int sprd_dphy_slave_probe(void)
{
	struct sprd_dphy *dphy_s;

	dphy_s = &dphy_slave_device;
	dphy_s->panel = panel_info_attach();
	dphy_s->ppi = dphy_ppi_ops_attach();
	dphy_s->pll = dphy_pll_ops_attach();
	dphy_s->glb = dphy_s_glb_ops_attach();
	dphy_context_init(dphy_s);
	sprd_dphy_resume(dphy_s);

	return 0;
}
