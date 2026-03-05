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

#include <malloc.h>
#include "sparse_format.h"
#include <dl_common.h>
#include "nvmerge.h"
#include "dl_operate.h"
#include "sprd_common_rw.h"

#ifdef  NVMERGE_TRACE
#undef  NVMERGE_TRACE
#endif // NVMERGE_TRACE

#define NV_HEAD_LEN 512
#define NVMERGE_TRACE debugf

typedef struct _NV_HEADER {
    uint32_t magic;
    uint32_t len;
    uint32_t checksum;
    uint32_t version;
} nv_header_t;
extern int nv_aes_gcm(unsigned char *input, int input_bytelen, unsigned char *output, int *output_bytelen);
/*
    id:
    nvBuf:        nv data
    nvLength:    nv size
    itemPos:    item pos
    itemSize:    item size
*/
BOOLEAN ___findItem( /*IN*/ uint32_t id, /*IN*/ uint8_t * nvBuf, /*IN*/ uint32_t nvLength, /*OUT*/ uint32_t * itemSize, /*OUT*/ uint32_t * itemPos)
{
    uint32_t offset = 4;
    uint16_t tmp[2];
    while (1) {
        if (offset + sizeof(tmp) > nvLength) {
            NVMERGE_TRACE("NVMERGE: ___findItem Surpass the boundary of the part\r\n");
            break;
        }
        if (*(uint16_t *) (nvBuf + offset) == INVALID_ID) {
            NVMERGE_TRACE("NVMERGE:___findItem find the tail\n");
            break;
        }
        memcpy(tmp, nvBuf + offset, sizeof(tmp));
        offset += sizeof(tmp);
        if (id == (uint32_t) tmp[0]) {
            *itemSize = (uint32_t) tmp[1];
            *itemPos = offset;
            NVMERGE_TRACE("NVMERGE:___findItem id = 0x%x\n", id);
            return TRUE;
        }
        offset += tmp[1];
        offset = (offset + 3) & 0xFFFFFFFC;
    }
    return FALSE;
}

BOOLEAN mergeItem(uint8_t * oldBuf, uint32_t oldNVlength, uint8_t * newBuf, uint32_t newNVlength)
{
    uint32_t i;
    uint32_t oldSize, oldPos;
    uint32_t newSize, newPos;
    for (i = 0; strcmp(nv_cfg[i].name,"\0"); i++) {
        if (!___findItem(nv_cfg[i].id, oldBuf, oldNVlength, &oldSize, &oldPos)) {
            continue;
        }
        if (!___findItem(nv_cfg[i].id, newBuf, newNVlength, &newSize, &newPos)) {
            continue;
        }
        NVMERGE_TRACE("NVMERGE:__mergeItem oldSize 0x%x newSize 0x%x\n", oldSize, newSize);
        if (oldSize == newSize) {
            memcpy(newBuf + newPos, oldBuf + oldPos, newSize);
            NVMERGE_TRACE("NVMERGE:__mergeItem success id = 0x%x\n", nv_cfg[i].id);
        } else {
            return FALSE;
        }
    }
    return TRUE;
}

char* getFixnvBuf(char *ori_path, char *bak_path, uint32_t * nv_len_ptr)
{
    uint32_t ori_len = 0;
    int status = 0;
    uint16_t ori_ecc = 0;
    nv_header_t *ori_header_ptr = NULL;
    uint8_t *buf = NULL;

    ori_header_ptr = (nv_header_t *) malloc(sizeof(nv_header_t));
    if (!ori_header_ptr) {
        NVMERGE_TRACE("%s ori_header_ptr malloc failed\n", __FUNCTION__);
        goto END;
    }

TRY_ORIGINAL:
    memset(ori_header_ptr, 0x0, sizeof(nv_header_t));
    if (0 != common_raw_read(ori_path, sizeof(nv_header_t), (uint64_t)0, ori_header_ptr)) {
        NVMERGE_TRACE("fail to read nv header!\n");
        goto TRY_BACKUP;
    }

    ori_ecc = ori_header_ptr->checksum;
    ori_len = ori_header_ptr->len;
    buf = malloc(ori_len);
    if (!buf) {
        NVMERGE_TRACE("%s ori_len malloc failed\n", __FUNCTION__);
        goto END;
    }
    memset(buf, 0xFF, ori_len);
    if (0 != common_raw_read(ori_path, (uint64_t)(ori_len), NV_HEAD_LEN, buf)) {
        NVMERGE_TRACE("fail to read ori data!\n");
        goto TRY_BACKUP;
    }
    NVMERGE_TRACE("%s ori buf ecc\n", __FUNCTION__);
    if (TRUE == fdl_check_crc((uint8_t *)buf, ori_len, ori_ecc)){
        *nv_len_ptr = ori_len;
        goto END;
    }

TRY_BACKUP:
    memset(ori_header_ptr, 0x0, sizeof(nv_header_t));
    if (0 != common_raw_read(bak_path, sizeof(nv_header_t), (uint64_t)0, ori_header_ptr)) {
        NVMERGE_TRACE("fail to read nv header!\n");
        goto ERROR;
    }

    ori_ecc = ori_header_ptr->checksum;
    ori_len = ori_header_ptr->len;
    if (!buf) {
        buf = malloc(ori_len);
    }
    if (!buf) {
        NVMERGE_TRACE("%s ori_len malloc failed\n", __FUNCTION__);
        goto END;
    }
    memset(buf, 0xFF, ori_len);
    NVMERGE_TRACE("%s bakbuf ecc\n", __FUNCTION__);
    if (0 != common_raw_read(bak_path, (uint64_t)(ori_len), NV_HEAD_LEN, buf)) {
        NVMERGE_TRACE("fail to read backup data!\n");
        goto ERROR;
    }
    if (TRUE == fdl_check_crc((uint8_t *)buf, ori_len, ori_ecc)) {
        *nv_len_ptr = ori_len;
        goto END;
    }

ERROR:
    if (buf) free(buf);
    buf = NULL;
END:
    if (ori_header_ptr) {
        free(ori_header_ptr);
    }
    return buf;
}

void ConvertBcdToDigitalStr(
                uint8_t         length,
                uint8_t         *bcd_ptr,      // in: the bcd code string
                uint8_t         *digital_ptr   // out: the digital string
                )
{
    int                i = 0;
    uint8_t         temp = 0;

    // get the first digital
    temp = ((*bcd_ptr >> 4) &0x0f);
    if (temp >= 0x0a) {
        *digital_ptr = (temp - 0x0a) + 'A';
    } else {
        *digital_ptr = temp + '0';
    }

    bcd_ptr++;
    digital_ptr++;


    for (i=0; i<(length - 1); i++) {
        temp = *bcd_ptr;
        // get the low 4 bits
        temp &= 0x0f;
        // A -- F
        if (temp >= 0x0a) {
            *digital_ptr = (temp - 0x0a) + 'A';
        } else {
            // 1 -- 9
            *digital_ptr = temp + '0';
        }
        digital_ptr++;

        temp = *bcd_ptr;
        // get the high 4 bits
        temp = (temp & 0xf0) >> 4;

        if ((temp == 0x0f) && (i == (length -1))) {
            *digital_ptr = '\0';
            return;
        } else if (temp>=0x0a) {
            *digital_ptr = (temp - 0x0a) + 'A';
        } else {
            // 1 -- 9
            *digital_ptr = temp + '0';
        }
        digital_ptr++;
        bcd_ptr++;
    }
    *digital_ptr = '\0';
}

#define MN_MAX_IMEI_LENGTH 16
extern SPECIAL_PARTITION_CFG const s_special_partition_cfg[];
extern int get_slot_ab(char *ab_part_name, const char *src);
extern int get_special_partition_size(void);
extern char g_env_slot[3];
int get_imei_from_nv(char* imei1, char* imei2) {
    int ret = 1;
    int i = 0;
    int num = 0;
    uint32_t imei_size = 0;
    uint32_t imei_position = 0;
    uint32_t size = 0;
    uint16_t imei[2] = {0x5, 0x179};
    char ori_partition_name[16] = {0};
    char bak_partition_name[16] = {0};
    uint8_t imei_data[2][8];
    uint8_t  tempbuf[32] = {0};
    uint8_t* buf = NULL;

    NVMERGE_TRACE("get_imei_from_nv enter\n");
    num = get_special_partition_size();
//check nv partition
    for (i = 0; i < num; i++) {
        if ((NULL == s_special_partition_cfg[i].partition) ||
            (NULL == s_special_partition_cfg[i].bak_partition)||
            (s_special_partition_cfg[i].purpose != PARTITION_PURPOSE_NV)) {
            break;
        }
        if (strstr(s_special_partition_cfg[i].partition, "fixnv1") == NULL &&
            strstr(s_special_partition_cfg[i].bak_partition, "fixnv2") == NULL) {
            continue;
        }
        if (((strlen(s_special_partition_cfg[i].partition) + strlen(g_env_slot) + 1) >= sizeof(ori_partition_name)) ||
           ((strlen(s_special_partition_cfg[i].bak_partition) + strlen(g_env_slot) + 1) >= sizeof(bak_partition_name))) {
            continue;
        }
        if (get_slot_ab(ori_partition_name, s_special_partition_cfg[i].partition) ||
           get_slot_ab(bak_partition_name, s_special_partition_cfg[i].bak_partition)) {
            continue;
        }
        NVMERGE_TRACE("get nv buf [%d:%s]\n", i, ori_partition_name);
        buf = getFixnvBuf(ori_partition_name, bak_partition_name, &size);
        if (buf != NULL) {
            break;
        }
    }
    if (!buf) {
        NVMERGE_TRACE("get nv buf error [%d:%s]\n", i, s_special_partition_cfg[i].partition);
        ret |= 1;
        goto END;
    }
//finditem
    for (int i =0; i < sizeof(imei)/sizeof(uint16_t); i++) {
        if (___findItem(imei[i], buf, size, &imei_size, &imei_position)) {
#ifndef NV_ENCRYPTION
            memcpy(imei_data[i], (uint8_t*)buf+imei_position, 8);
#else
            nv_aes_gcm(buf+imei_position, imei_size, imei_data[i], &imei_size);
#endif
            //NVMERGE_TRACE("imei_data[%d]: 0x%x_0x%x\n", i, *(uint32_t*)((uint8_t*)buf+imei_position), (uint32_t*)((uint8_t*)buf+imei_position+4));
            ConvertBcdToDigitalStr(MN_MAX_IMEI_LENGTH, imei_data[i], tempbuf);
            if (i == 0) {
                snprintf(imei1, 16, "%s",tempbuf);
            } else {
                snprintf(imei2, 16, "%s",tempbuf);
            }
            ret = 0;
            NVMERGE_TRACE("get imei [%d:%s][%d:%s]\n", 1, imei1, 2, imei2);
        } else {
            NVMERGE_TRACE("get imei error [%d]\n", imei[i]);
            ret |= ((i + 1)<<2);
        }
    }
END:
    if(buf) {
        free(buf);
    }
    NVMERGE_TRACE("get imei ret [%d]\n", ret);
    return ret;
}
