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
//ZOVERLAY_TAG_HMD_ONEIMAGE

#include <linux/kernel.h>
#include <linux/byteorder/little_endian.h>
#include <libfdt.h>
#include <lib/mincrypt/sha256.h>
#include <chipram_env.h>
#include <miscdata_def.h>
#include <sprd_common.h>
#include <sprd_common_rw.h>
#include <sprd_cpcmdline.h>
#include <sprd_log.h>
#include <sprd_wdt.h>
#include "boot_parse.h"
#include "boot_mode.h"
#include "sprd_fdt_support.h"
#include "sprd_fdt_memory.h"
#include <sprd_cpcmdline.h>
#include <fdtdec.h>
#include <tee_smc_call.h>
#include <lk_sec_drv.h>
#include "sprd_bootconfig_support.h"
#ifdef SPRD_VBOOT_V2
#include <uboot_avb_ops.h>
#endif
#include <sprd_boardid.h>
#include <lk/board.h>
extern char *get_product_sn(void);
extern unsigned int g_DtboIndex;
extern enVerifiedState g_verifiedbootstate;
extern boot_device_t get_bootdevice(void);
extern char *bootconfig_get_hwfeature(void);
extern char *bootconfig_get_bootmode(void);
extern char *bootconfig_get_chipid(void);

static uint64_t bootconfig_size = 0;

struct bootconfig_cmd_data{
/*
 * 	the location where the parameters are stored
 */
	uint32_t c_cmd_offset;
	uint32_t n_cmd_offset;
};

void bootconfig_chosen_bootargs_append(uint8_t *ramdisk_addr, const char* buf)
{
	int len = strlen(buf);

	memcpy(ramdisk_addr + bootconfig_size, buf, len);
	bootconfig_size += len;
	dprintf(INFO,"fixup %s finished!\n", buf);
}

struct bootconfig_cmd_data find_in_bootconfig(uint8_t *ramdisk_addr, char *buf)
{
	char *take;
	char *save;
	const char *lim = "\n";
	const char *lim1 = "=";
	char *take1;
	char *save1;
	char *find;
	struct bootconfig_cmd_data ret = {.c_cmd_offset=0, .n_cmd_offset=0};

	if(bootconfig_size == 0) {
		debugf("nothing in bootconfig!\n");
		return ret;
	}

	find = malloc(bootconfig_size);
	if (find == NULL) {
		errorf("bootconfig_replace malloc failed!\n");
		return ret;
	}
	memcpy(find, ramdisk_addr, bootconfig_size);
	take = strtok_r(find, lim, &save);

	if(strncasecmp(take, buf, strlen(buf))==0)
		ret.c_cmd_offset = 0;
	else {
		while(strncasecmp(take, buf, strlen(buf))!=0) {
			ret.c_cmd_offset += strlen(take)+1;
			if(ret.c_cmd_offset==bootconfig_size) {
//				dprintf(INFO,"%s is not in bootconfig list!\n", buf);
				ret.c_cmd_offset = 0;
				free(find);
				return ret;
			}
			take = strtok_r(NULL, lim, &save);
			if (strncasecmp(take, buf, strlen(buf))==0)
				break;
		};
	}

	take1 = strtok_r(take, lim1, &save1);
	//take1 = strtok_r(NULL, lim1, &save1);
	ret.n_cmd_offset = ret.c_cmd_offset + strlen(save1)+strlen(buf)+1;

	free(find);
	return ret;
}

int delete_bootconfig(uint8_t *ramdisk_addr, const char *buf)
{
	int c_offset, n_offset, size;
	char *bufin = malloc(strlen(buf)+1);
	if (bufin == NULL) {
		errorf("delete_bootconfig malloc failed!\n");
		return -1;
	}

	sprintf(bufin, "%s%s", buf, "=");
	struct bootconfig_cmd_data offset = find_in_bootconfig(ramdisk_addr, bufin);
	c_offset = offset.c_cmd_offset;
	n_offset = offset.n_cmd_offset;

	dprintf(INFO,"c_offset=%d\n",c_offset);
	dprintf(INFO,"n_offset=%d\n",n_offset);

	if((c_offset==0)&&(n_offset==0)) {
		dprintf(INFO,"%s is not in bootconfig list!\n", buf);
		free(bufin);
		return 0;
	} else {
		size = bootconfig_size - n_offset+1;
		memcpy(ramdisk_addr+c_offset, ramdisk_addr+n_offset, size);
		bootconfig_size = bootconfig_size-n_offset+c_offset;
		memset(ramdisk_addr+bootconfig_size, 0, n_offset - c_offset);
		dprintf(INFO,"delete %s finished!\n", buf);
		free(bufin);
		return 1;
	}
}

int bootconfig_replace(uint8_t *ramdisk_addr, const char *buf, char *change)
{
	int len;
	struct bootconfig_cmd_data offset;
	int c_offset;
	int n_offset;
	int size;
	int change_size;
	uint32_t buf_len = strlen(buf)+1;
	char *bufin = malloc(buf_len);
	if (bufin == NULL) {
		errorf("bootconfig_replace malloc-0 failed!\n");
		return -1;
	}

	memset(bufin, '\0', buf_len);
	sprintf(bufin, "%s%s", buf, "=");
	len = strlen(bufin);
	offset = find_in_bootconfig(ramdisk_addr, bufin);
	c_offset = offset.c_cmd_offset;
	n_offset = offset.n_cmd_offset;
	size = n_offset - c_offset;
	change_size = strlen(change)-(size-len-1)-1;

	if ((c_offset==0)&&(n_offset==0)) {
		debugf("%s is not in bootconfig list!\n", buf);
		free(bufin);
		return 0;
	} else {
		if((bootconfig_size-n_offset)==0) {
			memcpy(ramdisk_addr+c_offset+len, change, strlen(change));
		} else {
			char *tmp_data = malloc(bootconfig_size - n_offset);
			if (tmp_data == NULL) {
				errorf("bootconfig_replace malloc-1 failed!\n");
				free(bufin);
				return -1;
			}

			memcpy(tmp_data, ramdisk_addr+n_offset, bootconfig_size-n_offset);
			memcpy(ramdisk_addr+c_offset+len, change, strlen(change));
			memcpy(ramdisk_addr+c_offset+len+strlen(change), tmp_data, bootconfig_size-n_offset);
			free(tmp_data);
		}
		bootconfig_size = bootconfig_size+change_size;
		dprintf(INFO,"replace %s to %s finished!\n", buf, change);
		free(bufin);
		return 1;
	}
}

void bootconfig_fixup_vendor_init_flag(uint8_t *ramdisk_addr)
{
	char buf[64] = {0};
	const char *boot_mode = NULL;

	boot_mode = g_env_bootmode;
	dprintf(INFO,"%s boot mode %s\n", __func__, boot_mode);
	/*cali/charger/factorytest*/
	if (strcmp("cali",boot_mode) == 0 || strcmp("charger",boot_mode) == 0 ||
		strcmp("factorytest",boot_mode) == 0) {
		sprintf(buf, "androidboot.vendor.skip.init=1\n");

	} else {
		sprintf(buf, "androidboot.vendor.skip.init=0\n");

	}
	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

#if DEBUG
void bootconfig_fixup_selinux_switch(uint8_t *ramdisk_addr)
{
	char buf[64] = {0};
	char selinux_info[SELINUX_INFO_LEN] = {0};

	if (common_raw_read("miscdata", (u64)SELINUX_INFO_LEN,
			(u64)SELINUX_SWITCH_OFFSET, selinux_info)) {
		errorf("read miscdata selinux authority error.\n");
	}

	if (!strcmp(selinux_info, "Selinux:0")) {
		sprintf(buf, "androidboot.selinux=permissive\n");
		bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
		dprintf(INFO,"androidboot.selinux=permissive\n");
	}
}
#endif

void bootconfig_fixup_dtbo_index(uint8_t *ramdisk_addr)
{
	char buf[64]={0};
	int dtboindex = g_DtboIndex;
	char c_dtboindex[3]={0};

	sprintf(c_dtboindex, "%d\n", dtboindex);

	// if dtbo idx exist,replace it
	if(!bootconfig_replace(ramdisk_addr, "androidboot.dtbo_idx", c_dtboindex)) {
		sprintf(buf, "androidboot.dtbo_idx=%d\n",dtboindex);
		bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	}
}

void bootconfig_fixup_ro_boot_ramsize(uint8_t *ramdisk_addr)
{
#ifdef SPRD_DDR_AUTO_DETECT
	char buf[64] = {0};

	phys_size_t ddr_size;
	uint64_t ro_boot_ddrsize = 0;
	int str_len;

	ddr_size = get_real_ram_size();
	ddr_size = ALIGN(ddr_size, 0x100000);
	ro_boot_ddrsize = ddr_size >> 20;

	sprintf(buf, "androidboot.ddrsize=%lldM", ro_boot_ddrsize);

	str_len = strlen(buf);
	buf[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);

	sprintf(buf, "androidboot.ddr_size=%lldM", ro_boot_ddrsize);
	str_len = strlen(buf);
	buf[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
#endif
}

void bootconfig_fixup_ddrsize_range(uint8_t *ramdisk_addr)
{
	char buf[64], buf2[32];
	const char *range = NULL;
	int str_len;
	memset(buf, 0, sizeof(buf));

#ifdef SPRD_DDR_AUTO_DETECT
	uint64_t ro_boot_ddrsize = 0;
	phys_size_t ddr_size;

	ddr_size = get_real_ram_size();
	ddr_size = ALIGN(ddr_size, 0x100000);
	ro_boot_ddrsize = ddr_size >> 20;

	if (ro_boot_ddrsize < 512) {
        	range = "[0,512)";
	} else if (ro_boot_ddrsize < 1024) {
        	range = "[512,1024)";
	} else if (ro_boot_ddrsize < 6144) {
        	uint64_t mid_size = 0;
        	mid_size = (ro_boot_ddrsize/1024)*1024;
        	sprintf(buf2, "[%lld,%lld)", mid_size, mid_size+1024);
        	range = buf2;
	} else {
        	range = "[6144,)";
	}
#else
	range = "[1024,2048)";
#endif

	sprintf(buf, "androidboot.ddrsize.range=%s", range);

	str_len = strlen(buf);
	buf[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

void bootconfig_fixup_wdten(uint8_t *ramdisk_addr)
{
	unsigned int val_en;
	char buf[32] = {0}, buf_p[32] = {0};

#if !DEBUG
	val_en = WDTEN_MAGIC;
#else
	val_en = 0;
#endif
	boot_adjust_wdt_flag();
	if (common_raw_read("miscdata", (uint64_t)WDTEN_DATA_LEN,
			WDTEN_DATA_OFFSET, buf_p)) {
		errorf("read wdten data error from miscdata...\n");
	} else {
		if (!strncmp("enabled", buf_p, strlen("enabled"))) {
			dprintf(INFO,"wdt was enabled by miscdata\n");
			val_en = WDTEN_MAGIC;
		} else if (!strncmp("disable", buf_p, strlen("disable"))) {
			dprintf(INFO,"wdt was disabled by miscdata\n");
			val_en = 0;
		} else {
			dprintf(INFO,"wdt has not beed assigned by miscdata\n");
		}
	}

	sprintf(buf, "androidboot.wdten=%x\n", val_en);

#if !DEBUG
	if (!val_en)
		stop_watchdog();
#endif

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

void bootconfig_fixup_dswdten(uint8_t *ramdisk_addr)
{
	char buf[32] = {0}, buf_p[32] = {0};

	if (common_raw_read("miscdata", (uint64_t)DSWDTEN_DATA_LEN,
			DSWDTEN_DATA_OFFSET, buf_p)){
		errorf("read dswdten data error from miscdata...\n");
		sprintf(buf, "androidboot.dswdten=%s\n", "disable");
	} else {
		if (!strncmp("enabled", buf_p, strlen("enabled"))) {
			dprintf(INFO,"dswdt was enabled by miscdata\n");
			sprintf(buf, "androidboot.dswdten=%s\n", "enabled");
		} else if (!strncmp("disable", buf_p, strlen("disable"))) {
			dprintf(INFO,"dswdt was disabled by miscdata\n");
			sprintf(buf, "androidboot.dswdten=%s\n", "disable");
		} else {
			dprintf(INFO,"dswdt has not beed assigned by miscdata\n");
			sprintf(buf, "androidboot.dswdten=%s\n", "disable");
		}
	}
	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

void bootconfig_fixup_verified_boot(uint8_t *ramdisk_addr)
{
	char buf[64] = {0};
	int str_len;

	switch(g_verifiedbootstate)
	{
		case v_state_green:
			sprintf(buf,"androidboot.verifiedbootstate=green");
			break;
		case v_state_yellow:
			sprintf(buf,"androidboot.verifiedbootstate=yellow");
			break;
		case v_state_orange:
			sprintf(buf,"androidboot.verifiedbootstate=orange");
			break;
		case v_state_red:
			sprintf(buf,"androidboot.verifiedbootstate=red");
			break;
		default:
			sprintf(buf,"androidboot.verifiedbootstate=green");
			break;
	}

	str_len = strlen(buf);
	buf[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

void bootconfig_fixup_flash_lock_state(uint8_t *ramdisk_addr)
{
	char buf[64] = {0};
	int str_len;

	switch(g_DeviceStatus) {
		case 1:
			sprintf(buf,"androidboot.flash.locked=0");
			break;
		case 0:
			sprintf(buf,"androidboot.flash.locked=1");
			break;
		default:
			sprintf(buf,"androidboot.flash.locked=1");
			break;
	}

	str_len = strlen(buf);
	buf[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

void bootconfig_fixup_dvfs_set(uint8_t *ramdisk_addr)
{
	unsigned int val = 0;
	char buf[32], dvfs_data[32] = {0};
	unsigned int *data_ptr = (unsigned int *)&dvfs_data[0];

	memset(buf, 0, 32);
	if (common_raw_read("miscdata", (uint64_t)DVFS_SET_LEN,
			    (uint64_t)DVFS_SET_OFFSET, dvfs_data)) {
		errorf("read dvfs_set data error from miscdata...\n");
		return;
	}

	val = *data_ptr;
	sprintf(buf, "androidboot.dvfs_set=0x%x\n", val);

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

void bootconfig_fixup_serialno(uint8_t *ramdisk_addr)
{
	char buf[255] = {0};
	int str_len;

	sprintf(buf, "androidboot.serialno=%s", get_product_sn());

	str_len = strlen(buf);
	buf[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}

#ifdef CONFIG_ANDROID_AB
void bootconfig_fixup_add_slot_suffix(uint8_t *ramdisk_addr)
{
	char buf[64] = {0};
	const char *slot;

	slot = g_env_slot;
	if (!slot) {
		errorf("get env: slot fail!\n");
	} else {
		snprintf(buf, ARRAY_SIZE(buf), "androidboot.slot_suffix=%s\n", slot);
		bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	}
}

#ifdef CONFIG_SPL_DOUBLE_SLOT
void bootconfig_fixup_dual_slot_flag(uint8_t *ramdisk_addr)
{
	char buf[64] = {0};
	chipram_env_t* cr_env = get_chipram_env();

	if (cr_env->dual_spl_flag == 0) {
		sprintf(buf, "androidboot.dual_spl_flag=0\n");
	} else {
		sprintf(buf, "androidboot.dual_spl_flag=1\n");
	}
	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}
#endif

void bootconfig_fixup_add_force_mode(uint8_t *ramdisk_addr)
{
	const char *buf;

	if (g_env_bootmode && (!strcmp("recovery", g_env_bootmode)))
		buf = "androidboot.force_normal_boot=0\n";
	else
		buf = "androidboot.force_normal_boot=1\n";

	dprintf(INFO,"set %s\n", buf);

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}
#endif

#if defined(SPRD_UFS_BOOTDEVICE) || defined(SPRD_EMMC_BOOTDEVICE)
void bootconfig_fixup_boot_device(uint8_t *ramdisk_addr)
{
	char buf[255] = {0};
	int str_len;

#if defined(CONFIG_BLK_DEV_BOOT)
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		sprintf(buf, SPRD_UFS_BOOTDEVICE); //ufs
	else if (get_bootdevice() == BOOT_DEVICE_EMMC)
		sprintf(buf, SPRD_EMMC_BOOTDEVICE); //eMMC
#else
	sprintf(buf, SPRD_EMMC_BOOTDEVICE); //eMMC
#endif

	str_len = strlen(buf);
	buf[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}
#endif

#ifdef SPRD_VBOOT_V2
void bootconfig_fixup_vboot(uint8_t *ramdisk_addr)
{
#ifdef CONFIG_VBOOT_SYSTEMASROOT
	bootconfig_fixup_vboot_system(ramdisk_addr);
#else
	bootconfig_fixup_ando_vboot(ramdisk_addr);
#endif
}

#ifdef CONFIG_VBOOT_SYSTEMASROOT
void bootconfig_fixup_vboot_system(uint8_t *ramdisk_addr)
{
	int str_len = 0;

	dprintf(INFO,"bootconfig_fixup_vboot_system enter \n");
	str_len = strlen((char *)g_vboot_sys_cmdline);
	if(str_len == 0) {
		dprintf(INFO,"g_vboot_sys_cmdline is null. \n");
	}
	g_vboot_sys_cmdline[str_len] = '\n';

	bootconfig_chosen_bootargs_append(ramdisk_addr, (char *)g_vboot_sys_cmdline);
	dprintf(INFO,"fdt vboot system str_len is %d", str_len);
}
#else
void bootconfig_fixup_ando_vboot(uint8_t *ramdisk_addr)
{
        int str_len = 0;
        int i;

        str_len = strlen((char *)g_sprd_vboot_cmdline);
        if(str_len == 0) {
                dprintf(INFO,"g_sprd_vboot_cmdline is null. \n");
        }

        for(i=0; i<str_len; i++) {
                if(g_sprd_vboot_cmdline[i]==' ') {
                        g_sprd_vboot_cmdline[i]='\n';
                }
        }

        g_sprd_vboot_cmdline[str_len] = '\n';

        bootconfig_chosen_bootargs_append(ramdisk_addr, (char *)g_sprd_vboot_cmdline);
        dprintf(INFO,"fdt vboot str_len is %d", str_len);
}
#endif
#endif

void bootconfig_fixup_hwfeature(uint8_t *ramdisk_addr)
{
#ifdef CONFIG_BOOTLOADER_HWFEATURE
	bootconfig_chosen_bootargs_append(ramdisk_addr, bootconfig_get_hwfeature());
#endif
}

void bootconfig_fixup_high_refresh(uint8_t *ramdisk_addr)
{
#ifdef CONFIG_HIGH_REFRESH
	const char *buf;
	int hr_version = 0;
	extern int sprd_get_tpic_version(void);

	hr_version = sprd_get_tpic_version();
	if (hr_version == 1)
		buf = "androidboot.high_refresh=ums9230s\n";
	else if (hr_version == 2)
		buf = "androidboot.high_refresh=customer\n";
	else
		return;

	dprintf(INFO,"set %s\n", buf);

	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
#endif
}


void cp_bootconfig_fixup(uint8_t *ramdisk_addr)
{
	bootconfig_chosen_bootargs_append(ramdisk_addr, bootconfig_get_bootmode());

#ifdef CONFIG_PMIC_CHIP_ID
	bootconfig_chosen_bootargs_append(ramdisk_addr, bootconfig_get_chipid());
#endif
}

void set_bootconfig_len(uint64_t size)
{
	bootconfig_size = size;
}

uint64_t get_bootconfig_len(void)
{
	return bootconfig_size;
}

void print_bootconfig_param(uint8_t *ramdisk_addr)
{
        int bootcfg_size = get_bootconfig_len();
        char *bootcfg_param = malloc(bootcfg_size);

        if(bootcfg_param == NULL) {
        	errorf("malloc bootcfg_param fail!\n");
	} else {
		memcpy(bootcfg_param, ramdisk_addr, bootcfg_size);
        	dprintf(CRITICAL, "bootconfig = %s\n", bootcfg_param);
		free(bootcfg_param);
		bootcfg_param = NULL;
	}
}

// Add by changmei.chen for hw-anti-rollback version 20241211 begin
extern int fb_oem_booargs_get_eps_state(char *buf, int len);
void bootconfig_fixup_eps_state(uint8_t *ramdisk_addr)
{
    char buf[32] = {0};

    if (fb_oem_booargs_get_eps_state(buf, sizeof(buf)) <= 0) {
        return;
    }
    bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
}
// Add by changmei.chen for hw-anti-rollback version 20241211 end

//#if defined(CONFIG_CALI_MODE)
extern int cali_verify_and_set_flag(void);
//#endif
extern int nsrp_efuse_read(void);
#define BUF_SIZE (MISCDATA_SKU_ID_DATA_LEN+MISCDATA_WALLPAPER_ID_DATA_LEN+MISCDATA_TA_CODE_DATA_LEN+MISCDATA_GUID_DATA_LEN)
extern char s_mask_serial_num[65];
extern int get_lcs(uint32_t *p_lcs);

void bootconfig_fixup_hmd_cmdline(uint8_t *ramdisk_addr)
{
	char buf_t[128], buf_p[128] = {0};
//Modify by wangkang for NYX-2554 Make the timestamp of ro.boot.factoryresettime into milliseconds
	char buf[64] = {0};
	int boardid = 0;
	int pcb_version = 0;
	int efuse_val_read = 0;
	int str_len = 0;
//	#if defined(CONFIG_CALI_MODE)
	int verify_cali_data = 0;
//	#endif

	memset(buf_p, 0, 128);
	memset(buf_t, 0, 128);
	if (common_raw_read("miscdata", (uint64_t)MISCDATA_SKU_ID_DATA_LEN, MISCDATA_SKU_ID_BASE, buf_p)){	    
		errorf("read hmd sku id data error from miscdata...\n");
	}

	#ifdef CONFIG_PHYS_64BIT
	if ((strcmp(buf_p, "100WW") != 0)&&(strcmp(buf_p, "100EEA") != 0)&&(strcmp(buf_p, "100M0") != 0)&&(strcmp(buf_p, "1HMWW") != 0)&&(strcmp(buf_p, "1NPWW") != 0)&&(strcmp(buf_p, "1NPM0") != 0)&&(strcmp(buf_p, "1NPEEA") != 0)) {
	errorf("wrong skuid,using 100WW...\n");
	strcpy(buf_p,"100WW");
	}
	#else
	if ((strcmp(buf_p, "100WW") != 0)&&(strcmp(buf_p, "100EEA") != 0)&&(strcmp(buf_p, "100ZA") != 0)) {
	errorf("wrong skuid,using 100WW...\n");
	strcpy(buf_p,"100WW");
	}
	#endif

	snprintf(buf, ARRAY_SIZE(buf), "androidboot.skuid=%s\n", buf_p);
    bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	memset(buf, 0, 40);
	snprintf(buf, ARRAY_SIZE(buf), "androidboot.product.vendor.sku=%s\n", buf_p);
    bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	memset(buf, 0, 40);
	memset(buf_p, 0, 128);




	if (common_raw_read("miscdata", (uint64_t)MISCDATA_WALLPAPER_ID_DATA_LEN, MISCDATA_WALLPAPER_ID_BASE, buf_p)){	    
		errorf("read hmd wallpaper data error from miscdata...\n");
	}
	 sprintf(buf_t, "%d",buf_p[0]);
	snprintf(buf, ARRAY_SIZE(buf), "androidboot.wallpaper=%s\n", buf_t);
    bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	memset(buf, 0, 40);
	memset(buf_t, 0, 128);
	memset(buf_p, 0, 128);
	
	boardid = sprd_get_bandinfo();
	if(boardid==4){
		strcpy(buf_p,"TA-1697");
	}else if(boardid==3){
		strcpy(buf_p,"TA-1682");
	}else if(boardid==2){
		strcpy(buf_p,"TA-1685");
	}else{
		strcpy(buf_p,"unknown");
	}
	
	snprintf(buf, ARRAY_SIZE(buf), "androidboot.ta.code=%s\n", buf_p);
    bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	memset(buf, 0, 40);
	memset(buf_p, 0, 128);


	if (common_raw_read("miscdata", (uint64_t)MISCDATA_HEF_FLAG_DATA_LEN, MISCDATA_HEF_FLAG_BASE, buf_p)){	    
		errorf("read hmd hef data error from miscdata...\n");
	}
	if(buf_p[0]==0)
	{
		sprintf(buf_t, "%s","0");	
	}else{
		sprintf(buf_t, "%s", buf_p);
	}
	snprintf(buf, ARRAY_SIZE(buf), "androidboot.hef=%s\n", buf_t);
    bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	memset(buf_t, 0, 128);
	memset(buf_p, 0, 128);
	memset(buf, 0, 40);
	


	if ((NULL != g_env_bootmode) && (strcmp(g_env_bootmode, "cali") != 0)) {
		if (common_raw_read("miscdata", (uint64_t)MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_DATA_LEN, MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_BASE, buf_p)){	    
			errorf("read hmd factory reset time data error from miscdata...\n");
		//	return -1;
		}
		snprintf(buf, ARRAY_SIZE(buf), "androidboot.factoryresettime=%s\n", buf_p);
		str_len = strlen(buf);


		if(buf_p[0]==0)
		{
			memset(buf, 0, 40);
			sprintf(buf, "androidboot.factoryresettime=0\n");
		}
		else
		{
			buf[str_len] = '\n';
		}
   		bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
		memset(buf_p, 0, 128);
		memset(buf, 0, 40);
	}


	pcb_version = sprd_get_pcb_version();
	if(pcb_version==1){
		strcpy(buf_p,"V0.1");
	}
	else if(pcb_version==2){
		strcpy(buf_p,"V0.2");
	}
	else if(pcb_version==3){
		strcpy(buf_p,"V0.3");
	}
	else if(pcb_version==4){
		strcpy(buf_p,"V0.4");
	}


	snprintf(buf, ARRAY_SIZE(buf), "androidboot.pcbversion=%s\n", buf_p);
   	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	memset(buf_p, 0, 128);
	memset(buf, 0, 40);
	
	efuse_val_read = nsrp_efuse_read();
	sprintf(buf_p, "%d",efuse_val_read);
	snprintf(buf, ARRAY_SIZE(buf), "androidboot.nsrp=%s\n", buf_p);
   	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	//modify by hyinfeng for NYX-275 Unique SoC serial number to ro.boot.uniqueno begin
	memset(buf_t, 0, 128);
	snprintf(buf_t, ARRAY_SIZE(buf_t), "androidboot.uniqueno=%s\n", s_mask_serial_num);
   	bootconfig_chosen_bootargs_append(ramdisk_addr, buf_t);
	//modify by hyinfeng for NYX-275 Unique SoC serial number to ro.boot.uniqueno end

	memset(buf_t, 0, 128);
	verify_cali_data = cali_verify_and_set_flag();
	if(verify_cali_data)
	{
		sprintf(buf_t, "androidboot.diag_lock=2\n");
	}
	else
	#if defined(CONFIG_CALI_MODE)
	{
		sprintf(buf_t, "androidboot.diag_lock=1\n");
	}
	#else
	{
		sprintf(buf_t, "androidboot.diag_lock=0\n");
	}
	#endif
	bootconfig_chosen_bootargs_append(ramdisk_addr, buf_t);
	
	//add by hyinfeng for NYX-3190,NYX-1852 ro.boot.secureboot to get efuse begin
	memset(buf, 0, 40);
	unsigned int t_lcs __attribute__((aligned(4096))) = 0;
	int ret = get_lcs(&t_lcs);//check if device is efused(t_lcs=5 means efused)
	snprintf(buf, ARRAY_SIZE(buf), "androidboot.secureboot=%d\n", 0 == ret && 5 == t_lcs);
   	bootconfig_chosen_bootargs_append(ramdisk_addr, buf);
	//add by hyinfeng for NYX-3190,NYX-1852 ro.boot.secureboot to get efuse end
}

