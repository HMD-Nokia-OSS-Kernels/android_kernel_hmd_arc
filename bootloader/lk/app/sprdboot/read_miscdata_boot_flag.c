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

#include <boot_mode.h>
#include <part_efi.h>
#include "boot_parse.h"
#include <sprd_common_rw.h>
#include <miscdata_def.h>

u32 get_first_mode = 0;
u32 first_cali_mode = 0;
extern void download_after_enter_cali_mode(u8 cail_mode);
extern void download_after_enter_autotest_mode(u8 cali_mode);

static first_boot_mode_t first_boot_mode[CMD_SET_FIRST_MAX_MODE] = {
	{CMD_SET_FIRST_NORMAL_BOOT_MODE, CMD_NORMAL_MODE, 0x0},

	{CMD_SET_FIRST_GSM_CLA_MODE, CMD_CALIBRATION_MODE, 0x1},
	{CMD_SET_FIRST_GSM_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x5},

	{CMD_SET_FIRST_WCDMA_CLA_MODE, CMD_CALIBRATION_MODE, 0xB},
	{CMD_SET_FIRST_WCDMA_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0xC},

	{CMD_SET_FIRST_TDSCDMA_CLA_MODE, CMD_CALIBRATION_MODE, 0x7},
	{CMD_SET_FIRST_TDSCDMA_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x8},

	{CMD_SET_FIRST_LTETDD_CLA_MODE, CMD_CALIBRATION_MODE, 0x10},
	{CMD_SET_FIRST_LTETDD_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x11},

	{CMD_SET_FIRST_LTEFDD_CLA_MODE, CMD_CALIBRATION_MODE, 0x10},
	{CMD_SET_FIRST_LTEFDD_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x11},

	{CMD_SET_FIRST_NR5GSUB6G_CLA_MODE, CMD_CALIBRATION_MODE, 0x18},
	{CMD_SET_FIRST_NR5GSUB6G_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x19},

	{CMD_SET_FIRST_NRMMW_CLA_MODE, CMD_CALIBRATION_MODE, 0x18},
	{CMD_SET_FIRST_NRMMW_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x19},

	{CMD_SET_FIRST_CDMA2K_CLA_MODE, CMD_CALIBRATION_MODE, 0x12},
	{CMD_SET_FIRST_CDMA2K_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x13},

	{CMD_SET_FIRST_BBAT_MODE, CMD_AUTOTEST_MODE, 0},

	{CMD_SET_FIRST_NATIVE_MMI_MODE, CMD_NORMAL_MODE, 0},/* dont support factorytest mode anymore */

	{CMD_SET_FIRST_APK_MMI_MODE, CMD_APKMMI_MODE, 0},

	{CMD_SET_FIRST_NBIOT_CAL_MODE, CMD_CALIBRATION_MODE, 0x20},
	{CMD_SET_FIRST_NBIOT_FINAL_TEST_MODE, CMD_CALIBRATION_MODE, 0x21},

	{CMD_SET_FIRST_UPT_MODE, CMD_UPT_MODE, 0x0},

	{CMD_SET_FIRST_AUTOPON_MODE, CMD_NORMAL_MODE, 0x0},
	{CMD_SET_FIRST_FASTBOOT_MODE, CMD_FASTBOOT_MODE, 0x0},
	{CMD_SET_FIRST_APK_MMI_AUTO_MODE, CMD_APKMMI_AUTO_MODE, 0x0},
	{CMD_SET_FIRST_AUTODLOADER_REBOOT_MODE, CMD_AUTODLOADER_REBOOT, 0x0},
};

static int get_miscdata_boot_flag(char *out)
{
	if (0 != common_raw_read("miscdata", SET_FIRST_MDOE_LEN,
			(uint64_t)(SET_FIRST_MODE_OFFSET), out)) {
		dprintf(INFO,"partition <miscdata> read error\n");
		return -1;
	}

	return 0;
}

static int set_miscdata_boot_flag(char *in)
{
	if (0 != common_raw_write("miscdata", SET_FIRST_MDOE_LEN, (uint64_t)0,
			(uint64_t)(SET_FIRST_MODE_OFFSET), in)) {
		dprintf(INFO,"write partition <miscdata> fail\n");
		return -1;
	}

	return 0;
}

int read_boot_flag(void)
{
	u32 first_mode = 0;
	u32 clear_first_mode = 0;
	int err;
	u32 first_mode_aon_flag = 0;

	err = get_miscdata_boot_flag((char *)(&get_first_mode));
	if (err < 0) {
		dprintf(INFO,"Reading first mode flag from miscdata partition failed!\n");
		return CMD_UNDEFINED_MODE;
	}

	if ((get_first_mode & 0xFFFFFF00) != SET_FIRST_MODE_MAGIC) {
		dprintf(INFO,"The data obtained from miscdata is not a first_mode magic number!\n");
		return CMD_UNDEFINED_MODE;
	}

	/*BIT(7) is an first_mode alway_on flag*/
	first_mode_aon_flag = get_first_mode & 0x00000080;
	first_mode = get_first_mode & 0x0000007F;
	first_cali_mode = first_boot_mode[first_mode].cail_parameter;
	dprintf(INFO,"The mode obtained from miscdata: 0x%x, alway_on flag is %s.\n", first_mode,
					first_mode_aon_flag?"open":"close");

	/*Bug ID:1776169
	*  Prevent customers' mobile phone form setting first mode and unable to enter other mode.
	* Usage:
	*  CONFIG_CUSTOMER_PHONE:
	*     used to judge whether it is our platform machine
	*     if define CONFIG_CUSTOMER_PHONE, always erase first_mode
	*/
#if (!defined(CONFIG_CUSTOMER_PHONE)) || (defined(CONFIG_CUSTOMER_PHONE) && defined(CONFIG_AUTOBOOT))
	if (!first_mode_aon_flag)
#endif
	{
		err = set_miscdata_boot_flag((char *)(&clear_first_mode));
		if (err < 0) {
			dprintf(INFO,"Clearing the miscdata set first mode flag failed!\n");
			return CMD_UNDEFINED_MODE;
		}
	}

	if ((first_mode > CMD_SET_FIRST_NORMAL_BOOT_MODE) &&
			(first_mode < CMD_SET_FIRST_MAX_MODE)) {
		if (first_boot_mode[first_mode].boot_mode == CMD_CALIBRATION_MODE)
			download_after_enter_cali_mode(first_boot_mode[first_mode].cail_parameter);
		else if (first_boot_mode[first_mode].boot_mode == CMD_AUTOTEST_MODE)
			download_after_enter_autotest_mode(first_boot_mode[first_mode].cail_parameter);

		return first_boot_mode[first_mode].boot_mode;
	}

	return CMD_UNDEFINED_MODE;
}
