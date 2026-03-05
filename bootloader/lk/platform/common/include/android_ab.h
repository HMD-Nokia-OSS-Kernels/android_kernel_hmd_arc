// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017 The Android Open Source Project
 */

#ifndef __ANDROID_AB_H
#define __ANDROID_AB_H

#include <sprd_common.h>
#include <secureboot/sec_common.h>

/* Android standard boot slot names are 'a', 'b', 'c', ... */
#define BOOT_SLOT_NAME(slot_num) ('a' + (slot_num))

/* Number of slots */
#define NUM_SLOTS 2

/**
 * Select the slot where to boot from.
 */
int ab_select_slot(void);
int ab_slot_rollback_spl(bool update_misc, uint8_t status);
int get_slot_ab(char *ab_part_name, const char *src);
int adjust_spl_slot(void);


#endif /* __ANDROID_AB_H */
