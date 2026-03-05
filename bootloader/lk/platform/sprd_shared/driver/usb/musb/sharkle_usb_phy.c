/******************************************************************************
 ** File Name:      sprd_musb2_driver.c                                        *
 ** Author:         chunhou.wang                                              *
 ** DATE:           07/11/2016                                                *
 ** Copyright:      2010 Spreatrum, Incoporated. All Rights Reserved.         *
 ** Description:                                                              *
 ******************************************************************************/
/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/



#include <sprd_common.h>
#include <asm/arch/sprd_reg.h>
#include "sprd_musb2_def.h"

#define USB20_TUNEHSAMP 0x2
#define USB20_TUNEEQ 0x0
#define USB20_TFREGRES 0x2

#define udelay(x) \
	do { \
		volatile int i; \
		int cnt = 200 * (x); \
		for (i=0; i<cnt; i++);\
	} while(0);

#define mdelay(_ms) udelay((_ms)*1000)
//extern void Musb_mdelay(uint32_t msecs);
static void __raw_bits_and(unsigned int v, unsigned int a)
{
	__raw_writel((__raw_readl(a) & v), a);
}

static void __raw_bits_or(unsigned int v, unsigned int a)
{
	__raw_writel((__raw_readl(a) | v), a);
}

void usb_phy_init(void)
{
	unsigned int tmp0, tmp1;

	__raw_bits_or(BIT_24|BIT_22, REG_AP_AHB_OTG_PHY_TEST);

	__raw_bits_and(~(BIT_29|BIT_30), REG_AP_AHB_OTG_PHY_CTRL);//SPRD USB PHY width: 8 bit

	/*config device eye pattern*/
  	tmp0 = __raw_readl(REG_AP_AHB_OTG_CTRL0);
	dprintf(INFO,"before eye pattern REG_AP_AHB_OTG_CTRL0 = 0x%x\n", tmp0);
	tmp0 &= ~BIT_AP_AHB_USB20_TUNEHSAMP(0x3);
	tmp0 |= BIT_AP_AHB_USB20_TUNEHSAMP(USB20_TUNEHSAMP);
	__raw_writel(tmp0, REG_AP_AHB_OTG_CTRL0);
	tmp1 = __raw_readl(REG_AP_AHB_OTG_CTRL1);
	dprintf(INFO,"before eye pattern REG_AP_AHB_OTG_CTRL1 = 0x%x\n", tmp1);
	tmp1 &= ~BIT_AP_AHB_USB20_TUNEEQ(0x7);
	tmp1 |= BIT_AP_AHB_USB20_TUNEEQ(USB20_TUNEEQ);
	__raw_writel(tmp1,REG_AP_AHB_OTG_CTRL1);
	//tmp1 = __raw_readl(REG_AP_AHB_OTG_CTRL1);
	//tmp1 &= ~BIT_AP_AHB_USB20_TFREGRES(0x1f);
	//tmp1 |= BIT_AP_AHB_USB20_TFREGRES(USB20_TFREGRES);
	//__raw_writel(tmp, REG_AP_AHB_OTG_CTRL1);
	dprintf(INFO,"eye pattern REG_AP_AHB_OTG_CTRL0 = 0x%x\n", __raw_readl(REG_AP_AHB_OTG_CTRL0));
	dprintf(INFO,"eye pattern REG_AP_AHB_OTG_CTRL1 = 0x%x\n", __raw_readl(REG_AP_AHB_OTG_CTRL1));

	__raw_bits_or(BIT_20, REG_AP_AHB_OTG_CTRL1);//utmi_rst

	__raw_bits_or(BIT_OTG_SOFT_RST|BIT_OTG_UTMI_SOFT_RST|BIT_OTG_PHY_SOFT_RST, REG_AP_AHB_AHB_RST);
	mdelay(5);
	__raw_bits_and(~(BIT_OTG_SOFT_RST|BIT_OTG_UTMI_SOFT_RST|BIT_OTG_PHY_SOFT_RST),REG_AP_AHB_AHB_RST);
}

