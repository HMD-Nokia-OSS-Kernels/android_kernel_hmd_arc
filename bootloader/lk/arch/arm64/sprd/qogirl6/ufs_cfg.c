/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>
#include <sprd_ufs.h>
#include <sprd_efuse.h>
#include <sprd_hwfeature.h>

#define BITS_PER_LONG 64
#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
/* UFS analog registers */
#define MPHY_2T2R_APB_REG1 0x68
#define MPHY_2T2R_APB_RESETN (0x1 << 3)

#define FIFO_ENABLE_MASK (0x1 << 15)

/* UFS mphy registers */
#define MPHY_LANE0_FIFO 0xc08c
#define MPHY_LANE1_FIFO 0xc88c
#define MPHY_TACTIVATE_TIME_LANE0 0xc088
#define MPHY_TACTIVATE_TIME_LANE1 0xc888

#define FIFO_ENABLE_MASK (0x1 << 15)
#define MPHY_TACTIVATE_TIME_200US (0x1 << 17)

/* UFS HC register */
#define HCLKDIV_REG 0xFC
#define CLKDIV 0x100

#define	MPHY_DIG_CFG7_LANE0 0xC01c
#define	MPHY_DIG_CFG7_LANE1 0xC81c
#define	MPHY_CDR_MONITOR_BYPASS_MASK GENMASK(24, 24)
#define	MPHY_CDR_MONITOR_BYPASS_ENABLE BIT(24)

#define	MPHY_DIG_CFG20_LANE0 0xC050
#define	MPHY_RXOFFSETCALDONEOVR_MASK GENMASK(5, 4)
#define	MPHY_RXOFFSETCALDONEOVR_ENABLE (BIT(5) | BIT(4))
#define	MPHY_RXOFFOVRVAL_MASK GENMASK(11, 10)
#define	MPHY_RXOFFOVRVAL_ENABLE (BIT(11) | BIT(10))

#define	MPHY_DIG_CFG49_LANE0 0xC0C4
#define	MPHY_DIG_CFG49_LANE1 0xC8C4
#define	MPHY_RXCFGG1_MASK GENMASK(23, 0)
#define	MPHY_RXCFGG1_VAL (0x0C0C0C << 0)

#define	MPHY_DIG_CFG51_LANE0 0xC0CC
#define	MPHY_DIG_CFG51_LANE1 0xC8CC
#define	MPHY_RXCFGG3_MASK GENMASK(23, 0)
#define	MPHY_RXCFGG3_VAL (0x0D0D0D << 0)

#define	MPHY_DIG_CFG72_LANE0 0xC120
#define	MPHY_DIG_CFG72_LANE1 0xC920
#define	MPHY_RXHSG3SYNCCAP_MASK GENMASK(15, 8)
#define	MPHY_RXHSG3SYNCCAP_VAL (0x4B << 8)

#define	MPHY_DIG_CFG60_LANE0 0xC0F0
#define	MPHY_DIG_CFG60_LANE1 0xC8F0
#define	MPHY_RX_STEP4_CYCLE_G3_MASK GENMASK(31, 16)
#define	MPHY_RX_STEP4_CYCLE_G3_VAL  BIT(23)
#define EFUSE_UFS_BLOCK45 45
#define UFS_TXRX_PHY_MASK GENMASK(11,0)
#define UFS_TXRX_PHY_ADDR 0x58
#define REG32(x) (*((volatile u32 *)(x)))

#define	MPHY_DIG_CFG18_LANE0  (0xc048)
#define	MPHY_APB_PLLTIMER_MASK  GENMASK(23, 16)
#define	MPHY_APB_PLLTIMER_VAL (0xd8<<16)

#define	MPHY_DIG_CFG19_LANE0 (0xc04c)
#define	MPHY_APB_HSTXSCLKINV1_MASK BIT(13)
#define	MPHY_APB_HSTXSCLKINV1_VAL BIT(13)

#define CHIP_ID_AB 1
extern struct hwfeature hwf;
extern unsigned int get_chip_id(struct hwfeature *phwf);

static void ufs_sprd_rmwl(void *base, u32 mask, u32 val, u32 reg)
{
	u32 tmp;
/*
	dprintf(INFO,"regmap_ufs %16x&=%8x\n",(base+reg),~mask);
	dprintf(INFO,"           %16x|=%8x\n",(base+reg),(val & mask));
*/
	tmp = REG32((base) + (reg));
	tmp &= ~mask;
	tmp |= (val & mask);
	REG32((base) + (reg)) = tmp;
}
int set_ufs_trim_from_efuse(void)
{
	unsigned int efuse_reg, ufs_trimbg,  ufs_rxtrim,  ufs_txtrim;

	efuse_reg = sprd_ap_efuse_read(EFUSE_UFS_BLOCK45);
	if (!efuse_reg) {
		debugf("err:read ufs efuse err\n");
		return -1;
	}
	ufs_trimbg = 0xf & (efuse_reg >> 0x0);
	ufs_rxtrim = 0xf & (efuse_reg >> 0x4);
	ufs_txtrim = 0xf & (efuse_reg >> 0x8);

	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, UFS_TXRX_PHY_MASK,
			(ufs_trimbg << 8) | (ufs_txtrim << 4) | (ufs_rxtrim << 0), UFS_TXRX_PHY_ADDR);
	return 0;
}

void init_global_reg(void)
{
	uint32_t chip_id;

	debugf("init_global_reg \n ");

	CHIP_REG_AND(REG_AP_APB_APB_EB1, ~(BIT_AP_APB_UFS_EB));
	CHIP_REG_OR(REG_AP_APB_APB_EB, BIT_AP_APB_APB_REG_EB);
	udelay(10);
	CHIP_REG_AND(REG_AP_APB_APB_EB, ~BIT_AP_APB_APB_REG_EB);
	/* REG_AP_APB_APB_EB1---2000000c */
	CHIP_REG_OR(REG_AP_APB_APB_EB1, (BIT_AP_APB_UFS_EB));

	/* REG_AP_AHB_UFS_CLK_CTRL---0x20403124 */
	CHIP_REG_OR(REG_AP_AHB_UFS_CLK_CTRL,BIT_AP_AHB_CG_CFGCLK_SW);

	/* REG_AON_APB_APB_EB1---0x64000004 */
	ufs_sprd_rmwl(REG_AON_APB_APB_EB1, (BIT_AON_APB_PIN_EB | BIT_AON_APB_ANA_EB), (BIT_AON_APB_PIN_EB | BIT_AON_APB_ANA_EB) ,0x0);

	//cbline reset
	CHIP_REG_OR(REG_AP_AHB_MPHY_CB_CHANNEL_0, (BIT_AP_AHB_CB_CFGCLK | BIT_AP_AHB_CB_RESET));
	udelay(100);

	/* apb reset */
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_2T2R_APB_RESETN,
			0, MPHY_2T2R_APB_REG1);
	mdelay(1);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_2T2R_APB_RESETN,
			MPHY_2T2R_APB_RESETN, MPHY_2T2R_APB_REG1);

	/* phy config */
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE1);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXOFFSETCALDONEOVR_MASK,
			MPHY_RXOFFSETCALDONEOVR_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXOFFOVRVAL_MASK,
			MPHY_RXOFFOVRVAL_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE1);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE1);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE0_FIFO);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE1_FIFO);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE1);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXHSG3SYNCCAP_MASK,
			MPHY_RXHSG3SYNCCAP_VAL, MPHY_DIG_CFG72_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RXHSG3SYNCCAP_MASK,
			MPHY_RXHSG3SYNCCAP_VAL, MPHY_DIG_CFG72_LANE1);

	/* add cdr count time */
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RX_STEP4_CYCLE_G3_MASK,
			MPHY_RX_STEP4_CYCLE_G3_VAL, MPHY_DIG_CFG60_LANE0);
	ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_RX_STEP4_CYCLE_G3_MASK,
			MPHY_RX_STEP4_CYCLE_G3_VAL, MPHY_DIG_CFG60_LANE1);

	//cbline reset
	CHIP_REG_AND(REG_AP_AHB_MPHY_CB_CHANNEL_0,
			 ~(BIT_AP_AHB_CB_CFGCLK | BIT_AP_AHB_CB_RESET));

	//enable refclk
	CHIP_REG_OR(REG_AP_AHB_MPHY_CB_CHANNEL_0,BIT_AP_AHB_REFCLKON);

	/* REG_AP_AHB_UFS_LP_CTRL_1---0x20403104 */
	CHIP_REG_OR(REG_AP_AHB_UFS_LP_CTRL_1,
			 (BIT_AP_AHB_UFS_FORCE_LP_RESET_N |
			BIT_AP_AHB_UFS_FORCE_LP_PWR_READY | BIT_AP_AHB_UFS_SEL_LP_PWR_READY |
			BIT_AP_AHB_UFS_SEL_LP_RESET_N | BIT_AP_AHB_UFS_SEL_LP_ISOL_EN));

	CHIP_REG_AND(REG_AP_AHB_UFS_LP_CTRL_1,
			 ~BIT_AP_AHB_UFS_FORCE_LP_ISOL_EN);

	//remove this first keep the same with the romcode
	/* REG_AP_APB_APB_RST 0x20000004 */
	CHIP_REG_OR(REG_AP_APB_APB_RST,(BIT_AP_APB_UFS_REG_SOFT_RST));
	udelay(100);
	CHIP_REG_AND(REG_AP_APB_APB_RST,~(BIT_AP_APB_UFS_REG_SOFT_RST));

	udelay(10);
	chip_id = get_chip_id(&hwf);
	if (chip_id == CHIP_ID_AB) {
		ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_APB_PLLTIMER_MASK,
				MPHY_APB_PLLTIMER_VAL, MPHY_DIG_CFG18_LANE0);
		ufs_sprd_rmwl(CTL_ANLG_PHY_G1_BASE, MPHY_APB_HSTXSCLKINV1_MASK,
				MPHY_APB_HSTXSCLKINV1_VAL, MPHY_DIG_CFG19_LANE0);
	}

	udelay(100);
	set_ufs_trim_from_efuse();

}

int ufshci_enable_bottom(void)
{
	return 0;
}

struct ufs_hba_variant_ops hba_vops = {
	.name = "qogirl6",
	.enable_bottom = ufshci_enable_bottom,
};
