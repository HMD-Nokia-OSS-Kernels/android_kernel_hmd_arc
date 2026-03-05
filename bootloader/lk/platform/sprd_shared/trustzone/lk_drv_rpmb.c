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

#include <lk_sec_drv.h>
#include <tee_smc_call.h>
#include <chipram_env.h>
#include <mmc.h>
#include <kernel/spinlock.h>
#ifdef CONFIG_UFS
#include <sprd_rpmb.h>
#endif
#include <string.h>

#ifdef CONFIG_MMC
#include <rpmb.h>
#endif

#ifdef CONFIG_UFS
#include <sprd_ufs.h>
#endif

#define RPMB_EKY_SIZE 32
extern spin_lock_t block_rw_lock;
#ifdef CONFIG_SPRD_GICV3
#define SPRD_BLOCK_LOCK(state) spin_lock_saved_state_t state; spin_lock_irqsave(&block_rw_lock, state)
#define SPRD_BLOCK_UNLOCK(state) spin_unlock_irqrestore(&block_rw_lock, state)
#else
#define SPRD_BLOCK_LOCK(state) sprd_spin_lock(&block_rw_lock)
#define SPRD_BLOCK_UNLOCK(state) sprd_spin_unlock(&block_rw_lock)
#endif

static u64 rpmb_size __attribute__((aligned(4096)));
static int is_rpmb_key __attribute__((aligned(4096)));

static u8 check_rpmb_key_pac[RPMB_DATA_FRAME_SIZE] __attribute__((aligned(4096)));

static u8 rpmb_key[RPMB_EKY_SIZE] __attribute__((aligned(4096)));

extern boot_device_t get_bootdevice(void);

int rpmb_blk_write(char *write_data, uint8_t *key, uint16_t blk_index)
{
	uint8_t ret = 0;
	SPRD_BLOCK_LOCK(state);
#ifdef CONFIG_MMC
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		ret = mmc_rpmb_blk_write(write_data, key, blk_index);
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		ret = ufs_rpmb_blk_write(write_data, key, blk_index);
#endif
	SPRD_BLOCK_UNLOCK(state);

	return ret;

}

int rpmb_blk_read(char *blk_data, uint16_t blk_index, uint8_t block_count)
{
	uint8_t ret = 0;
	SPRD_BLOCK_LOCK(state);
#ifdef CONFIG_MMC
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		ret = mmc_rpmb_blk_read(blk_data, blk_index, block_count);
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		ret = ufs_rpmb_blk_read(blk_data, blk_index, block_count);
#endif
	SPRD_BLOCK_UNLOCK(state);
	return ret;
}

uint32_t rpmb_read_writecount(void)
{
	uint32_t ret = 0;

#ifdef CONFIG_MMC
	if (get_bootdevice()  == BOOT_DEVICE_EMMC)
		ret = mmc_rpmb_read_writecount();
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice()  == BOOT_DEVICE_UFS)
		ret = ufs_rpmb_read_writecount();
#endif

	return ret;
}

int rpmb_write_pac(uint8_t *pac_data, uint16_t blk_index)
{
	uint8_t ret = 0;

#ifdef CONFIG_MMC
	if (get_bootdevice()  == BOOT_DEVICE_EMMC)
		ret = mmc_rpmb_write_pac(pac_data, blk_index);
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice()  == BOOT_DEVICE_UFS)
		ret = ufs_rpmb_write_pac(pac_data, blk_index);
#endif

	return ret;
}

int rpmb_swp_config_read(uint8_t *swp_config)
{
	uint8_t ret = 0;

#ifdef CONFIG_UFS
	if (get_bootdevice()  == BOOT_DEVICE_UFS)
		ret = ufs_rpmb_swp_config_read(swp_config);
#endif

	return ret;
}

int check_rpmb_key(uint8_t *package, int package_size)
{
	int ret = 0;
	SPRD_BLOCK_LOCK(state);
#ifdef CONFIG_MMC
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		ret = check_mmc_rpmb_key(package, package_size);
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		ret = check_ufs_rpmb_key(package, package_size);
#endif
	SPRD_BLOCK_UNLOCK(state);

	return ret;
}

int is_wr_rpmb_key(void)
{
	int ret = 0;
	SPRD_BLOCK_LOCK(state);
#ifdef CONFIG_MMC
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		ret = is_wr_mmc_rpmb_key();
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		ret = is_wr_ufs_rpmb_key();
#endif
	SPRD_BLOCK_UNLOCK(state);

	return ret;
}

int lk_set_rpmb_size(void)
{
	int ret = 0;

	SPRD_BLOCK_LOCK(state);
#ifdef CONFIG_MMC
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		rpmb_size = emmc_get_capacity(PARTITION_RPMB);
#endif
#ifdef CONFIG_UFS
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

	dprintf(INFO,"%s: rpmb size %lld\n", __func__, rpmb_size);
	smc_param *param = tee_common_call(FUNCTYPE_SET_RPMB_SIZE,
					   (uint32_t)(&rpmb_size),
					   sizeof(rpmb_size));

    return param->a0;
}


int lk_is_wr_rpmb_key(void)
{
	is_rpmb_key = -1;
	is_rpmb_key = is_wr_rpmb_key();

	if (0 == is_rpmb_key) {
		dprintf(INFO,"%s rpmb unwritten, call tos \n", __func__);
		smc_param *param = tee_common_call(FUNCTYPE_IS_WR_RPMB_KEY,
						   (uint32_t)(&is_rpmb_key),
						   sizeof(is_rpmb_key));
		return param->a0;
	} else {
		return -1;
	}
}

int lk_check_rpmb_key(void)
{
	int ret = 0;

	ret = check_rpmb_key(check_rpmb_key_pac, RPMB_DATA_FRAME_SIZE);
	if (0 == ret) {
		dprintf(INFO,"%s get rpmb package, call tos to check rpmb key \n", __func__);
		smc_param *param = tee_common_call(FUNCTYPE_CHECK_RPMB_KEY,
						   (uint32_t)check_rpmb_key_pac,
						   RPMB_DATA_FRAME_SIZE);
	return param->a0;
	} else {
		errorf("%s get rpmb package fail\n", __func__);
		return -1;
	}
}

int lk_set_rpmb_device_type(void)
{
	unsigned long rpmb_device_type = BOOT_DEVICE_EMMC;
#ifdef CONFIG_MMC
	if (get_bootdevice() == BOOT_DEVICE_EMMC) {
		rpmb_device_type = BOOT_DEVICE_EMMC;
	}
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice() == BOOT_DEVICE_UFS) {
		rpmb_device_type = BOOT_DEVICE_UFS;
	}
#endif
	smc_param *param = tee_common_call(FUNCTYPE_SET_RPMB_DEVICE_TYPE,
					   (uint32_t)(rpmb_device_type),
					   sizeof(rpmb_device_type));

    return param->a0;
}

/*
*reutrn 0 success
*/
int lk_write_rpmb_key(u8 *key)
{
	smc_param *param = NULL;
	int rc = 0;


	memset((void*)rpmb_key,  0, sizeof(rpmb_key));
	param = tee_common_call(FUNCTYPE_GET_RPMB_KEY, (uint32_t)(rpmb_key), sizeof(rpmb_key));

	if (NULL == param) {
		errorf("%s: tee_common_call faile\n", __func__);
		return -1;
	}

	if (0 != param->a0) {
		errorf("%s: get rpmb key fail %d.\n", __func__, param->a0);
		return param->a0;
	}

	if (0 != is_wr_rpmb_key()) {
		errorf("%s: rpmb key has been written.\n", __func__);
		if (key != NULL) {
			memcpy(key, rpmb_key, sizeof(rpmb_key));
		}
		return 0;
	}

	SPRD_BLOCK_LOCK(state);
#ifdef CONFIG_MMC
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		rc = mmc_rpmb_write_key(rpmb_key, sizeof(rpmb_key));
#endif
#ifdef CONFIG_UFS
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		rc = ufs_rpmb_write_key(rpmb_key, sizeof(rpmb_key));
#endif
	SPRD_BLOCK_UNLOCK(state);
	if (0 != rc) {
		errorf("%s: write rpmb key fail %d.\n", __func__, rc);
	} else {
		if (key != NULL) {
			memcpy(key, rpmb_key, sizeof(rpmb_key));
		}
	}

	memset((void*)rpmb_key,  0, sizeof(rpmb_key));
	dprintf(INFO,"%s: rpmb key write successful.\n", __func__);

	return rc;
}

