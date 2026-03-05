#ifndef TP_LIST_H
#define TP_LIST_H

#include <soc/sprd/board.h>

struct gslX680_ts_platform_data{
        int irq_gpio_number;
        int reset_gpio_number;
  //int power_en_gpio_number;
  	const char *vdd_name;
	//int *flag;
	int virtualkeys[12];
	int TP_MAX_X;
	int TP_MAX_Y;
};

struct fw_data {
	u32 offset:8;
	 u32:0;
	u32 val;
};
#define GSL_NOID_VERSION
struct gsl_touch_info {
     int x[10];
     int y[10];
     int id[10];
     int finger_num;
};

struct gsl_tp_info {
	char name[32];
	struct fw_data *fw;
	int fw_len;
	unsigned int * config;
};

///  all tp from here @{
// WWX277-105-V0
//#include "P30_ST_WUXGA.h"
// MJK-GG101
//#include "E30_MJK_WUXGA.h"
//#include "Z716_WSVGA.h"
//#include "DFL_PX101D28B021.h"
//#include "CX031D.h"
//#include "QSF_PG8014.h"
// MJK-PG101-1716
//#include "MJK_PG101.h"
//#include "MJK_GG080.h"
//#include "HZYCTP_102690.h"
//#include "E960_HZYCTP102383.h"
//#include "E960_DC101208.h"
//#include "E960_GT10PG127V20_3676.h"
//#include "M863_GSL1680.h"
//#include "G863_XL.h"
//#include "GSL1680_JWN_PG07003.h"
//#include "GSL1680_JWN_PG07003_Ver.h"	//横屏竖显
//#include "E863_MS1365.h"
//#include "E863_MJK_1685_GSL1680_800x1280.h"
//#include "E863_MJK_1685_GSL1680_1200x1920.h"
//#include "G960_DC101208.h"
//#include "E863_GSL3670_PXA42A011_800x1280.h"
//#include "GSL1680_XC_0700235.h"
//#include "GSL3670_DFL_P30.h"
//#include "GSL1680_MJK_MJK1279.h"
//#include "GSL1680_MJK_MJK1540.h"
//#include "GSL3670_YJ1337PG101A2J1.h"
//#include "GSL3670_MJK_GG101_1534.h"
//#include "GSL3670_WXW_WWX504101V0.h"
#ifdef ZCFG_GSL860_T863_FHYN
#include "GSL1680_T863_FHYN.h"

#elif defined(ZCFG_TP_GSL3670_AUTO_UPGRADE)
#include ZCFG_TP_GSL3670_AUTO_UPGRADE
#else
#include "../gslx680.h"
#endif

static struct gsl_tp_info all_tps[] = {
	//{"MJK_GG080",  FW_MJK_GG080,  ARRAY_SIZE(FW_MJK_GG080), gsl_config_data_id_MJK_GG080 },
	//{"MJK_PG101",  FW_MJK_PG101,  ARRAY_SIZE(FW_MJK_PG101), gsl_config_data_id_MJK_PG101 },
	//{"P30_ST_WUXGA",  FW_P30_ST_WUXGA,  ARRAY_SIZE(FW_P30_ST_WUXGA), gsl_config_data_id_P30_ST_WUXGA },
	//{"E30_MJK_WUXGA",  FW_E30_MJK_WUXGA,  ARRAY_SIZE(FW_E30_MJK_WUXGA), gsl_config_data_id_E30_MJK_WUXGA },
	//{"Z716_WSVGA",  FW_Z716_WSVGA,  ARRAY_SIZE(FW_Z716_WSVGA), gsl_config_data_id_Z716_WSVGA },
	//{"DFL_PX101D28B021",  FW_DFL_PX101D28B021,  ARRAY_SIZE(FW_DFL_PX101D28B021), gsl_config_data_id_DFL_PX101D28B021 },
	//{"CX031D",  FW_CX031D,  ARRAY_SIZE(FW_CX031D), gsl_config_data_id_CX031D },
	//{"QSF_PG8014",  FW_QSF_PG8014,  ARRAY_SIZE(FW_QSF_PG8014), gsl_config_data_id_QSF_PG8014 },
	//{"HZYCTP_102690",  FW_HZYCTP102690,  ARRAY_SIZE(FW_HZYCTP102690), gsl_config_data_id_HZYCTP102690 },
	//{"E960_HZYCTP102383",  FW_E960_HZYCTP102383,  ARRAY_SIZE(FW_E960_HZYCTP102383), gsl_config_data_id_E960_HZYCTP102383 },
	//{"E960_DC101208",  FW_E960_DC101208,  ARRAY_SIZE(FW_E960_DC101208), gsl_config_data_id_E960_DC101208 },
	//{"E960_GT10PG127V20_3676",  FW_E960_GT10PG127V20_3676,  ARRAY_SIZE(FW_E960_GT10PG127V20_3676), gsl_config_data_id_E960_GT10PG127V20_3676 },
	//{"M863_GSL1680",  FW_M863_GSL1680,  ARRAY_SIZE(FW_M863_GSL1680), gsl_config_data_id_M863_GSL1680 },
	//{"G863_XL",  FW_G863_XL,  ARRAY_SIZE(FW_G863_XL), gsl_config_data_id_G863_XL },
	//{"GSL1680_JWN_PG07003",  FW_GSL1680_JWN_PG07003,  ARRAY_SIZE(FW_GSL1680_JWN_PG07003), gsl_config_data_id_GSL1680_JWN_PG07003 },
	//{"GSL1680_JWN_PG07003_Ver",  FW_GSL1680_JWN_PG07003_Ver,  ARRAY_SIZE(FW_GSL1680_JWN_PG07003_Ver), gsl_config_data_id_GSL1680_JWN_PG07003_Ver },
	//{"E863_MS1365",  FW_E863_MS1365,  ARRAY_SIZE(FW_E863_MS1365), gsl_config_data_id_E863_MS1365 },
	//{"E863_MJK_1685_GSL1680_800x1280",  FW_E863_MJK_1685_GSL1680_800x1280,  ARRAY_SIZE(FW_E863_MJK_1685_GSL1680_800x1280), gsl_config_data_id_E863_MJK_1685_GSL1680_800x1280 },
	//{"G960_DC101208",  FW_G960_DC101208,  ARRAY_SIZE(FW_G960_DC101208), gsl_config_data_id_G960_DC101208 },
	//{"E863_GSL3670_PXA42A011_800x1280",  FW_E863_GSL3670_PXA42A011_800x1280,  ARRAY_SIZE(FW_E863_GSL3670_PXA42A011_800x1280), gsl_config_data_id_E863_GSL3670_PXA42A011_800x1280 },
	//{"GSL1680_XC_0700235",  FW_GSL1680_XC_0700235,  ARRAY_SIZE(FW_GSL1680_XC_0700235), gsl_config_data_id_GSL1680_XC_0700235 },
	//{"E863_MJK_1685_GSL1680_1200x1920",  FW_E863_MJK_1685_GSL1680_1200x1920,  ARRAY_SIZE(FW_E863_MJK_1685_GSL1680_1200x1920), gsl_config_data_id_E863_MJK_1685_GSL1680_1200x1920 },
	//{"GSL3670_DFL_P30",  FW_GSL3670_DFL_P30,  ARRAY_SIZE(FW_GSL3670_DFL_P30), gsl_config_data_id_GSL3670_DFL_P30 },
	//{"GSL1680_MJK_MJK1279",  FW_GSL1680_MJK_MJK1279,  ARRAY_SIZE(FW_GSL1680_MJK_MJK1279), gsl_config_data_id_GSL1680_MJK_MJK1279 },
	//{"GSL1680_MJK_MJK1540",  FW_GSL1680_MJK_MJK1540,  ARRAY_SIZE(FW_GSL1680_MJK_MJK1540), gsl_config_data_id_GSL1680_MJK_MJK1540 },
	//{"GSL3670_YJ1337PG101A2J1",  FW_GSL3670_YJ1337PG101A2J1,  ARRAY_SIZE(FW_GSL3670_YJ1337PG101A2J1), gsl_config_data_id_GSL3670_YJ1337PG101A2J1 },
	//{"GSL3670_MJK_GG101_1534",  GSLX680_FW,  ARRAY_SIZE(GSLX680_FW), gsl_config_data_id_GSLX680_FW },
	//{"GSL3670_WXW_WWX504101V0",  FW_GSL3670_WXW_WWX504101V0,  ARRAY_SIZE(FW_GSL3670_WXW_WWX504101V0), gsl_config_data_id_GSL3670_WXW_WWX504101V0 },
	{"GSL3670_FM961L6_CD634",  GSLX680_FW,  ARRAY_SIZE(GSLX680_FW), gsl_config_data_id_GSLX680_FW },
	
};

//@}

#endif
