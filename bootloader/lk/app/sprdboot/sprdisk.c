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
#include <sprd_common_rw.h>

#define PART_SYSTEM	"userdata"
#define SPRDISK_RAMDISK_PATH	"/sprdisk/ramdisk.img"
#define SD_RAMDISK	"ramdisk.img"
extern int _sd_fat_mount(void);
extern int load_sd_ramdisk(void* buf,int size);
extern unsigned reboot_mode_check(void);
static int file_read(const char *mpart, const char *filenm, void *buf, int len)
{
	int ret = 0;
#ifdef CONFIG_FS_EXT4
	disk_partition_t info;
	block_dev_desc_t *dev_desc;

	loff_t len_read = 0;
	loff_t file_len = len;

	dev_desc = get_dev("mmc", 0);
	if (NULL == dev_desc) {
		errorf("ext4_read_content get dev error\n");
		return -1;
	}

	if (get_partition_info_by_name(dev_desc, mpart, &info) == -1) {
		errorf("## Valid EFI partition not found ##\n");
		return -1;
	}

	/* set the device as block device */
	ext4fs_set_blk_dev(dev_desc, &info);

	/* mount the filesystem */
	if (!ext4fs_mount(info.size)) {
		errorf("Bad ext4 partition:%s\n",  mpart);
		ext4fs_close();
		return -1;
	}

	ret = ext4fs_open(filenm, &file_len);
	if (ret < 0) {
		errorf("** %s doesn't exist %s **\n", mpart, filenm);
		return -1;
	}

	if (len != 0) {
		/* start read */
	    if (ext4fs_read(buf, file_len, &len_read) < 0){
			errorf("** Error ext4fs_read file %s **\n", filenm);
			ext4fs_close();
			return -1;
		}
	}

	ext4fs_close();
	ret = (int)file_len;
#endif
	return ret;
}

static int read_sd_ramdisk(void *buf, int size)
{
	int ret = 0;
#if 0
	if(size != 0)
		return load_sd_ramdisk(buf, size);
	ret = _sd_fat_mount();
	if (ret < 0) {
		errorf("mount fat fs in sd fail\n");
		return -1;
	}

	loff_t len = 0;
	ret = fat_size(SD_RAMDISK, &len);
	if (ret < 0) {
		errorf("get %s in sd fail\n", SD_RAMDISK);
		return -1;
	}

	ret = (int)len;
#endif
	return ret;
}

int boot_sprdisk(int offset, char *ramdisk_addr)
{
	int	ret;

	ret = file_read(PART_SYSTEM, SPRDISK_RAMDISK_PATH, (void *)ramdisk_addr, offset);
	if(ret <= 0){
		ret = read_sd_ramdisk((void *)ramdisk_addr, offset);
		if(ret < 0){
			errorf("sd read file %s wrong!\n", SD_RAMDISK);
		}
	}
	return ret;
}

int sprdisk_mode_detect(void)
{
	if(reboot_mode_check() == CMD_SPRDISK_MODE) {
		return 1;
	}

	return 0;
}
