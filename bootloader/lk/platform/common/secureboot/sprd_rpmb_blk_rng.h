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

 #ifndef _SPRD_RPMB_BLK_RNG_H
 #define _SPRD_RPMB_BLK_RNG_H

#define MMC_BLOCK_SIZE 512
#define RPMB_DATA_SIZE 256

#define MAX_RPMB_BLOCK_CNT    2
//secure storage use 0 - 1021,see APP_STORAGE_RPMB_BLOCK_COUNT in .mk
#define RPMB_BLOCK_IND_START  (1022 * 2)
#define RPMB_BLOCK_IND_END    (1023 * 2)

#define SPRD_IMGVERSION_BLK   (1023 * 2)
#define SPRD_MODEM_IMGVERSION_BLK   (1022 * 2)


#endif // _SPRD_RPMB_BLK_RNG_H
