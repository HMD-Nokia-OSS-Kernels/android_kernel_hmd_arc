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

#ifndef _SEC_EFUSE_H_
#define _SEC_EFUSE_H_

#ifdef CONFIG_SC9833
#include "sec_efuse_sharkl2.h"
#endif
#ifdef PLATFORM_SHARKLJ1
#include "sec_efuse_sharklj1.h"
#endif
#ifdef PLATFORM_PIKE2
#include "sec_efuse_pike2.h"
#endif
#ifdef PLATFORM_SHARKLE
#include "sec_efuse_sharkle.h"
#endif
#ifdef PLATFORM_SHARKL3
#include "sec_efuse_sharkl3.h"
#endif
#ifdef PLATFORM_SHARKL5
#include "sec_efuse_sharkl5.h"
#endif
#ifdef PLATFORM_QOGIRL6
#include "sec_efuse_sharkl6.h"
#endif
#ifdef PLATFORM_ROC1
#include "sec_efuse_roc1.h"
#endif
#ifdef PLATFORM_ORCA
#include "sec_efuse_orca.h"
#endif
#ifdef PLATFORM_SHARKL5PRO
#include "sec_efuse_sharkl5pro.h"
#endif
#ifdef PLATFORM_QOGIRN6PRO
#include "sec_efuse_sharkl6pro.h"
#endif

#ifdef PLATFORM_QOGIRN6L
#include "sec_efuse_qogirn6l.h"
#endif
#endif
