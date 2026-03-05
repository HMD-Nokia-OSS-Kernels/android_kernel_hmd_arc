/*
 * * Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * * you may not use this file except in compliance with the License.
 * * You may obtain a copy of the License at
 * * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * * Software distributed under the License is distributed on an "AS IS" BASIS,
 * * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * * See the Unisoc General Software License, version 1.0 for more details.
 * */

#ifndef _UFS_CFG_H_
#define _UFS_CFG_H_
#include <sprd_ufs.h>

void init_global_reg(void);
static int ufshci_enable_bottom(void);
extern struct ufs_hba_variant_ops hba_vops;

#endif
