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

#include <linux/types.h>
#include <malloc.h>
#include <mmc.h>
#ifdef CONFIG_UFS
#include <sprd_rpmb.h>
#include <sprd_ufs.h>
#endif
#include <string.h>
#include <chipram_env.h>
#include <sprd_imgversion.h>
#include <rpmb.h>
#include <secureboot/sec_common.h>
#include <lk_sec_drv.h>
#include <kernel/spinlock.h>

#define SPRD_IMGVER_MAGIC 0xA50000A5
extern spin_lock_t block_rw_lock;
#ifdef CONFIG_SPRD_GICV3
#define SPRD_BLOCK_LOCK(state) spin_lock_saved_state_t state; spin_lock_irqsave(&block_rw_lock, state)
#define SPRD_BLOCK_UNLOCK(state) spin_unlock_irqrestore(&block_rw_lock, state)
#else
#define SPRD_BLOCK_LOCK(state) sprd_spin_lock(&block_rw_lock)
#define SPRD_BLOCK_UNLOCK(state) sprd_spin_unlock(&block_rw_lock)
#endif

struct sprd_img_ver_t {
    uint32_t magic;
    uint32_t system_imgver;
    uint32_t vendor_imgver;
};

struct sprd_modem_img_ver_t {
    uint32_t magic;
    uint32_t l_modem_imgver;
    uint32_t l_ldsp_imgver;
    uint32_t l_lgdsp_imgver;
    uint32_t pm_sys_imgver;
    uint32_t agdsp_imgver;
    uint32_t wcn_imgver;
    uint32_t gps_imgver;
    uint32_t gpu_imgver;
    uint32_t vbmeta_imgver;
    uint32_t boot_imgver;
    uint32_t recovery_imgver;
    uint32_t socko_imgver;
    uint32_t odmko_imgver;
    uint32_t spare_imgver;
    uint32_t spares_imgver;
};

struct sprd_VAB_img_ver_t {
    uint32_t magic;
    uint32_t all_imgver[AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_LOCATIONS];
};

extern boot_device_t get_bootdevice(void);

int sprd_get_rpmb_block_cnt(void)
{
    u64 rpmb_size = 0;

    SPRD_BLOCK_LOCK(state);
#ifdef CONFIG_MMC
		if (get_bootdevice() == BOOT_DEVICE_EMMC)
			rpmb_size = emmc_get_capacity(PARTITION_RPMB);
#endif
#ifdef CONFIG_UFS
		int ret;

		if (get_bootdevice() == BOOT_DEVICE_UFS) {
			ret = prepare_rpmb_lu();
			if (ret != UFS_SUCCESS) {
				errorf("%s: prepare rpmb lu failed ret =%d\n", __func__, ret);
				return -1;
			}
			rpmb_size = (1 << rpmb_lu_info.log2blksz) *
					(rpmb_lu_info.blkcnt);
		}
#endif
    SPRD_BLOCK_UNLOCK(state);

    return (rpmb_size / RPMB_DATA_SIZE);
}


int get_sprdimgver_blk_ind(void)
{
	int ret;

	ret = sprd_get_rpmb_block_cnt();
	if (0 > ret ){
		errorf("%s: get rpmb block count error! return code %d \n", __func__, ret);
		return -1;
	}
	//The last two blocks save struct sprd_img_ver_t and are compatible with previous versions
	return ret - 2;
}

int get_sprdmodemimgver_blk_ind(void)
{
	int ret;

	ret = sprd_get_rpmb_block_cnt();
	if (0 > ret ){
		errorf("%s: get rpmb block count error! return code %d \n", __func__, ret);
		return -1;
	}

	//The last four blocks save struct sprd_modem_img_ver_t and are compatible with previous versions
	return ret - 4;

}

int sprd_get_imgversion(int imgType, unsigned int* swVersion)
{
    struct sprd_VAB_img_ver_t imgver;
    uint8_t data_rd[RPMB_DATA_SIZE]; //change data to 256-size,some emmc do not support size 512 at a time
    uint16_t block_ind, block_count;
    int ret;

    block_ind = get_sprdimgver_blk_ind();

    memset(data_rd, 0x0, sizeof(data_rd));
    block_count = sizeof(data_rd) / RPMB_DATA_SIZE;
    ret = rpmb_blk_read(data_rd, block_ind, block_count);
    if(ret < 0) {
        errorf("%s: rpmb read blk %d fail! ret %d \n", __func__, block_ind, ret);
        return ret;
    }

#ifdef IMGVER_DEBUG
    dprintf(INFO,"%s: rpmb read blk %d successful \n", __func__, block_ind);
#endif

    memcpy((void *)&imgver, data_rd, sizeof(struct sprd_VAB_img_ver_t));

    if(imgver.magic != SPRD_IMGVER_MAGIC) {
#ifdef IMGVER_DEBUG
        errorf("invalid sprd imgversion magic %x exp %x \n",
            imgver.magic, SPRD_IMGVER_MAGIC);
#endif
        return -1;
    }

    if((imgType > 31) || (imgType < 0)) {
        errorf("invalid sprd image type %d\n", imgType);
        return -1;
    }

    *swVersion = imgver.all_imgver[imgType];
#ifdef IMGVER_DEBUG
    dprintf(INFO,"sprd image type %d, swVersion:0x%x\n", imgType, *swVersion);
#endif
    return 0;
}

int sprd_get_b_slot_imgversion(int imgType, unsigned int* swVersion)
{
    struct sprd_VAB_img_ver_t imgver = {0};
    uint8_t data_rd[RPMB_DATA_SIZE];
    uint16_t block_ind = 0;
    uint16_t block_count = 0;
    int ret = -1;

    block_ind = get_sprdmodemimgver_blk_ind();

    memset(data_rd, 0x0, sizeof(data_rd));
    block_count = sizeof(data_rd) / RPMB_DATA_SIZE;
    ret = rpmb_blk_read(data_rd, block_ind, block_count);
    if(ret < 0) {
        errorf("%s: rpmb read blk %d fail! ret %d \n", __func__, block_ind, ret);
        return ret;
    }

#ifdef IMGVER_DEBUG
    dprintf(INFO,"%s: rpmb read blk %d successful \n", __func__, block_ind);
#endif

    memcpy((void *)&imgver, data_rd, sizeof(struct sprd_VAB_img_ver_t));

    if(imgver.magic != SPRD_IMGVER_MAGIC) {
#ifdef IMGVER_DEBUG
        errorf("invalid sprd imgversion magic %x exp %x \n",
            imgver.magic, SPRD_IMGVER_MAGIC);
#endif
        return -1;
    }

    if((imgType > 31) || (imgType < 0)) {
        errorf("invalid sprd image type %d\n", imgType);
        return -1;
    }

    *swVersion = imgver.all_imgver[imgType];
#ifdef IMGVER_DEBUG
    dprintf(INFO,"sprd image type %d, b_slot swVersion:0x%x\n", imgType, *swVersion);
#endif
    return 0;
}

int sprd_get_all_imgversion(VbootVerInfo* vboot_ver_info)
{
	struct sprd_VAB_img_ver_t imgver;
	uint8_t data_rd[RPMB_DATA_SIZE]; //change data to 256-size,some emmc do not support size 512 at a time
	uint16_t block_ind, block_count;
	int ret, i;
	uint32_t ab_slot_flag = 0;

	ab_slot_flag = vboot_ver_info->ab_slot_flag;
	dprintf(INFO,"%s: ab_slot_flag is %d \n", __func__, ab_slot_flag);

	if(0 == ab_slot_flag){// a slot or NO_AB
		block_ind = get_sprdimgver_blk_ind();
	}else{
		block_ind = get_sprdmodemimgver_blk_ind();
	}
	memset(data_rd, 0x0, sizeof(data_rd));
	block_count = sizeof(data_rd) / RPMB_DATA_SIZE;
	ret = rpmb_blk_read(data_rd, block_ind, block_count);
	if(ret < 0) {
		errorf("%s: rpmb read blk %d fail! ret %d \n", __func__, block_ind, ret);
		return ret;
	}

#ifdef IMGVER_DEBUG
	dprintf(INFO,"%s: rpmb read blk %d successful \n", __func__, block_ind);
#endif
	memcpy((void *)&imgver, data_rd, sizeof(struct sprd_VAB_img_ver_t));

	if(imgver.magic != SPRD_IMGVER_MAGIC) {
#ifdef IMGVER_DEBUG
		errorf("invalid sprd imgversion magic %x exp %x \n",
				imgver.magic, SPRD_IMGVER_MAGIC);
#endif
		return -1;
	} else {
		for(i = 0; i < 32; i++){
			vboot_ver_info->img_ver[i] = imgver.all_imgver[i];
		}
//		memcpy(vboot_ver_info->img_ver, imgver.all_imgver, AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_LOCATIONS);
	}
#ifdef IMGVER_DEBUG
	for(ret = 0; ret < AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_LOCATIONS; ret++) {
		dprintf(INFO,"vboot_ver_info->img_ver[%d] = 0x%x\n", ret, vboot_ver_info->img_ver[ret]);
	}
#endif

	return 0;
}

int sprd_init_all_imgversion(uint8_t *rpmb_key)
{
    struct sprd_img_ver_t imgver;
    struct sprd_modem_img_ver_t modem_imgver;
    uint8_t data_wr[RPMB_DATA_SIZE]; //change data to 256-size,some emmc do not support size 512 at a time
    uint16_t block_ind;
    int ret;

    memset(data_wr, 0x0, sizeof(data_wr));
    memset(&imgver, 0x0, sizeof(imgver));
    imgver.magic = SPRD_IMGVER_MAGIC;
    memcpy((void *)data_wr, &imgver, sizeof(imgver));

    block_ind = get_sprdimgver_blk_ind();
    ret = rpmb_blk_write(data_wr, rpmb_key, block_ind);
    if(ret != 0) {
        errorf("%s: init imgversion fail,rpmb write blk %d fail! ret %d \n", __func__, block_ind, ret);
        return ret;
    }
    dprintf(INFO,"%s: init imgversion susseccful,rpmb write blk %d \n", __func__, block_ind);


    memset(data_wr, 0x0, sizeof(data_wr));
    memset(&modem_imgver, 0x0, sizeof(modem_imgver));
    modem_imgver.magic = SPRD_IMGVER_MAGIC;
    memcpy((void *)data_wr, &modem_imgver, sizeof(modem_imgver));

    block_ind = get_sprdmodemimgver_blk_ind();
    ret = rpmb_blk_write(data_wr, rpmb_key, block_ind);
    if(ret != 0) {
        errorf("%s: init modemimgver fail,rpmb write blk %d fail! ret %d \n", __func__, block_ind, ret);
        return ret;
    }
    dprintf(INFO,"%s: init modemimgver susseccful,rpmb write blk %d \n", __func__, block_ind);

    return 0;
}

