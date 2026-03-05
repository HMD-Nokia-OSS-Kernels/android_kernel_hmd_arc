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

#define udelay(x) \
	do { \
		volatile int i; \
		int cnt = 200 * (x); \
		for (i=0; i<cnt; i++);\
	} while(0);

#define mdelay(_ms) udelay((_ms)*1000)
//extern void Musb_mdelay(uint32_t msecs);

#define USB20_TUNEHSAMP 0x3
#define USB20_TUNEEQ 0x0
#define USB20_TFREGRES 0x2

void usb_phy_init(void)
{
	unsigned int tmp = readl(REG_ANLG_PHY_G4_ANALOG_USB20_USB20_TRIMMING);
	dprintf(INFO,"before eye pattern = 0x%x\n", tmp);
	/* enable usb module*/
	CHIP_REG_OR(REG_AP_AHB_AHB_EB, BIT_USB_EB);
	CHIP_REG_OR(REG_AON_APB_APB_EB2,
				BIT_AON_APB_ANLG_APB_EB | BIT_AON_APB_ANLG_EB);


	CHIP_REG_OR(REG_ANLG_PHY_G4_ANALOG_USB20_IDDG,
				BIT_ANLG_PHY_G4_ANALOG_USB20_UTMIOTG_IDDG);
	/*PHY bus valid*/
	CHIP_REG_OR(REG_AP_AHB_OTG_PHY_TEST,
				BIT_AP_AHB_OTG_VBUS_VALID_PHYREG);
	CHIP_REG_OR(REG_ANLG_PHY_G4_ANALOG_USB20_USB20_UTMI_CTL1,
				BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_DATABUS16_8 |
				BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_VBUSVLDEXT);

	/*PHY width: 16 bit*/
	CHIP_REG_OR(REG_AP_AHB_OTG_PHY_CTRL,
				BIT_AP_AHB_UTMI_WIDTH_SEL);

	/*config eye pattern*/
	tmp &= ~BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_TUNEHSAMP(0x3);
	tmp |= BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_TUNEHSAMP(USB20_TUNEHSAMP);
	tmp &= ~BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_TUNEEQ(0x7);
	tmp |= BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_TUNEEQ(USB20_TUNEEQ);
	//tmp &= ~BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_TFREGRES(0x1f);
	//tmp |= BIT_ANLG_PHY_G4_ANALOG_USB20_USB20_TFREGRES(USB20_TFREGRES);
	writel(tmp, REG_ANLG_PHY_G4_ANALOG_USB20_USB20_TRIMMING);
	dprintf(INFO,"eye pattern = 0x%x\n", __raw_readl(REG_ANLG_PHY_G4_ANALOG_USB20_USB20_TRIMMING));
	/*Soft reset phy*/
	CHIP_REG_OR(REG_AP_AHB_AHB_RST,
				BIT_OTG_SOFT_RST | BIT_OTG_UTMI_SOFT_RST);
	CHIP_REG_OR(REG_AON_APB_APB_RST2,  BIT_OTG_PHY_SOFT_RST);
	mdelay(5);
	CHIP_REG_AND(REG_AP_AHB_AHB_RST,
				~(BIT_OTG_SOFT_RST | BIT_OTG_UTMI_SOFT_RST));
	CHIP_REG_AND(REG_AON_APB_APB_RST2,
				~BIT_OTG_PHY_SOFT_RST);
}

