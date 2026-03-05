// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017 The Android Open Source Project
 */

#include <linux/kernel.h>
#include <sprd_common.h>
#include <bootloader_message.h>
#include <android_ab.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <lib/cksum.h>
#include <chipram_env.h>
#include <boot_mode.h>
#include <part.h>
#include "sprd_common_rw.h"

#ifdef CONFIG_SPL_DOUBLE_SLOT
#define SPL_HEADER_SLOT_A 	(1)
#define SPL_HEADER_SLOT_B	(2)
#endif

//DECLARE_GLOBAL_DATA_PTR;
extern int get_buffer_base_size_from_dt(const char *name, unsigned long *basep, unsigned long *sizep);
extern void reboot_devices(unsigned reboot_mode);

static uint32_t ab_control_compute_crc(struct bootloader_control *abc)
{
	return crc32(0, (void *)abc, offsetof(typeof(*abc), crc32_le));
}

/**
 * Load the boot_control struct from disk into newly allocated memory.
 * Normally the "misc" partition should be used.
 */
static int ab_control_read_from_disk(const char *partition_name,
				       const disk_partition_t *part_info,
				       struct bootloader_control **abc)
{
	uint64_t abc_offset, abc_size, ret;

	abc_offset = offsetof(struct bootloader_message_ab, slot_suffix);
	abc_size = sizeof(struct bootloader_control);
	debugf("abc offset: %llu, size: %llu\n", abc_offset, abc_size);

	if (abc_offset + abc_size > (uint64_t)(part_info->blk_cnt * part_info->blksz)) {
		errorf("ANDROID: boot control partition %s too small. Need at", partition_name);
		errorf(" least size %llu but have size "LBAFU" .\n",
			abc_offset + abc_size, part_info->blk_cnt);
		return -EINVAL;
	}
	*abc = malloc(abc_size);
	if (*abc == NULL)
		return -ENOMEM;

	ret = common_raw_read(partition_name, abc_size, abc_offset, (char *)*abc);
	if (ret) {
		errorf("ANDROID: Could not read from boot ctrl partition\n");
		free(*abc);
		return -EIO;
	}

	debugf("ANDROID: Loaded ABC control from %s, size: %llu\n", partition_name, abc_size);

	return 0;
}

/**
 * Set bootloader_control struct as default value.
 *
 * This value will be used when the disk bootloader message
 * is corrupted.
 */
static int ab_control_default(struct bootloader_control *abc)
{
	int i;
	const struct slot_metadata metadata = {
		.priority = 15,
		.tries_remaining = 7,
		.successful_boot = 0,
		.verity_corrupted = 0,
		.reserved = 0
	};

	if (abc == NULL)
		return -EFAULT;

	memcpy(abc->slot_suffix, "_a\0\0", 4);
	abc->magic = BOOT_CTRL_MAGIC;
	abc->version = BOOT_CTRL_VERSION;
	abc->nb_slot = NUM_SLOTS;
	memset(abc->reserved0, 0, sizeof(abc->reserved0));
	for (i = 0; i < abc->nb_slot; ++i)
		abc->slot_info[i] = metadata;

	/* It is forbidden to start from slot B during initialization. */
	abc->slot_info[1].tries_remaining = 0;

	/* It is used to resolve the occurrence of the 8th failure to enter calibration mode*/
	abc->slot_info[0].successful_boot = 1;

	memset(abc->reserved1, 0, sizeof(abc->reserved1));

	/* At last caculate the crc32 of bootloader control struct */
	abc->crc32_le = ab_control_compute_crc(abc);

	return 0;
}

/**
 * Store the boot_control message to block.
 *
 * Store the same location it was read from with
 * read_ab_control_from_misc().
 * @return 0 on success and a negative on error
 */
static int ab_control_store(const char *partition_name,
			    const disk_partition_t *part_info,
			    struct bootloader_control *abc)
{
	uint64_t abc_offset, abc_size, ret;

	abc_offset = offsetof(struct bootloader_message_ab, slot_suffix);
	abc_size = sizeof(struct bootloader_control);
	ret = common_raw_write(partition_name, abc_size, 0, abc_offset, (char *)abc);
	if (ret) {
		errorf("ANDROID: Could not write boot control message to %s partition\n", partition_name);
		return -EIO;
	}

	return 0;
}

/**
 * Compare two slots.
 *
 * The function determines which slot should we boot from among the two.
 *
 * @param a The first bootable slot metadata
 * @param b The second bootable slot metadata
 * @return Negative if the slot "a" is better, positive if the slot "b" is
 *         better or 0 if they are equally good.
 */
static int ab_compare_slots(const struct slot_metadata *a,
			    const struct slot_metadata *b)
{
	/* Higher priority is better */
	if (a->priority != b->priority)
		return b->priority - a->priority;

	/* Higher successful_boot is better */
	if (a->successful_boot != b->successful_boot)
		return b->successful_boot - a->successful_boot;

	/* Higher tries_remaining is better */
	if (a->tries_remaining != b->tries_remaining)
		return b->tries_remaining - a->tries_remaining;

	/* They are equally good. */
	return 0;
}

/**
 * roll back spl.
 *
 * This function is used to roll back the SPL after a failed upgrade.
 *
 * @return 0 on success and a negative on error
 */
static int ab_slot_rollback(void)
{
	char *ifname;
	unsigned long rb_base = 0;
	uint64_t total_part_size = 0;

	ifname = block_dev_get_name();
	// if (get_buffer_base_size_from_dt("heap@4", &rb_base, &rb_size) < 0)
	// 	return -1;
	rb_base = FB_BUF_ADDR;

	if ((total_part_size = get_devsize_hwpart(ifname, BOOT_PART2)) == 0) {
		errorf("%s: BOOT_PART2 size is zero, stop rollback!\n", __func__);
		return -1;
	}

	if (0 != common_raw_read("splloader_bak", (uint64_t)total_part_size, (uint64_t)0, (char *) rb_base)) {
		errorf("splloader_bak read error\n");
		return -1;
	}
	if (0 != common_raw_write("splloader", (uint64_t)total_part_size, (uint64_t)0, (uint64_t)0, (char *) rb_base)) {
		errorf("splloader write error\n");
		return -1;
	}
	reboot_devices(CMD_NORMAL_MODE);
	return 0;
}

/**
 * clear spl adjust flag.
 *
 * This function is used to clear spl adjust flag.
 *
 * @return 0 on success and a negative on error
 */

static int ab_slot_clear_spl_adjust_flag(void)
{
	struct bootloader_control *abc = NULL;
	u32 crc32_le;
	int ret;
	bool store_needed = false;
	disk_partition_t part_info = {0};
	int rb_slot;

	ret = get_img_partition_info("misc", &part_info);
	if (ret) {
		errorf("get info from misc partition failed!\n");
		return ret;
	}

	ret = ab_control_read_from_disk("misc", &part_info, &abc);
	/*
	 * Fixme:
	 * When can't get ab_control from disk partition, should not go
	 * to the next step.
	 */
	if (ret < 0) {
		return ret;
	}

	crc32_le = ab_control_compute_crc(abc);
	if (abc->crc32_le != crc32_le) {
		debugf("ANDROID: Invalid crc32_le (caculate: %.8x, read from disk %.8x),",
			crc32_le, abc->crc32_le);
		debugf("Initializing ab_control to default value\n");

		ret = ab_control_default(abc);
		if (ret < 0) {
			errorf("Initializing ab_control failed!\n");
			free(abc);
			return -ENODATA;
		}
		/* Should store boot_control to the disk partition after */
		store_needed = true;
	}

	/* Verify the ab_control message */
	if (abc->magic != BOOT_CTRL_MAGIC) {
		errorf("ANDROID: Bad ab_control magic: %.8x\n", abc->magic);
		free(abc);
		return -ENODATA;
	}

	if (abc->version > BOOT_CTRL_VERSION) {
		errorf("ANDROID: Unsupported ab_control version: %.8x\n",
			abc->version);
		free(abc);
		return -ENODATA;
	}

	/* Safety check: limit the number of slots not exceed 4. */
	if (abc->nb_slot > ARRAY_SIZE(abc->slot_info)) {
		abc->nb_slot = ARRAY_SIZE(abc->slot_info);
		/* Should store boot_control to the disk partition after */
		store_needed = true;
	}

	if (abc->reserved0[0] != 0) {
		dprintf(ALWAYS, "do clear adjust spl flag in misc.\n");
		memset(abc->reserved0, 0, sizeof(abc->reserved0));
		store_needed = true;
	}

	if (get_boot_role() == BOOTLOADER_MODE_LOAD && store_needed) {
		abc->crc32_le = ab_control_compute_crc(abc);
		ab_control_store("misc", &part_info, abc);
	}
	free(abc);

	return 0;
}

/**
 * roll back spl partition only.
 *
 * This function is used to roll back the SPL after a failed upgrade.
 *
 * @return 0 on success and a negative on error
 */
int ab_slot_rollback_spl(bool update_misc, uint8_t status)
{
	char *ifname;
	unsigned long rb_base = 0;
	uint64_t total_part_size = 0;
	char magicdata[4];
	u8 i = 0;
	uint8_t roll_back_status = 0;

	ifname = block_dev_get_name();
	rb_base = FB_BUF_ADDR;
	if ((total_part_size = get_devsize_hwpart(ifname, BOOT_PART1)) == 0)
			return -1;

	if (0 != common_raw_read("splloader", (uint64_t)total_part_size, (uint64_t)0, (char *) rb_base)) {
			errorf("splloader read error\n");
			return -1;
	}
	memcpy(magicdata, (char *) rb_base, 4);
	dprintf(INFO,"status: %d, original splloader magic data :%02x %02x %02x %02x\n",
		status, magicdata[0], magicdata[1], magicdata[2], magicdata[3]);

	for (i=0; i<4; i++) {
		magicdata[i] ^= 0xFF;
	}

	if (status == 1) {
		if (magicdata[0] == 0x44 && magicdata[1] == 0x48
			&& magicdata[2] == 0x54 && magicdata[3] == 0x42) {
			roll_back_status = 1;
			errorf("rollback: do spl repair.\n");
		}
	} else if (status == 2) {
		if (magicdata[0] == 0xBB && magicdata[1] == 0xB7
			&& magicdata[2] == 0xAB && magicdata[3] == 0xBD) {
			errorf("rollback: do spl destory.\n");
			roll_back_status = 1;
		}
	}
	if (roll_back_status == 1) {
		memcpy((char *) rb_base, magicdata, 4);
		if (0 != common_raw_write("splloader", (uint64_t)total_part_size, (uint64_t)0, (uint64_t)0, (char *) rb_base)) {
			errorf("splloader write error\n");
			return -1;
		} else
			errorf("splloader write success.\n");
	}
	if (update_misc == true)
			ab_slot_clear_spl_adjust_flag();

	reboot_devices(CMD_NORMAL_MODE);
	return 0;
}

#ifdef CONFIG_SPL_DOUBLE_SLOT
static int adjust_spl_header_slot(int slot)
{
	dprintf(ALWAYS, "enter %s, slot:%d\n", __func__, slot);
	sys_img_header *spl_header = NULL;
	struct bootloader_control *abc = NULL;
	int ret;
	disk_partition_t part_info = {0};

	//Read slot_metadata[a]、[b]
	ret = get_img_partition_info("misc", &part_info);
	if (ret) {
		errorf("get info from misc partition failed!\n");
		return ret;
	}

	ret = ab_control_read_from_disk("misc", &part_info, &abc);
	dprintf(ALWAYS, "slot_metadata[a]:0x%x, slot_metadata[b]:0x%x\n", abc->slot_info[0], abc->slot_info[1]);

	spl_header = malloc_cache_aligned(sizeof(sys_img_header));
	if (spl_header == NULL) {
		errorf("No enough memory for spl_header\n");
		goto fail;
	}
	memset(spl_header, 0, sizeof(sys_img_header));

	if (0 != common_raw_read("splloader", (uint64_t)sizeof(sys_img_header), (uint64_t)0, (char *)spl_header)) {
		errorf("splloader_bak read error\n");
		goto fail;
	}

	if (spl_header->mMagicNum == IMG_BAK_HEADER) {
		dprintf(ALWAYS, "orig spl_header->spl_slot:%x\n", spl_header->slot);
		if (slot == -1) {
			if (spl_header->slot == SPL_HEADER_SLOT_A) {
				spl_header->slot = SPL_HEADER_SLOT_B;
			} else if (spl_header->slot == SPL_HEADER_SLOT_B) {
				spl_header->slot = SPL_HEADER_SLOT_A;
			} else {//for spl header no slot info
				dprintf(ALWAYS, "spl header no slot info, set spl_header->slot to %x default\n", SPL_HEADER_SLOT_A);
				spl_header->slot = SPL_HEADER_SLOT_A;
			}
		} else {
			if (slot == 0) {
				spl_header->slot = SPL_HEADER_SLOT_A;
			} else if (slot == 1) {
				spl_header->slot = SPL_HEADER_SLOT_B;
			}
		}
		dprintf(ALWAYS, "rollback: adjust spl_header->slot to %x\n", spl_header->slot);

		if (0 != common_raw_write("splloader", (uint64_t)sizeof(sys_img_header), (uint64_t)0, (uint64_t)0, (char *)spl_header)) {
				errorf("splloader write error\n");
				goto fail;
		}
		free(spl_header);
		dprintf(ALWAYS, "Have already adjust spl slot, reboot now!\n");
		reboot_devices(CMD_NORMAL_MODE);
	} else {
		errorf("No mMagicNum in spl header!\n");
		goto fail;
	}

fail:
	if (spl_header != NULL) {
		free(spl_header);
	}
	return -1;
}
#endif

int adjust_spl_slot(void)
{
	uint8_t spl_status = 0;
	chipram_env_t* cr_env = get_chipram_env();
	chipram_env_t * env = (chipram_env_t *)CHIPRAM_ENV_LOCATION;
#ifdef CONFIG_SPL_DOUBLE_SLOT
	if ((cr_env->spl_edit_flag == 0x1) && (cr_env->dual_spl_flag == 0x1)) {
		dprintf(ALWAYS, "Romcode support dual spl enabled, cr_env->spl_edit_flag:0x%x\n", cr_env->spl_edit_flag);
		cr_env->spl_edit_flag = 0x0;
		env->spl_edit_flag = 0x0;
		if (adjust_spl_header_slot(-1) == -1) {
			return -1;
		}
	} else {
#endif
		if (0x55CC55CC == cr_env->spl_adjust_flag)
			spl_status = 1;
		else if (0x55DD55DD == cr_env->spl_adjust_flag)
			spl_status = 2;
		/* spl_status=1:repair main spl,spl_status=2:destory main spl(for user operate reset after OTA) */
		if (spl_status) {
			cr_env->spl_adjust_flag = 0x0;
			env->spl_adjust_flag = 0x0;
			/* spl rollback */
			if (ab_slot_rollback_spl(true, spl_status) == -1) {
				return -1;
			}
		}
#ifdef CONFIG_SPL_DOUBLE_SLOT
	}
#endif
	return 0;
}

int ab_select_slot(void)
{
	struct bootloader_control *abc = NULL;
	u32 crc32_le;
	int slot, i, ret;
	bool store_needed = false;
	char slot_suffix[4];
	disk_partition_t part_info = {0};
	int rb_slot;
	uint8_t temp = 0;
#ifdef CONFIG_SPL_DOUBLE_SLOT
	chipram_env_t* cr_env = get_chipram_env();
#endif

	ret = get_img_partition_info("misc", &part_info);
	if (ret) {
		errorf("get info from misc partition failed!\n");
		return ret;
	}

	ret = ab_control_read_from_disk("misc", &part_info, &abc);

	/*
	 * Fixme:
	 * When can't get ab_control from disk partition, should not go
	 * to the next step.
	 */
	if (ret < 0) {
		return ret;
	}

	crc32_le = ab_control_compute_crc(abc);
	if (abc->crc32_le != crc32_le) {
		debugf("ANDROID: Invalid crc32_le (caculate: %.8x, read from disk %.8x),",
			crc32_le, abc->crc32_le);
		debugf("Initializing ab_control to default value\n");

		ret = ab_control_default(abc);
		if (ret < 0) {
			errorf("Initializing ab_control failed!\n");
			free(abc);
			return -ENODATA;
		}
		/* Should store boot_control to the disk partition after */
		store_needed = true;
	}

	/* Verify the ab_control message */
	if (abc->magic != BOOT_CTRL_MAGIC) {
		errorf("ANDROID: Bad ab_control magic: %.8x\n", abc->magic);
		free(abc);
		return -ENODATA;
	}

	if (abc->version > BOOT_CTRL_VERSION) {
		errorf("ANDROID: Unsupported ab_control version: %.8x\n",
			abc->version);
		free(abc);
		return -ENODATA;
	}

	/*
	 * Now we have got a valid boot control stored in abc, handle and select one.
	 */

	/* Safety check: limit the number of slots not exceed 4. */
	if (abc->nb_slot > ARRAY_SIZE(abc->slot_info)) {
		abc->nb_slot = ARRAY_SIZE(abc->slot_info);
		/* Should store boot_control to the disk partition after */
		store_needed = true;
	}

	slot = -1;
	rb_slot = -1;
	/*
	 *  - Slot should not marked as corrupted and
	 *  - either has tries_remaining > 0 or successful_boot is true.
	 */
	for (i = 0; i < abc->nb_slot; ++i) {
		if (abc->slot_info[i].verity_corrupted ||
		    !abc->slot_info[i].tries_remaining) {
			debugf("ANDROID: unbootable slot %d tries_remaining: %d, ",
				  i, abc->slot_info[i].tries_remaining);
			debugf("verity_corrupted: %d\n",
				  abc->slot_info[i].verity_corrupted);
			/* rolling back after */
			rb_slot = i;
			continue;
		}
		dprintf(ALWAYS, "ANDROID: bootable slot %d priority: %d, tries_remaining: %d, ",
			  i, abc->slot_info[i].priority,
			  abc->slot_info[i].tries_remaining);
		dprintf(ALWAYS, "verity_corrupted: %d, successful_boot: %d\n",
			  abc->slot_info[i].verity_corrupted,
			  abc->slot_info[i].successful_boot);

		if (!abc->slot_info[i].successful_boot &&
		    abc->slot_info[i].tries_remaining == 1) {
			errorf("ANDROID: check rollback slot %d tries: %d\n",
				  i, abc->slot_info[i].tries_remaining);
			rb_slot = i;
			continue;
		}

		/* For first slot or select higher priority slot */
		if (slot < 0 ||
		    ab_compare_slots(&abc->slot_info[i],
				     &abc->slot_info[slot]) < 0) {
			slot = i;
		}
	}

	/* The higher priority slot boot fail, rolling back splloader and reboot. */
	if (rb_slot >= 0 && slot >= 0
		&& abc->slot_info[rb_slot].priority > abc->slot_info[slot].priority
		&& get_boot_role() == BOOTLOADER_MODE_LOAD) {
		debugf("ANDROID: slot %d booted fail, rolling back spl and reboot into normal\n", rb_slot);
		temp = abc->slot_info[rb_slot].priority;
		abc->slot_info[rb_slot].priority = abc->slot_info[slot].priority;
		abc->slot_info[slot].priority = temp;
		abc->slot_info[rb_slot].successful_boot = 0;
		abc->crc32_le = ab_control_compute_crc(abc);
		ab_control_store("misc", &part_info, abc);
		dprintf(ALWAYS, "exchange boot slot!!!\n");
#ifdef CONFIG_SPL_DOUBLE_SLOT
		if (cr_env->dual_spl_flag == 0x1) {
			if (adjust_spl_header_slot(slot) == -1) {
				free(abc);
				return -1;
			}
		}
#endif
		if (-1 == ab_slot_rollback_spl(false, 1)) {
			errorf("ANDROID: slot %d rolling back fail, stop reboot\n", rb_slot);
			free(abc);
			return -1;
		} else {
			errorf("[%s]: slot %d rolling back success, rebooting now!\n", __func__, rb_slot);
			while(1);
		}
	}

	/*
	 * If the selected slot has a false successful_boot, we also decrement
	 * the tries_remaining until it reaches 0(becomes unbootable). This
	 * mechanism produces a bootloader induced rollback, typically right
	 * after a failed update.
	 */
	if (slot >= 0 && !abc->slot_info[slot].successful_boot) {
		debugf("ANDROID: Attempting slot %c, tries_remaining %d\n",
			BOOT_SLOT_NAME(slot),
			abc->slot_info[slot].tries_remaining);
		abc->slot_info[slot].tries_remaining--;
		store_needed = true;
	}

	if (slot >= 0) {
		/*
		 * Update slot_suffix area.
		 * Legacy user-space requires this field to be set in the BCB.
		 * Newer releases load this slot suffix from the command line
		 * or the device tree.
		 */
		memset(slot_suffix, 0, sizeof(slot_suffix));
		slot_suffix[0] = '_';
		slot_suffix[1] = BOOT_SLOT_NAME(slot);
		if (memcmp(abc->slot_suffix, slot_suffix,
			   sizeof(slot_suffix))) {
			memcpy(abc->slot_suffix, slot_suffix,
			       sizeof(slot_suffix));
			store_needed = true;
		}
	}

	if (get_boot_role() == BOOTLOADER_MODE_LOAD && store_needed) {
		abc->crc32_le = ab_control_compute_crc(abc);
		ab_control_store("misc", &part_info, abc);
	}
	free(abc);

	if (slot < 0) {
		errorf("Have not get right slot\n");
		return -EINVAL;
	}

	return slot;
}

int get_slot_ab(char *ab_part_name, const char *src)
{
	if(src != NULL)
		strcpy(ab_part_name, src);

#ifdef CONFIG_ANDROID_AB
	if(!strlen(g_env_slot)) {
		errorf("get_slot_ab: g_env_slot is empty!\n");
		return -1;
	}
	strcat(ab_part_name, g_env_slot);
#endif
	return 0;
}
