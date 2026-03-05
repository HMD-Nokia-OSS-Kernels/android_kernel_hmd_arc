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

#ifndef _SPRD_REG_H_
#define _SPRD_REG_H_

#ifndef BIT
#define BIT(x) (1<<(x))
#endif

#include <config.h>
#include "hardware.h"
#include "sprd_module_config.h"
#include <power/sprd_pmic/pmic_glb_reg.h>

#include "./chip_qogirn6pro/ai_apb.h"
#include "./chip_qogirn6pro/ai_clk.h"
#include "./chip_qogirn6pro/ai_dvfs_apb.h"
#include "./chip_qogirn6pro/anlg_phy_g0l.h"
#include "./chip_qogirn6pro/anlg_phy_g1.h"
#include "./chip_qogirn6pro/anlg_phy_pcie3.h"
#include "./chip_qogirn6pro/aon_apb.h"
#include "./chip_qogirn6pro/aon_clk.h"
#include "./chip_qogirn6pro/aon_sec_apb.h"
#include "./chip_qogirn6pro/aon_sec_dbg_apb.h"
#include "./chip_qogirn6pro/aon_sys.h"
#include "./chip_qogirn6pro/apcpu_dvfs_apb.h"
#include "./chip_qogirn6pro/ap_ahb.h"
#include "./chip_qogirn6pro/ap_apb.h"
#include "./chip_qogirn6pro/ap_clk.h"
#include "./chip_qogirn6pro/dpu_vsp_apb.h"
#include "./chip_qogirn6pro/dpu_vsp_clk.h"
#include "./chip_qogirn6pro/gpu_apb.h"
#include "./chip_qogirn6pro/gpu_dvfs_apb.h"
#include "./chip_qogirn6pro/hardware.h"
#include "./chip_qogirn6pro/ipa_apb.h"
#include "./chip_qogirn6pro/ipa_clk.h"
#include "./chip_qogirn6pro/mem_fw_pub.h"
#include "./chip_qogirn6pro/pmu_apb.h"
#include "./chip_qogirn6pro/pub_ahb.h"
#include "./chip_qogirn6pro/pub_apb.h"
#include "./chip_qogirn6pro/pub_qosc_ahb.h"
#include "./chip_qogirn6pro/reg_fw_ap_ahb.h"
#include "./chip_qogirn6pro/camerasys_glb.h"
#include "./chip_qogirn6pro/top_dvfs_apb.h"
#include "./chip_qogirn6pro/redefine.h"

#endif
