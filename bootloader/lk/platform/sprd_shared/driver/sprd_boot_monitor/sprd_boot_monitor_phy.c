#include <linux/types.h>
#include <asm/arch/sprd_reg.h>
#include "sprd_boot_monitor.h"

const char* err_string[]={
	"err in uboot0",
	"err in uboot1",
	"err in uboot2",
	"err in uboot3",
	"err in uboot4",
	"err in uboot5"
};

void sprd_boot_monitor_write(sprd_boot_monitor_t err_stage)
{
	// for UBOOT
	if ((err_stage > 0x78) && (err_stage < 0xA1)){
		ANA_REG_AND(ANA_REG_GLB_POR_RST_MONITOR, 0xFF);
		ANA_REG_OR(ANA_REG_GLB_POR_RST_MONITOR, err_stage << 8);
	}else {
		pr_err("uns_boot_monitor_write input: %x overflow (0x79-0xa0) \n", err_stage);
	}
}

const char* sprd_boot_monitor_read(void)
{
	u32 err_stage = (ANA_REG_GET(ANA_REG_GLB_POR_RST_MONITOR) & 0xFF00)>> 8;
	return err_string[err_stage - err_uboot0];
}
