
#ifndef _SPRD_BOOT_MONITOR_H_
#define _SPRD_BOOT_MONITOR_H_

#include "adi_hal_internal.h"
#include "sprd_common.h"



/*
 *Definition of ANA_RST_STATUS(ANA_REG_GLB_POR_RST_MONITOR) register
 *bit[8..15]: Error stage (1-255)
 *1		-40		: SPL
 *41		-80 		: SML
 *81		-120 	: CH
 *121	-160	: UBOOT
 *161	-200	: KNL
 *201	-240	: CM4
 *(add you need)
 *After making changes in sprd_boot_monitor_t，please synchronize them to err_string(in sprd_boot_monitor_phy.c)
 *  */

typedef enum{
	//eg: SPRD_BOOT_SPL_ERR						0x1
	err_uboot0 = 0x79,
	err_uboot1,
	err_uboot2,
	err_uboot3,
	err_uboot4,
	err_uboot5
}sprd_boot_monitor_t;

void sprd_boot_monitor_write(sprd_boot_monitor_t err_stage);

const char* sprd_boot_monitor_read(void);

#endif /* _SPRD_BOOT_MONITOR_H_ */



