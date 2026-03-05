/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef INTERFACE_KEYMINT_H_
#define INTERFACE_KEYMINT_H_

/* This file contains all the interfaces provided to external calls. */

// KM client should start after AVB parses vbmeta.
void km_initialize(void);


#endif /* INTERFACE_KEYMINT_H_ */
