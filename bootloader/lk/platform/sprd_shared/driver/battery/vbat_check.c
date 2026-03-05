//#include <common.h>
//#include "sprd_adc.h"
#include <boot_mode.h>
#include <sprd_battery.h>
#include <sys/types.h>
#include <lk/debug.h>

#ifndef LOW_BAT_VOL
#  define LOW_BAT_VOL		3400
#endif

#ifndef LOW_BAT_VOL_CHG
#  define LOW_BAT_VOL_CHG	3500
#endif

int charger_connected(void);
/*
int32_t ADC_GetValue(unsigned int id, int scale)
{
	int32_t result;

	if (-1 == pmic_adc_get_values(id, scale, 1, &result)) {
		return -1;
	}

	return result;
}
*/

#ifndef LOW_BAT_VOL
#define LOW_BAT_VOL 3400
#endif

int is_bat_low(void)
{
	int32_t vbat_vol;
	uint16_t comp_vbat;

#ifndef BAT_LOW_MODE_SUPPORT
	dprintf(ALWAYS,"sprd_chg: %s BAT_LOW_MODE_SUPPORT not define!!!\n", __func__);
	return 0;
#endif

	if (charger_connected()) {
		comp_vbat = LOW_BAT_VOL_CHG;
	} else {
		comp_vbat = LOW_BAT_VOL;
	}

	vbat_vol = sprdfgu_read_vbat_vol();
	dprintf(ALWAYS,"sprd_chg: %s vbat_vol:%d,comp_vbat:%d\n",
		__func__, vbat_vol, comp_vbat);

	if (vbat_vol < comp_vbat)
		return 1;
	else
		return 0;

}
