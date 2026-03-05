//#include <common.h>
#include <sprd_battery.h>
#include <i2c.h>
typedef unsigned char BYTE;

enum sprdchg_cmd {
	CHG_UNKNOWN,
	CHG_CHIP_INIT,
	CHG_START,
	CHG_STOP,
	CHG_SET_CURRENT,
	CHG_SET_LIMIT_CURRENT,
	CHG_SET_PRE_CURRENT,
	CHG_SET_POWER_PATH,
};

struct sprdchg_ic_operations {
	void (*chg_start)(void);
	void (*chg_stop)(void);
	void (*timer_callback)(void);
	void (*chg_cmd)(enum sprdchg_cmd cmd, int value);
	int (*is_support_power_path)(void);
};

int sprdchg_charger_is_adapter(void);
void chg_low_bat_chg(void);
void sprdbat_help_init(void);
void sprdfgu_init(void);
void sprdfgu_late_init(void);
const struct sprdchg_ic_operations *sprdchg_ic_ops_get(void);
void sprdchg_register_ops(const struct sprdchg_ic_operations* ops);