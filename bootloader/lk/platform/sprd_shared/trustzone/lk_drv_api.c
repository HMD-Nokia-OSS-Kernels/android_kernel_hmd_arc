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

#include "lk_sec_drv.h"
#include "tee_smc_call.h"
//#include <otp_helper.h>
#include <boot_mode.h>
#include <kernel/debug.h>
#include <platform.h>
#include <stdio.h>
#include <string.h>
#include <secureboot/sec_common.h>
#ifdef CONFIG_ANDROID_AB
#include <asm/arch/check_reboot.h>
#endif
#include <sprd_common.h>

/**
NOTICE, from now on, in order to satisfy all current TEEs,
we use smc32 call from uboot to tos, this is also the way for generic
TEE driver, there are two ways if you want to pass 64 bits addr to tos:
1. put 64 bits addr to your own structure, then pass structure's addr
2. seperate 64 bits addr to two 4 bytes value and put them into param,
   then call tee_smc_call
*/

int uboot_verify_img(unsigned long start_addr, uint32_t lenth)
{

  smc_param *param = tee_common_call(FUNCTYPE_VERIFY_IMG, (uint32_t)start_addr, lenth);
  if (param->a0!=NO_ERROR) {//SM_ERR_PANIC may be returned due to tos panic
  	errorf("uboot_verify_img() return error:param->a0=%d\n",param->a0);
	// while (1);
  }
  return param->a0;
}

int uboot_vboot_verify_img(unsigned long start_addr, uint32_t lenth)
{
  VbootVerifyInfo *vboot_verify_info = NULL;
  vboot_verify_info = (VbootVerifyInfo *)start_addr;
  smc_param *param = tee_common_call(FUNCTYPE_VBOOT_VERIFY_IMG, (uint32_t)start_addr, lenth);

  if (vboot_verify_info->verify_return_data->vboot_verify_ret != param->a0) {//SM_ERR_PANIC may be returned due to tos panic
    dprintf(INFO,"uboot_vboot_verify_img() return error:param->a0=%u\n",param->a0);
    dprintf(INFO,"uboot_vboot_verify_img() verify image name=%s\n",(uint8_t *)vboot_verify_info->img_name);
    dprintf(INFO,"uboot_vboot_verify_img() verify ret=%u\n",vboot_verify_info->verify_return_data->vboot_verify_ret);
    panic("uboot_vboot_verify_img failed!\n");
  }
  return param->a0;
}

int uboot_verify_product_sn_signature(unsigned long start_addr, uint32_t lenth){
	smc_param *param = tee_common_call(FUNCTYPE_VERIFY_PRODUCT_SN_SIGNATURE, (uint32_t)start_addr, lenth);
	if (param->a0 != NO_ERROR) {
		dprintf(INFO,"uboot_verify_product_sn_signature() return error:param->a0=%d\n",param->a0);
		panic("uboot_verify_product_sn_signature error!\n");
	}
	return param->a0;
}

int uboot_config_os_version(unsigned long start_addr, uint32_t lenth){
	smc_param *param = tee_common_call(FUNCTYPE_CONFIG_OS_VERSION, (uint32_t)start_addr, lenth);
	if (param->a0 != NO_ERROR) {
		dprintf(INFO,"uboot_config_os_version() return error:param->a0=%d\n",param->a0);
		panic("uboot_config_os_version error!\n");
	}
	return param->a0;
}

int uboot_set_root_of_trust(unsigned long start_addr, uint32_t lenth){
	smc_param *param = tee_common_call(FUNCTYPE_SET_ROOT_OF_TRUST, (uint32_t)start_addr, lenth);
	if (param->a0 != NO_ERROR) {
		dprintf(INFO,"uboot_set_root_of_trust() return error:param->a0=%d\n",param->a0);
		panic("uboot_set_root_of_trust error!\n");
	}
	return param->a0;
}

int uboot_get_tos_random(unsigned long start_addr, uint32_t lenth){
	if (((start_addr & 0xFFF) != 0) || lenth > 4096) {
		dprintf(INFO,"please guaranted addr (%lx) is align(4096) "
				"and lenth (%d) is less than 4096! \n", start_addr, lenth);
		fastboot_mode();
	}

	smc_param *param = tee_common_call(FUNCTYPE_GET_TOS_RANDOM, (uint32_t)start_addr, lenth);
	if (param->a0 != NO_ERROR) {
		errorf("uboot_get tos random() return error:param->a0=%d\n",param->a0);
		fastboot_mode();
	}
	return param->a0;
}

int uboot_encrypt_data(uint64_t start_addr, uint64_t lenth){
  smc_param *param = tee_common_call(FUNCTYPE_CRYPTO_DATA, (uint32_t)start_addr, (uint32_t)lenth);
  return param->a0;
}

int uboot_verify_pwd(uint64_t start_addr, uint64_t lenth){
  smc_param *param = tee_common_call(FUNCTYPE_CHECK_PWD, (uint32_t)start_addr, (uint32_t)lenth);
  return param->a0;
}

int uboot_verify_lockstatus(uint64_t start_addr, uint64_t lenth){
  smc_param *param = tee_common_call(FUNCTYPE_CHECK_LOCK_STATUS, (uint32_t)start_addr, (uint32_t)lenth);
  return param->a0;
}

int uboot_vboot_set_ver(unsigned long start_addr, uint32_t lenth)
{
  smc_param *param = tee_common_call(FUNCTYPE_VBOOT_SET_VERSION, (uint32_t)start_addr, lenth);
  return param->a0;
}

static void disable_debug_control_signal(void)
{
#if defined (SPRD_SECBOOT)
	//sprd_ap_efuse_prog(BLOCK_NUM_3,DISABLE_SPRD_DEBUG_CONTROL);
#endif
#if defined (CONFIG_SANSA_SECBOOT)
    sprd_ap_efuse_prog(BLOCK_NUM_3,DISABLE_DX_DEBUG_CONTROL);
#endif
}

int uboot_program_efuse(uint64_t start_addr, uint64_t lenth){

  disable_debug_control_signal();

//  ap_sansa_efuse_prog_power_on();

  smc_param *param = tee_common_call(FUNCTYPE_PROGRAM_EFUSE, (uint32_t)start_addr, (uint32_t)lenth);

//  ap_sansa_efuse_power_off();
  dprintf(INFO,"pro_ret=%x\n",param->a0);
  return param->a0;
}

int uboot_get_socid(uint64_t start_addr, uint64_t lenth){

 // ap_sansa_efuse_prog_power_on();

  smc_param *param = tee_common_call(FUNCTYPE_GET_SOCID, (uint32_t)start_addr, (uint32_t)lenth);

//  ap_sansa_efuse_power_off();
  return param->a0;
}

int uboot_update_swVersion(uint64_t start_addr, uint64_t lenth){

  smc_param *param = tee_common_call(FUNCTYPE_UPDATE_VERSION, (uint32_t)start_addr, (uint32_t)lenth);
  return param->a0;
}

int uboot_sansa_get_lcs(uint32_t *p_lcs){

//   ap_sansa_efuse_prog_power_on();

   smc_param *param = tee_common_call(FUNCTYPE_GET_SANSA_LCS, 0, 0);
   *p_lcs = param->a2;

 //  ap_sansa_efuse_power_off();
   dprintf(INFO,"lcsval=%d\n",param->a2);
   return param->a0;
}

int uboot_sansa_set_rma(uint32_t *p_rmaret){

//   ap_sansa_efuse_prog_power_on();

   smc_param *param = tee_common_call(FUNCTYPE_SET_SANSA_RMA, 0, 0);

   *p_rmaret = param->a2;

//   ap_sansa_efuse_power_off();
   dprintf(INFO,"setrma=%d\n",param->a2);
   return param->a0;
}

int uboot_kce_program(uint64_t start_addr, uint64_t lenth){

//  ap_sansa_efuse_prog_power_on();

  smc_param *param = tee_common_call(FUNCTYPE_KCE_PROGRAM, (uint32_t)start_addr, (uint32_t)lenth);

//  ap_sansa_efuse_power_off();
  dprintf(INFO,"kce_pro=%x\n",param->a0);
  return param->a0;
}

int uboot_write_hbk(uint64_t start_addr, uint64_t lenth){
  smc_param *param = tee_common_call(FUNCTYPE_WR_ROTPK, (uint32_t)start_addr, (uint32_t)lenth);
  return param->a0;
}

int uboot_get_hbk(uint64_t start_addr, uint64_t lenth){
  smc_param *param = tee_common_call(FUNCTYPE_GET_HBK, (uint32_t)start_addr, (uint32_t)lenth);
  return param->a0;
}

int check_kce_gid(void){
	smc_param *param = tee_common_call(FUNCTYPE_CHECK_KCE_GID, 0, 0);
	return param->a0;
}

int check_gpt_efuse(void){
	smc_param *param = tee_common_call(FUNCTYPE_CHECK_GPT_EFUSE, 0, 0);
	return param->a0;
}

int check_kce_status(void){
	smc_param *param = tee_common_call(FUNCTYPE_CHECK_KCE_STATUS, 0, 0);
	return param->a0;
}
/*add fastboot cmd for sharkl2*/
#ifdef SPRD_SECBOOT
int get_lcs(uint32_t *p_lcs){
	smc_param *param = tee_common_call(FUNCTYPE_GET_LCS, p_lcs, sizeof(uint32_t));
	return param->a0;
}

int get_sec_efuse_ver(uint32_t *ver){
	smc_param *param = tee_common_call(FUNCTYPE_GET_EFUSE_VER, ver, sizeof(uint32_t));
	return param->a0;
}

int get_socid(uint64_t start_addr, uint64_t lenth){
	smc_param *param = tee_common_call(FUNCTYPE_GET_SOCID, (uint32_t)start_addr, (uint32_t)lenth);
	return param->a0;
}

int set_rma(void){
	smc_param *param = tee_common_call(FUNCTYPE_SET_RMA, 0, 0);
	return param->a0;
}

int get_secdebug_bit(uint32_t *secdebug_bit) {
	smc_param *param = tee_common_call(FUNCTYPE_GET_SECDEBUG_BIT, (uint32_t)secdebug_bit, sizeof(uint32_t));
	return param->a0;
}

int get_rotpk0(uint64_t start_addr, uint64_t lenth) {
	smc_param *param = tee_common_call(FUNCTYPE_GET_ROTPK0, (uint32_t)start_addr, (uint32_t)lenth);
	return param->a0;
}

int get_rotpk1(uint64_t start_addr, uint64_t lenth) {
	smc_param *param = tee_common_call(FUNCTYPE_GET_ROTPK1, (uint32_t)start_addr, (uint32_t)lenth);
	return param->a0;
}

int get_secure_version(secure_version_info *ver_info){
	smc_param *param = tee_common_call(FUNCTYPE_GET_SECURE_EFUSE_VERSION, (uint32_t)ver_info, sizeof(secure_version_info));
	return param->a0;
}
#endif

