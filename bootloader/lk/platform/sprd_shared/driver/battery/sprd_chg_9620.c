#include <regs_adi.h>
#include "adi_hal_internal.h"
#include <asm/arch/sprd_reg.h>
#include <sprd_battery.h>
#include "sprd_chg_helper.h"
#include <delay.h>

int sprd_charge_pd_control(bool enable)
{
	if (enable)
		ANA_REG_MSK_OR(ANA_REG_GLB_CHGR_CTRL, 0, BIT_CHGR_PD);
	else
		ANA_REG_MSK_OR(ANA_REG_GLB_CHGR_CTRL, BIT_CHGR_PD,
			       BIT_CHGR_PD);
	return 0;
}

void sprdchg_common_cfg(void)
{
}

int sprdchg_charger_is_adapter(void)
{
	int ret = ADP_TYPE_UNKNOW;
	int charger_status, cnt = SPRD_CHGDET_CNT;

	do {
		charger_status = sci_adi_read(ANA_REG_GLB_CHGR_STATUS);
		if (charger_status & BIT_CHG_DET_DONE)
			break;
		udelay(SPRD_CHGDET_DELAY_US);
	} while (--cnt > 0);

	charger_status &= (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);

	switch (charger_status) {
	case BIT_CDP_INT:
		ret = ADP_TYPE_CDP;
		break;
	case BIT_DCP_INT:
		ret = ADP_TYPE_DCP;
		break;
	case BIT_SDP_INT:
		ret = ADP_TYPE_SDP;
		break;
	default:
		break;
	}
	return ret;
}
