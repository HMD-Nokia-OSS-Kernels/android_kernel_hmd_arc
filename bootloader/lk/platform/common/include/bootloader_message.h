/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BOOTLOADER_MESSAGE_H
#define _BOOTLOADER_MESSAGE_H

#ifndef __LK__
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#else
/* compiler.h defines the types that otherwise are included from stdint.h and
 * stddef.h
 */
#include <compiler.h>
#endif


/* Bootloader Message (size 2KB)
 *
 * In this struct, used to have slot_suffix field for A/B boot control metadata.
 * which gets unintentionally cleared by recovery or uncrypt.
 * Move it into struct bootloader_message_ab to avoid the issue.
 */
struct bootloader_message {
    char command[32];
    char status[32];
    char recovery[768];
    char stage[32];
    char reserved[1184];
};

#include <boot_control_definition.h>

#endif  // _BOOTLOADER_MESSAGE_H
