#ifndef __UDC_H__
#define __UDC_H__


#include <lcd.h>
#include <errno.h>
#include <stdlib.h>

#include <part.h>

#include <../driver/video/sprd/sprd_dphy.h>
#include <../driver/video/sprd/sprd_dsi.h>
#include <../driver/video/sprd/sprd_panel.h>
#include <../driver/video/sprd/dsi/mipi_dsi_api.h>

#include <../soc/sharkl3/include/asm/arch/mfp.h>


#include "gpio_plus.h"
#include <linux/types.h>

#include <string.h>
#include <android_bootimg.h>
//#include <linux/mtd/mtd.h>
//#include <linux/mtd/nand.h>


//#include <environment.h>

#include <boot_mode.h>
#include <malloc.h>

#ifdef CONFIG_EMMC_BOOT
#include <asm/arch/common.h>
#endif

#ifdef CONFIG_UFS_BOOT
#include <asm/arch/common.h>
#endif
//#ifdef CONFIG_UDC_SC9832E
#include <../driver/udc/include/udc_base.h>
#include <../driver/udc/include/udc_lcd.h>

//#endif

#endif

 
