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

#include <asm/arch/common.h>
#include <libavb.h>
#include <uboot_avb_pub.h>
#include <secureboot/sec_common.h>
#include <sprd_common_rw.h>
#include "boot_parse.h"
#ifdef CONFIG_ANDROID_AB
#include <linux/kernel.h>
extern char g_env_slot[3];
#endif

AvbOps g_avb_ops;
AvbSlotVerifyData* avb_slot_data[2] = {NULL, NULL};

int avb_check_vbmeta(uint8_t * vbmeta_img_paddr, uint32_t vbmeta_image_len)
{
    AvbSlotVerifyResult verify_result;
    uint8_t * vbmeta_pubkey_addr = 0;
    uint32_t vbmeta_pubkey_len = 0;
    AvbVBMetaVerifyResult vbmeta_ret;
    const uint8_t* pk_data = 0;
    size_t pk_len = 0;
    bool key_istrusted = false;
    uint32_t vboot_verify_ret = 0;
    uint64_t rollback_index = 0;
    uint64_t stored_rollback_index = 0;
    AvbIOResult io_ret;

    if (!vbmeta_img_paddr ||!vbmeta_image_len) {
        debugf("invalid: para is null.\n");
        return AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    }
    vbmeta_ret = avb_vbmeta_image_verify(vbmeta_img_paddr, vbmeta_image_len,
                                         &pk_data, &pk_len, &rollback_index);
    debugf("vbmeta_ret is %d.\n", vbmeta_ret);
    debugf("rollback_index is %lld.\n", rollback_index);
    if (vbmeta_ret == 0) {
#ifdef CONFIG_VBOOT_DUMP
        debugf("dump pk_data:, pk_len:%d\n", pk_len);
        do_hex_dump(pk_data, pk_len);
#endif
        vboot_verify_ret = g_avb_ops.validate_vbmeta_public_key(&g_avb_ops, pk_data, pk_len, vbmeta_pubkey_addr, vbmeta_pubkey_len, &key_istrusted);
        debugf("vboot_verify_ret is %d, key_istrusted:%d.\n", vboot_verify_ret, key_istrusted);
        verify_result = vboot_verify_ret;
        if (verify_result!=AVB_IO_RESULT_OK)
            goto out;
        /* Check rollback index. */
        debugf("Check rollback index for vbmeta \n");
        io_ret = g_avb_ops.read_rollback_index(&g_avb_ops, 0, &stored_rollback_index);
        if (io_ret != AVB_IO_RESULT_OK) {
            verify_result = AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX;
            goto out;
        }
        if (rollback_index < stored_rollback_index) {
            debugf("Error: vbmeta rollback_index:%lld, < stored_rollback_index:%lld.\n",
                  rollback_index, stored_rollback_index);
            verify_result = AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX;
            goto out;
        } else {
            debugf("Info: vbmeta rollback_index:%lld, >= stored_rollback_index:%lld.\n",
                  rollback_index, stored_rollback_index);
            verify_result = AVB_SLOT_VERIFY_RESULT_OK;
        }
    } else {
        verify_result = vbmeta_ret;
    }
out:
    return verify_result;
}


void check_dmverity_state(void) {
	debug("resolved_hashtree_error_mode = %d.\n",avb_slot_data[0]->resolved_hashtree_error_mode);
	if (avb_slot_data[0]) {
		if (avb_slot_data[0]->resolved_hashtree_error_mode
				 == AVB_HASHTREE_ERROR_MODE_EIO) {
			debug("dmverity_state is eio.\n");
			g_dmverity_flag = HWRST_STATUS_SECBOOT;
		}
	}
}

/*check dm-verity flags when booting*/
static DmVerityIOResult save_and_check_dmverity_flags(void)
{
	uint32_t flag_len = sizeof(uint32_t);//dmverity_info.error_flag size
	DmVerityIOResult ret = DMVERITY_IO_RESULT_OK;

	if (g_dmverity_flag == HWRST_STATUS_SECBOOT) {
		if (common_raw_write(PRODUCTINFO_FILE_PATITION, flag_len, 0,
				PDT_INFO_DMVERITY_FLAG_OFFSET, (char *)&g_dmverity_flag)) {
			debug("write miscdata error.\n");
			ret = DMVERITY_IO_RESULT_ERROR_IO;
			goto out;
		}
		debugf("hashtree corruption: g_dmverity_flag 0x%x.\n", g_dmverity_flag);
		ret = DMVERITY_IO_RESULT_ERROR_HASHTREE_CORRUPTION;
		goto out;
	}

out:
	return ret;
}

static void avb_get_verify_flags(AvbSlotVerifyFlags *flags)
{
	DmVerityIOResult ret = DMVERITY_IO_RESULT_OK;

	ret = save_and_check_dmverity_flags();
	switch(ret) {
		case DMVERITY_IO_RESULT_OK: {
			if (VBOOT_STATUS_UNLOCK == get_lock_status()) {
				debug("avb allow verification error in UNLOCK status while normal booting.\n");
				*flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;
			}
		} break;

		case DMVERITY_IO_RESULT_ERROR_HASHTREE_CORRUPTION: {
		debug("avb dm-verity hashtree corruption.\n");
		*flags |= AVB_SLOT_VERIFY_FLAGS_RESTART_CAUSED_BY_HASHTREE_CORRUPTION;
		} break;

		case DMVERITY_IO_RESULT_ERROR_IO: {
			errorf("io error, enter while(1), ret = %d.\n", ret);
			while(1);
		} break;

		default:
			debug("Warning:unknown ret: %d.\n", ret);
	}
}

int avb_check_image(unsigned char *name)
{
    AvbSlotVerifyResult verify_result;
    const char         *slot_suffixes[2] = {"", ""};
    const char         *requested_partitions[] = {"boot", NULL};
    AvbSlotVerifyFlags flags = AVB_SLOT_VERIFY_FLAGS_NONE;
    AvbSlotVerifyData  *out_slot_data = NULL;

    if (name) {
        debugf("avb check image name:%s.\n", name);
        requested_partitions[0] = name;
    } else {
        debugf("avb check image name is null.\n");
        requested_partitions[0] = "";
    }

#ifndef CONFIG_VBOOT_SYSTEMASROOT
	if (g_ulAvbCheckType == AVB_CHECK_BOOTING)
		avb_get_verify_flags(&flags);
#endif

#ifdef CONFIG_ANDROID_AB
	const char *sufixx = NULL;
	char *download_sufixx = "_a";
	char write_temp[4] = {0};
	if (g_ulAvbCheckType == AVB_CHECK_DOWNLOAD){
		/* Read Vbmeta_a is used to judge the partition "vbmeta" suffix is _a,
		compatible with Q using R FDL2 download. Gerrit ID: 701132 */
		if (0 != common_raw_read("vbmeta_a", (uint64_t) ARRAY_SIZE(write_temp), 0, write_temp))
			sufixx = slot_suffixes[0];
		else
			sufixx = download_sufixx;
	} else{
		sufixx = g_env_slot;
	}
	debugf("avb check image sufixx is %s.\n", sufixx);
#endif
    //load data and check
      verify_result = avb_slot_verify(&g_avb_ops,
                                      requested_partitions,
#ifdef CONFIG_ANDROID_AB
                                      sufixx,
#else
                                      slot_suffixes[0],
#endif
                                      flags,
                                      AVB_HASHTREE_ERROR_MODE_MANAGED_RESTART_AND_EIO,
                                      &avb_slot_data[0]);
	debugf(">>>>>>>>>>>>>>>>  avb_slot_verify result is %d.\n", verify_result);
	debugf(">>>>>>>>>>>>>>>>  avb_slot_verify result is %s.\n", avb_slot_verify_result_to_string(verify_result));
	debugf("slot_data[0] is 0x%x.\n", avb_slot_data[0]);
	debugf("slot_data[1] is 0x%x.\n", avb_slot_data[1]);

	out_slot_data = avb_slot_data[0];
	return verify_result;
}
