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

#include <lk_keymint_device.h>

#ifdef SPRD_VBOOT_V2
#include <uboot_avb_ops.h>
#include <boot_parse.h>
#include <libavb.h>
#endif

#define LOCAL_LOG 0

#define VERIFIED_BOOT_HASH_STR_LENGTH 64
#define CHAR_TO_HEX(ch)                                                          \
    (((((ch) >= 'A') && ((ch) <= 'F')) || (((ch) >= 'a') && ((ch) <= 'f'))) ?  \
     (((ch) <= 'F') ? ((ch) - 'A' + 10) : ((ch) - 'a' + 10)) :                   \
     ((ch) - '0'))

static int trusty_km_version = 2;

static void forward_command(struct km_ipc_task* ipc_task,
                            void* req_buffer, void* rsp_buffer) {
    keymaster_error_t err;
    int rc;

    rc = km_ipc_connect();
    if (rc != 0) {
        trusty_error("Initlializing Trusty Keymint client failed (%d)\n", rc);
        return;
    }

    err = lk_keymint_send(ipc_task, req_buffer, rsp_buffer);
    if (err != KM_ERROR_OK) {
        trusty_error("Cmd: %d returned error: %d\n", ipc_task->cmd, ipc_task->error);
    }

    km_ipc_disconnect();
}

static int32_t string_to_hex(const uint8_t *str, uint32_t str_len, uint8_t *out)
{
    uint8_t *p = (uint8_t*)str;
    uint8_t ch = 0;
    uint8_t high = 0;
    uint8_t low = 0;
    uint32_t len = 0;
    uint32_t offset = 0;

    if (!str || !out)
        return 0;

    len = str_len;
    while(offset < (len / 2))
    {
        ch = *p;
        high = CHAR_TO_HEX(ch);
        ch = *(++p);
        low = CHAR_TO_HEX(ch);
        out[offset] = ((high & 0x0f) << 4 | (low & 0x0f));
        p++;
        offset++;
    }
    if(len % 2 != 0) {
        ch = *p;
        out[offset] = CHAR_TO_HEX(ch);
    }

    return len / 2 + len % 2;
}

static int32_t message_version(uint8_t major_ver,
                              uint8_t minor_ver,
                              uint8_t subminor_ver) {
    int32_t message_version = -1;
    switch (major_ver) {
    case 0:
        message_version = 0;
        break;
    case 1:
        switch (minor_ver) {
        case 0:
            message_version = 1;
            break;
        case 1:
            message_version = 2;
            break;
        }
        break;
    case 2:
        message_version = 3;
        break;
    }
    return message_version;
}

static void km_get_version(int32_t* version) {
    void* req = NULL;
    struct km_get_version_rsp rsp;

    struct km_ipc_task ipc_task = {
        .cmd = KM_GET_VERSION,
        .serialized_size = km_empty_serialized_size,
        .serialize = km_empty_serialize,
        .deserialize = km_get_version_deserialize,
        .error = KM_ERROR_UNKNOWN_ERROR,
    };

    forward_command(&ipc_task, req, &rsp);

    if(ipc_task.error != KM_ERROR_OK) {
        trusty_error("Failed to get TA version.");
        return;
    }

    *version = message_version(rsp.major_ver, rsp.minor_ver, rsp.subminor_ver);
    return;
}

static long km_get_boot_params(km_set_boot_params_req *boot_params, uint32_t verify_result) {
    uint8_t* KM_verified_boot_hash_val_addr = NULL;
    uint8_t verified_boot_hash_str[VERIFIED_BOOT_HASH_STR_LENGTH] = {0};

    if (boot_params == NULL) {
        trusty_error("keymint boot_params is null.");
        return KM_ERROR_UNEXPECTED_NULL_POINTER;
    }

    trusty_memset(boot_params, 0, sizeof(km_set_boot_params_req));

    boot_params->os_patchlevel = 0;
    boot_params->os_version = 0;

#ifdef SPRD_VBOOT_V2
    boot_params->device_locked = KM_DEVICE_LOCKED;
    if (g_DeviceStatus == VBOOT_STATUS_UNLOCK) {
        boot_params->device_locked = KM_DEVICE_UNLOCKED;
        boot_params->verified_boot_state = KM_VERIFIED_BOOT_UNVERIFIED;
    } else if (verify_result == 0) {
        boot_params->verified_boot_state = KM_VERIFIED_BOOT_VERIFIED;
    } else if (verify_result == AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED) {
        boot_params->verified_boot_state = KM_VERIFIED_BOOT_SELF_SIGNED;
    } else {
        boot_params->verified_boot_state = KM_VERIFIED_BOOT_FAILED;
    }

    boot_params->verified_boot_key_len = g_sprd_vboot_key_len;
    if (boot_params->device_locked == KM_DEVICE_LOCKED) {
        trusty_memcpy(boot_params->verified_boot_key, g_sprd_vboot_key, g_sprd_vboot_key_len);
    }


    KM_verified_boot_hash_val_addr = (uint8_t *)strstr(
            (const char*)g_sprd_vboot_cmdline,
            "androidboot.vbmeta.digest");

    if (KM_verified_boot_hash_val_addr == NULL) {
        trusty_error("fail to find androidboot.vbmeta.digest.");
        return KM_ERROR_UNKNOWN_ERROR;
    }

    while(*(KM_verified_boot_hash_val_addr++) != '=');

    trusty_memcpy(verified_boot_hash_str,
        KM_verified_boot_hash_val_addr, VERIFIED_BOOT_HASH_STR_LENGTH);

    boot_params->verified_boot_hash_len = string_to_hex((const uint8_t*)verified_boot_hash_str,
                                                        VERIFIED_BOOT_HASH_STR_LENGTH,
                                                        boot_params->verified_boot_hash);

    if(boot_params->verified_boot_hash_len == 0) {
        trusty_error("fail to string_to_hex of verified_boot_hash.");
        return KM_ERROR_UNKNOWN_ERROR;
    }
#endif

    return KM_ERROR_OK;
}

void km_initialize(void) {
    km_ipc_init();
    int32_t version = -1;
    km_get_version(&version);
    trusty_info("Get keymint version %d\n", version);
    if (version < trusty_km_version) {
        trusty_error("keymint version mismatch. Expected %d, received %d\n",
                     trusty_km_version, version);
        return;
    }

    km_config_boot_patchlevel();
    km_set_boot_params();

    // km_ipc_shutdown()

    return;
}

void km_config_boot_patchlevel(void) {
    struct km_boot_patchlevel_req req;
    void* rsp = NULL;

    uint32_t boot_patchlevel = 0;
#ifdef SPRD_VBOOT_V2
    get_boot_patch_level(&boot_patchlevel);
#endif

    req.boot_patchlevel = boot_patchlevel;

    struct km_ipc_task ipc_task = {
        .cmd = KM_CONFIGURE_BOOT_PATCHLEVEL,
        .serialized_size = km_boot_patchlevel_serialized_size,
        .serialize = km_boot_patchlevel_serialize,
        .deserialize = km_empty_deserialize,
        .error = KM_ERROR_UNKNOWN_ERROR,
    };

    forward_command(&ipc_task, &req, rsp);

    return;
}

void km_set_boot_params(void)
{
    struct km_set_boot_params_req req;
    uint32_t verify_result = vboot_verify_info->verify_return_data->vboot_verify_ret;

    if (km_get_boot_params(&req, verify_result) != KM_ERROR_OK) {
        trusty_error("Failed to get boot_params!");
        return;
    }
    void* rsp = NULL;

    struct km_ipc_task ipc_task = {
        .cmd = KM_SET_BOOT_PARAMS,
        .serialized_size = km_set_boot_params_serialized_size,
        .serialize = km_set_boot_params_serialize,
        .deserialize = km_empty_deserialize,
        .error = KM_ERROR_UNKNOWN_ERROR,
    };

    forward_command(&ipc_task, &req, rsp);

    return;
}