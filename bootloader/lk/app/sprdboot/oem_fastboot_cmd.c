//#include <common.h>
#include <malloc.h>
#include <android_bootimg.h>
#include <boot_mode.h>
#include <dl_common.h>
#include "sparse_format.h"
#include <linux/usb/usb_uboot.h>
#include <sprd_common_rw.h>
#include <secureboot/sec_common.h>
#include "miscdata_def.h"
#include "sprd_cpcmdline.h"
#include "oem_fastboot_cmd.h"

//ZOVERLAY_TAG_HMD_ONEIMAGE
#ifdef CONFIG_HMD_FASTBOOT

typedef void (*fastboot_cmd_fn)(const char *, void *, unsigned);
struct fastboot_cmd_desc {
	char * name;
	fastboot_cmd_fn cb;
};


#define MAX_RSP_SIZE            128
#define HMD_PROJECT_NAME        "NYX"
#define HMD_PRODUCTION_MODEL	"HMD ARC"
#define HMD_PRODUCTION_MODEL_MKOPA	"M-KOPA M10"
#define HMD_PRODUCTION_MODEL_NYXPLUS	"HMD AURA2"

extern int hmd_rsa_encrypt_data(unsigned char *send_data);
extern int hmd_rsa_decrypt_data(unsigned char *data_type, unsigned char *revice_data, unsigned char *sn_data, int fdl);
extern int oem_repair_read_mmc_ex(const char *type,unsigned char *buf,int len);
extern int oem_repair_write_mmc_ex(const char *type,unsigned char *buf);
extern void fastboot_enter_cali_mode(u8 cail_mode, u8 cail_freq);
extern void calibration_mode(void);
extern int sprd_get_imgversion(int imgType, unsigned int* swVersion);
extern int get_lcs(uint32_t *p_lcs);
extern unsigned int get_lock_status(void);
extern void fastboot_response_info(const char *info);
extern void fb_cmd_upload(const char *arg, void *data, uint64_t sz);
extern void fastboot_okay(const char *info);
extern void fastboot_fail(const char *reason);
extern void lcd_clear(void);
extern int lcd_splash(char *logo_part_name);
extern int set_product_sn(char *psn, int len);
extern char *get_product_sn(void);
extern void fastboot_publish(const char *name, const char *value);
extern void fastboot_register(const char *prefix, void (*handle) (const char *arg, void *data, uint64_t sz));
extern void fb_cmd_oem_permission(const char *arg, void *data, uint64_t sz);
extern void fb_cmd_reboot_edl(const char *arg, void *data, uint64_t sz);
extern int get_imei_from_nv(char* imei1, char* imei2);

int rsa_encrypt_data_flag = RSA_ENCRYPT_HMD;
char upload_buf[UPLOADE_SIZE] = {0};
unsigned int upload_len = 0;
int fastboot_mode_flag = MODE_FASTBOOT_BASIC;
char powp_buf[MISCDATA_POWP_DATA_LEN] = {0};
char cali_buf[MISCDATA_CALI_DATA_LEN] = {0}; //modify by ysong for cali fastboot begin
int POWP_flag;	//1=enable protect data   0=disable  pubkey write,can modify data
int powp_size = 0;

#if  1//def REFERENCE/*REF*/
	char sw_pub_key[SW_PUB_KEY_LEN+1] =
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0huFu7hxa22/hy4M99Lv\
R4IsT99PxOHwpv6sMxenai43mS2WT+UNXcLsXB/DZYtOWRJ2kuGB9c4pUg9pTedI\
7OX+i3+AXlrIGGxcIvaCRIDQ3r+CaeqZFy5iS5UlY6fRl0X/L6zAW4c5KzHo6sfQ\
/PvK7FvuWPlRIE87W1TbTWW+NItP0om8C7Hz0jcggsjvKSZ/v7yBpgrrH5evfJxX\
Erk9ZbabKc7YI66eiCN0DQHWs8NXIM/ZEbC2EeqoUP/M9eFFAOznkknQtdDg3KUq\
6CgdGatiBto5wxSWJ+y4U+8RQTsP1ad5J34u7SMNmLVOFCxvegwRTFV5ibmz5/Wx\
YQIDAQAB";
#else ifdef REFERENCE_PLUS/*REP*/
	char sw_pub_key[SW_PUB_KEY_LEN+1] =
"MIIBIjANBgkqhkiG9w0BAQEAABBSOCAQ8AMIIBgKCAQEAyBNeW2UiDLDMIhLv6vw1\
L+JVP6oo3vmOPwFVO/KtU9IgjZE7FLP36GtD3Amhn8bx5FY7UiQUgZMBjUx9hv9S\
sYVQIWbxxZNclWQ9uT+G2SmVnIvkdHPNoasg2153WyFUpEt7t0tvF+KrTfplAJQ7\
+6+EFzQHQsnkbBc0MM2h4OqxD/B8H8tpbowSNhX5wkhnQ5RDk30oM7OFDjeZuxgQ\
hDd0wripM7bmmCzJ/thRbBwO1g9TMH/ias265vppubMmx/xKOsb5BEj+POI3lZlG\
iWEaMwQZl/UNQteIBNk3/d26y3J2yIVBemOtMujblmAasGwbTRRtcJzV767AuUMO\
mwIDAQAB";
#endif

int auth_flag=0;  //1=enable   0=disable
char *basic_cmd_disallow[] = {"reboot-emergency", "oem reboot-edl", "oem repair", "oem enter_calibration", "oem zeroflag","oem powp_enable","oem hef"};
char *flash_cmd_disallow[] = {"reboot-emergency", "oem reboot-edl", "oem repair", "oem enter_calibration", "oem zeroflag","oem powp_enable","oem hef"};
char *repair_cmd_disallow[] = { };
char *simlock_cmd_disallow[] = {"reboot-emergency", "oem reboot-edl", "oem repair"};
char *factory_cmd_disallow[] = {"reboot-emergency"};

char *factory_allow_erase_arg[] = {"userdata", "cache", "metadata"};
char *flash_basic_allow_arg[] = {"unlock"};
char *flash_disallow_arg[] = {"frp"};

struct mode_cmd_check_table check_tbl[MODE_FASTBOOT_NUM] = {
	{ basic_cmd_disallow, sizeof(basic_cmd_disallow) / sizeof(basic_cmd_disallow[0]) },
	{ flash_cmd_disallow, sizeof(flash_cmd_disallow) / sizeof(flash_cmd_disallow[0]) },
	{ repair_cmd_disallow, sizeof(repair_cmd_disallow) / sizeof(repair_cmd_disallow[0]) },
	{ simlock_cmd_disallow, sizeof(simlock_cmd_disallow) / sizeof(simlock_cmd_disallow[0]) },
	{ factory_cmd_disallow, sizeof(factory_cmd_disallow)/ sizeof(factory_cmd_disallow[0])},
};

const unsigned char *next_arg(char *next/*out param*/, const unsigned char *buff)
{
	int i, idx_start, idx_end;
	if(NULL == buff)
		return NULL;
	i = 0;
	while (buff[i] == ' ')
		i++;
	idx_start = i;

	while (buff[i] != ' ' && buff[i] != '\0')
		i++;
	idx_end = i;

	for (i = 0; i < idx_end - idx_start; ++i)
		next[i] = buff[idx_start + i];
	next[i] = '\0';
    return buff + idx_start + i;
}

int fastboot_mode_cmd_filter(const char *cmd_prefix, const unsigned char *buff)
{
	int i, cnt;
	char next[24] = {0};
	char cmd[30] = {0};
	int is_flash_erase = 0;
	int cmd_disallow = 0;
	
	next_arg(next, buff);
    /* every mode disallow several cmds; so simply disallow in respective mode */
	if (check_tbl[fastboot_mode_flag].cmd_disallow != NULL) {
		if(!strcmp(cmd_prefix, "oem")) {
			snprintf(cmd, 30, "%s %s", cmd_prefix, next);
		} else {
			snprintf(cmd, 30, "%s", cmd_prefix);
		}
		
		for (i = 0; i < check_tbl[fastboot_mode_flag].cmd_disallows; i++) {
			if (!strcmp(cmd, check_tbl[fastboot_mode_flag].cmd_disallow[i])) {
				cmd_disallow = 1;
				break;
			}
		}
	}

    /*
	 * if command is flash or erase, get in trouble here. due to HMD's requirements,
	 * in basic mode, we should allow flash/erase userdata and cache parition
	 * and forbid any other paritions.
	 * in flash/simlock mode, we should allow flash/erase all partition except
	 * config parition(FRP flag) and unlock.
	 */
	if (!strcmp(cmd_prefix, "flash") || !strcmp(cmd_prefix, "erase"))
		is_flash_erase = 1;

	if(is_flash_erase) {
		cnt = sizeof(flash_basic_allow_arg) / sizeof(flash_basic_allow_arg[0]);
		if (fastboot_mode_flag == MODE_FASTBOOT_BASIC) {
			cmd_disallow = 1;
			for (i = 0; i < cnt; i++) {
				if (!strcmp(next, flash_basic_allow_arg[i])) {
					cmd_disallow = 0;
					break;
				}
			}
		}

		cnt = sizeof(flash_disallow_arg) / sizeof(flash_disallow_arg[0]);
		if ((fastboot_mode_flag == MODE_FASTBOOT_FLASH || fastboot_mode_flag == MODE_FASTBOOT_SIMLOCK)) {
			for (i = 0; i < cnt; i++) {
				if (!strcmp(next, flash_disallow_arg[i])) {
					cmd_disallow = 1;
					break;
				}
			}
		}
		if(fastboot_mode_flag == MODE_FASTBOOT_FACTORY){
			cmd_disallow = 1;
			cnt = sizeof(factory_allow_erase_arg) / sizeof(factory_allow_erase_arg[0]);

			for (i = 0; i < cnt; i++) {
				if (!strcmp(next, factory_allow_erase_arg[i])) {
					cmd_disallow = 0;
					break;
				}
			}
		}
    }
	 
	if (is_flash_erase && get_lock_status() == VBOOT_STATUS_UNLOCK)
	{	
		debugf("fastboot unlock status can flash partitions for flash in fastboot\n");
		cmd_disallow = 0;
	}
	if (cmd_disallow) {
		return -1;
	} else {
		return 0;
	}
}
#if 0
void cmd_oem_getsecurityversion(const char *arg, void *data, unsigned size) {
        //now it is 101-version for adapting HMD flash tool
        fastboot_response_info("112");
        fastboot_okay("");
}
#endif
void cmd_oem_getdllname(const char *arg, void *data, unsigned size) {
	/*the value of return refer to our tool lib*/
    fastboot_response_info("dllname=hmdLibrary_wtwd_nyx:oem enter_calibration");
	fastboot_okay("");
}

void cmd_oem_auth_start(const char *arg, void *data, unsigned size) {
	int ret = -1;

	rsa_encrypt_data_flag = RSA_ENCRYPT_HMD;

	memset(upload_buf,0,sizeof(upload_buf));
	ret = hmd_rsa_encrypt_data(upload_buf);
	if(ret == 0){
		upload_len = AUTH_DATA_LEN;
		fastboot_okay("");
	}else
		fastboot_fail("rsa encrypt failed");

	return;
}

void cmd_oem_factory_auth_start(const char *arg, void *data, unsigned size) {

	int ret = -1;

	rsa_encrypt_data_flag = RSA_ENCRYPT_FACTORY;

	memset(upload_buf,0,sizeof(upload_buf));
	ret = hmd_rsa_encrypt_data(upload_buf);
	if(ret == 0){
		upload_len = AUTH_DATA_LEN;
		char info[60] = "";
		int str_len;
		int i=0;
		for(i=0;i<7;i++){
			memset(info, 0x0,sizeof(info));
			memcpy(info, upload_buf+(i*50)*sizeof(char), 50*sizeof(char));			
			str_len = strlen(info);
			info[str_len] = '\0';
			fastboot_response_info(info);
		}

		fastboot_okay("");
	}
	else
		fastboot_fail("factory rsa encrypt failed");

	return;
}

void cmd_oem_enter_calibration(const char *arg, void *data, unsigned size)
{
	/*calibration default para is: 1,0,146*/
	unsigned char cail_mode = 1;
	unsigned char cail_freq = 0;
	char *tok;
	const char *delim = " ";

	tok = strtok(arg, delim);
	if (tok) {
		cail_mode = (unsigned char)simple_strtoul(tok, NULL, 10);
	}

	tok = strtok(NULL, delim);
	if (tok) {
		cail_freq = (unsigned char)simple_strtoul(tok, NULL, 10);
	}

	debugf("cail_mode=%d, cail_freq=%d\n", cail_mode, cail_freq);

	fastboot_enter_cali_mode(cail_mode, cail_freq);

	fastboot_okay("");
	mdelay(5000);//add for PGN-1704 Failed to write SN\IMEI\WIFI\BT\SIMLOCK using DeviceKit
	lcd_clear();
	lcd_splash("logo");
	
	calibration_mode();
}

void fb_printf_devinfo(char *info)
{
	int str_len;
	str_len = strlen(info);
	info[str_len] = '\0';
	fastboot_response_info(info);
}

void cmd_oem_get_devinfo(const char *arg, void *data, uint64_t sz)
{
	int ret;
	//char tempword[20] = "empty";
	char info[256] = {0};
	//char tembuf[128] = {0};
	char skuid[128] = {0};
	char tacode[128] = {0};
	//char simslot_buf[16] = {0};
	//unsigned int simslot_buf_len=0;
	//int hardware_level = 1;//sprd_get_hardwareid_info();
	int isGo64 = 0;
	if(!strcmp(SPRD_BOARD_INFO_ID, "sp9863a 1h10 go")) {
		isGo64 = 1;
	}
	//fastboot_response_info("");
	fastboot_response_info("{");

	//memset(info, 0x0,sizeof(info));
	//memset(skuid, 0x0,sizeof(skuid));
	ret = common_raw_read("miscdata", (uint64_t)MISCDATA_SKU_ID_DATA_LEN, (uint64_t)MISCDATA_SKU_ID_BASE, skuid);
	if (strcmp(skuid, "100ZA") && strcmp(skuid, "100EEA") && strcmp(skuid, "100M0") && strcmp(skuid, "1HMWW") && strcmp(skuid, "1NPWW") && strcmp(skuid, "1NPM0") && strcmp(skuid, "1NPEEA")) {
		strcpy(skuid,"100WW");
	}
	if ((!strcmp(skuid, "100ZA") && isGo64) || ((!strcmp(skuid, "100M0") || !strcmp(skuid, "1HMWW") || !strcmp(skuid, "1NPWW") || !strcmp(skuid, "1NPM0") || !strcmp(skuid, "1NPEEA")) && !isGo64)) {
		strcpy(skuid,"100WW");
	}
	if(0 != ret)
		debugf("%s:%d common raw read return%d\r\n",__func__, __LINE__, ret);
	
	sprintf(info, "\"SKUID\": \"%s\",", skuid);
	fb_printf_devinfo(info);

	memset(info, 0x0,sizeof(info));
	sprintf(info, "\"Project\": \"%s\",", HMD_PROJECT_NAME);
	fb_printf_devinfo(info);
	
	//memset(tacode, 0x0,sizeof(tacode));
	ret = common_raw_read("miscdata", (uint64_t )MISCDATA_TA_CODE_DATA_LEN, (uint64_t)MISCDATA_TA_CODE_BASE, tacode);
	if (strncmp(tacode, "TA-", 3)) {
		strcpy(tacode,"TA-1697");
	}
	if(0 != ret)
		debugf("%s:%d common raw read return%d\r\n",__func__, __LINE__, ret);
	
	memset(info, 0x0,sizeof(info));
	if(!strcmp(skuid, "100WW")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"Nyx_0AWW":"Nyx2G_0AWW"));
	}else if(!strcmp(skuid, "100ZA")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"undefine":"Nyx2G_0AZA"));
	}else if(!strcmp(skuid, "100EEA")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"Nyx_0AEEA":"Nyx2G_0AEEA"));
	}else if(!strcmp(skuid, "100M0")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"Nyx_0AM0":"undefine"));
	}else if(!strcmp(skuid, "1HMWW")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"Nyx_HMWW":"undefine"));
    }else if(!strcmp(skuid, "1NPWW")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"Nyx_0AWW":"undefine"));
    }else if(!strcmp(skuid, "1NPM0")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"Nyx_0AM0":"undefine"));
    }else if(!strcmp(skuid, "1NPEEA")){
		sprintf(info, "\"Product\": \"%s\",", (isGo64?"Nyx_0AEEA":"undefine"));
	}else{
		sprintf(info, "\"Product\": \"%s\",", "undefine");
	}
	fb_printf_devinfo(info);

	memset(info, 0x0,sizeof(info));
	sprintf(info, "\"Version\": \"%s\",", HMD_PRODUCTION_VERSION);
	fb_printf_devinfo(info);
	
	memset(info, 0x0,sizeof(info));
	sprintf(info, "\"ProductTAcode\": \"%s\",", tacode);
	fb_printf_devinfo(info);

	memset(info, 0x0,sizeof(info));
    if (!strcmp(skuid, "1HMWW")) {
        sprintf(info, "\"ProductModel\": \"%s\",", HMD_PRODUCTION_MODEL_MKOPA);
    } else if (!strcmp(skuid, "1NPWW") || !strcmp(skuid, "1NPM0") || !strcmp(skuid, "1NPEEA")) {
		sprintf(info, "\"ProductModel\": \"%s\",", HMD_PRODUCTION_MODEL_NYXPLUS);
	} else {
		sprintf(info, "\"ProductModel\": \"%s\",", HMD_PRODUCTION_MODEL);
    }
	fb_printf_devinfo(info);

	memset(info, 0x0,sizeof(info));
	sprintf(info, "\"AntiRollback_HW\": \"%s\",", ANTIROLLBACK_HW);
	fb_printf_devinfo(info);

	memset(info, 0x0,sizeof(info));
	sprintf(info, "\"AntiRollback_vbmeta\": \"%s\",", ANTIROLLBACK_VBMETA);
	fb_printf_devinfo(info);

	memset(info, 0x0,sizeof(info));
	sprintf(info, "\"AntiRollback_vbmeta_system\": \"%s\",", ANTIROLLBACK_VBMETA_SYSTEM);
	fb_printf_devinfo(info);

	/*
	memset(simslot_buf, 0x0, sizeof(simslot_buf));
	//get_status_simslot(simslot_buf,&simslot_buf_len);
	if (!strncmp(simslot_buf,"single", sizeof("single"))){
		memset(info, 0x0, sizeof(info));
		memset(tembuf, 0x0, sizeof(tembuf));
	//	ret = common_raw_read("protect_data", (uint64_t )XX_IMEI1_LEN, (uint64_t )XX_IMEI1_OFFSET, tembuf);

	//	if(0 != ret)
		//	debugf("%s:%d common raw read return%d\r\n",__func__, __LINE__, ret);
//		sprintf(info, "\"IMEI0\"\: \"%s\",", get_product_imei1());
		sprintf(info, "\"IMEI0\": \"%s\",", tempword);
		str_len = strlen(info);
		info[str_len] = '\0';
		fastboot_response_info(info);
	}
	else if (!strncmp(simslot_buf,"dual", sizeof("dual"))){
		memset(info, 0x0,sizeof(info));
		memset(tembuf, 0x0, sizeof(tembuf));
	//	ret = common_raw_read("protect_data", (uint64_t )XX_IMEI1_LEN, (uint64_t )XX_IMEI1_OFFSET, tembuf);

//		if(0 != ret)
//			debugf("%s:%d common raw read return%d\r\n",__func__, __LINE__, ret);
//		sprintf(info, "\"IMEI0\"\: \"%s\",", get_product_imei1());
		sprintf(info, "\"IMEI0\": \"%s\",", tempword);
		str_len = strlen(info);
		info[str_len] = '\0';
		fastboot_response_info(info);

		memset(info, 0x0,sizeof(info));
		memset(tembuf, 0x0, sizeof(tembuf));
	//	ret = common_raw_read("protect_data", (uint64_t )XX_IMEI2_LEN, (uint64_t )XX_IMEI2_OFFSET, tembuf);

//		if(0 != ret)
//			debugf("%s:%d common raw read return%d\r\n",__func__, __LINE__, ret);
//		sprintf(info, "\"IMEI1\"\: \"%s\",", get_product_imei2());
		sprintf(info, "\"IMEI1\": \"%s\",", tempword);
		str_len = strlen(info);
		info[str_len] = '\0';
		fastboot_response_info(info);
	}
	*/

	char imei1[16] = {0};
	char imei2[16] = {0};
	ret = get_imei_from_nv(imei1, imei2);
	if (ret == 0) {
		memset(info, 0x0,sizeof(info));
		sprintf(info, "\"IMEI\": \"%s,%s\",", imei1, imei2);
		fb_printf_devinfo(info);
		
		//memset(info, 0x0,sizeof(info));
		//sprintf(info, "\"IMEI2\": \"%s\",", );
		//fb_printf_devinfo(info);
	}
	
	memset(info, 0x0,sizeof(info));
	sprintf(info, "\"POWP\": \"%d\",", POWP_flag);	
	fb_printf_devinfo(info);

	memset(info, 0x0,sizeof(info));
#if DEBUG
    sprintf(info, "\"SU\": \"%s\",", "1");
#else
    sprintf(info, "\"SU\": \"%s\",", "0");
#endif
	fb_printf_devinfo(info);

    memset(info, 0x0,sizeof(info));
    sprintf(info, "\"Locked\": \"%d\",", get_lock_status() == VBOOT_STATUS_LOCK);
    fb_printf_devinfo(info);

    memset(info, 0x0,sizeof(info));
    unsigned int t_lcs __attribute__((aligned(4096))) = 0;
	ret = get_lcs(&t_lcs);
    sprintf(info, "\"Secure\": \"%d\",", 0 == ret && 5 == t_lcs);
    fb_printf_devinfo(info);
	
    fastboot_response_info("}");
    fastboot_okay("");
}
void show_oem_repair_usage(void) {
    fastboot_response_info("Usages:fastboot oem repair [psn|wallpapered|skuid|hef|simslot|battery] [set|get]");
    fastboot_response_info("Example:fastboot oem repair wallpapered set 0x1");
    fastboot_response_info("Example:fastboot oem repair wallpapered get");
    fastboot_fail("error command");
}

void show_oem_repair_response(const char *type,unsigned char *value)
{
	char response[MAX_RSP_SIZE] = {};
    snprintf(response, sizeof(response), "%s=%s", type, value);
    fastboot_response_info(response);
}

int oem_repair_handle(const char *op,const char *type,unsigned char *value)
{
	int ret = -1;
	int hardware_level = 1;//sprd_get_hardwareid_info();
	debugf("oem_repair_handle %d\n", hardware_level);
	if(!strcmp(op, "set")){
		if(!strcmp(type, "skuid") || !strcmp(type, "sku") || !strcmp(type, "tacode") || !strcmp(type, "battery") ||
		!strcmp(type, "colorid") || !strcmp(type, "wallpapered") || !strcmp(type, "hef")|| !strcmp(type, "cali")) {
			if(hardware_level<=1){
				ret = oem_repair_write_mmc_ex(type,value);
			}else{
				//ret = write_protect_data_ex(type,value);
			}
		}else if(!strcmp(type, "psn")){
			debugf("oem_repair_handle set psn [%s] [%d]\n", value,strlen(value));
			ret = set_product_sn(value, !value ? 0 : strlen(value));
		}else{		
			//ret = write_protect_data_ex(type,value);
		}
	}else if(!strcmp(op, "get")){
		if(!strcmp(type, "skuid") || !strcmp(type, "sku") || !strcmp(type, "tacode") || !strcmp(type, "battery") ||
		!strcmp(type, "colorid") || !strcmp(type, "wallpapered") || !strcmp(type, "hef")|| !strcmp(type, "cali")) {
			if(hardware_level<=1){
				ret = oem_repair_read_mmc_ex(type,value,0);
			}else{
				//ret = read_protect_data_ex(type,value,READ_Protect_data_LEN_AUTO);
			}
		}else if(!strcmp(type, "psn")){
			strcpy(value, get_product_sn());
			debugf("oem_repair_handle get psn =%s,len=%d\n", value,strlen(value));
			ret=0;
		}else{
			//ret = read_protect_data_ex(type,value,READ_Protect_data_LEN_AUTO);
		}
	}
	return ret;
}

void cmd_oem_repair(const char *arg, void *data, uint64_t size) {
    debugf("oem reapir [%s]\n", arg);
    if(!arg) {
        show_oem_repair_usage();
        return;
    }
    int len = strlen(arg);
    char args[len + 1];
    char *delim = " ";
    char *sp = NULL;
    char *type = NULL;
    char *op = NULL;
    char value_buf[64] = {0};
    char *value = value_buf;
    strncpy(args, arg, len);
    args[len] = '\0';  // don't remove this or will case error
    debugf("args [%s]\n", args);
    type = strtok_r(args, delim, &sp);
    if (!type){
        show_oem_repair_usage();
        return;
    }
    debugf("oem repair type is %s\n", type);
    op = strtok_r(NULL, delim, &sp);
    if (!op){
        show_oem_repair_usage();
        return;
    }
    debugf("oem repair operation is %s\n", op);
    if(!strcmp(op, "set")){
        value = strtok_r(NULL, delim, &sp);
        if (value == NULL) {
            show_oem_repair_usage();
            return;
        }
        debugf("oem repair set value is [%s]\n", value);
        show_oem_repair_response(type, value);

        if(!oem_repair_handle(op,type,value)) {
            fastboot_okay("");
        }else{
            fastboot_fail("oem repair set failed");
        }
    }else if(!strcmp(op, "get")){
        if(!oem_repair_handle(op,type,value)) {
            show_oem_repair_response(type,value);
            fastboot_okay("");
        }else{
            fastboot_fail("oem repair get failed");
        }
    } else {
        debugf("oem repair unsupport command");
        fastboot_fail("unsupport command");
    }
}

extern int atoi(const char *src);
void cmd_oem_hef(const char *arg, void *data, unsigned size) {
    int len = strlen(arg);
    char args[len + 1];
    char *delim = " ";
    char *sp = NULL;
    char *op = NULL;
	char read_buf[32];
	memset(read_buf, 0, 32);
    strncpy(args, arg, len);
    args[len] = '\0';  // don't remove this or will cause error
    op = strtok_r(args, delim, &sp);

    if(!strcmp(op, "get")){
		//read_protect_data_ex("hef", read_buf, HEF_LEN);
		oem_repair_read_mmc_ex("hef",read_buf,0);
		char info_buf[32];
		memset(info_buf, 0, sizeof(info_buf));
		sprintf(info_buf ,"get hef=%s",read_buf);
		//fastboot_response_info("get hef=");
		fastboot_response_info(info_buf);
		fastboot_okay("");
    }
	else {
		int op_int=atoi(op);
		debugf("oem hef = %dn",op_int);
		if(op_int>=0 && op_int <=65535){
			if(!oem_repair_write_mmc_ex("hef",op)) {
				fastboot_response_info("set success to hef");
				fastboot_okay("");
			}else{
				fastboot_fail("oem hef set failed");
			}
		}else{
        	debugf("oem hef unsupport command\n");
        	fastboot_fail("unsupport command");
		}
    }
}
#if 0
static int equalsymbols(char *s, int len)
{
    int cnt = 0;

    while (s[--len] == '=' && len > 0)
        cnt++;

    return cnt;
}
#endif
static void cmd_oem_permission(const char *arg, void *data, uint64_t size)
{
	fb_cmd_oem_permission(arg,data,size);

	if(MODE_FASTBOOT_FACTORY == fastboot_mode_flag || MODE_FASTBOOT_REPAIR == fastboot_mode_flag)
	{
		auth_flag = 1;
	}

	fastboot_okay("");
}

void cmd_oem_zeroflag(const char *arg, void *data, unsigned size) {
	char subcmd[64];
	const char *delim = " ";
	int ret = -1;

	memset(subcmd, 0, sizeof(subcmd));
	strcpy(subcmd, strtok(arg, delim));

	if(!strcmp(subcmd, "set")){
	//	ret = set_zeroflag(0x55);
		fastboot_response_info("set success");
	}else if(!strcmp(subcmd, "clr")){
	//	ret = set_zeroflag(0x22);
		fastboot_response_info("clr success");
	}

	fastboot_okay("");
}

#if 0
void cmd_get_zeroflag(const char *arg, void *data, unsigned size) {
	unsigned char zeroflag = 0;
	char test[10] = {};
	int ret;

	ret = get_zeroflag(&zeroflag);

	sprintf(test,"0x%x",zeroflag);
	fastboot_response_info(test);
	fastboot_okay("");
}
#endif

void cmd_oem_sw_pub_key(const char *arg, void *data, unsigned size)
{
	int i;

	upload_len = SW_PUB_KEY_LEN;
	memset(upload_buf,0,sizeof(upload_buf));

	for(i = 0; i < upload_len; i++)
	      upload_buf[i] = sw_pub_key[i];

	fastboot_okay("");
}

void cmd_oem_getpermissions(const char *arg, void *data, unsigned size) {
	switch (fastboot_mode_flag) {
		case MODE_FASTBOOT_BASIC:
			fastboot_response_info("permission=None");
			break;
		case MODE_FASTBOOT_FLASH:
			fastboot_response_info("permission=flash");
			break;
		case MODE_FASTBOOT_REPAIR:
			fastboot_response_info("permission=flash | repair");
			break;
		case MODE_FASTBOOT_SIMLOCK:
			fastboot_response_info("permission=flash | simlock");
			break;
		case MODE_FASTBOOT_FACTORY:
			fastboot_response_info("permission=factory");
			break;
		default:
			fastboot_fail("mode error!!!");
			return;
	}
	fastboot_okay("");
}

#ifdef XX_OEM_TEST
void cmd_oem_powp_xx_enable(const char *arg, void *data, unsigned size)
{
	int ret = -1;
	char powp_enable[MISCDATA_POWP_DATA_LEN] = {0};

	ret = oem_repair_write_mmc_ex("powp",powp_enable);
	if(ret){
		fastboot_fail("clear powp bin fail");
	}

	fastboot_okay("");
}
#endif
#ifdef CONFIG_EMMC_WP
extern ulong mmc_set_pwr_wp(int dev_num, lbaint_t start, int grp_cnt);
#endif
#define EMMC  0
void cmd_oem_powp_enable(const char *arg, void *data, unsigned size)
{
	int ret = -1;
	char powp_enable[MISCDATA_POWP_DATA_LEN] = {0};

	ret = oem_repair_write_mmc_ex("powp",powp_enable);
	if(ret){
		fastboot_fail("clear powp bin fail");
	}else{
		#ifdef CONFIG_EMMC_WP
		ret = mmc_set_pwr_wp(EMMC,0x00024000,1);  //1 powp enable,protect data
		#endif
		if (ret) {
			fastboot_fail("write powpflag fail");
        }else{
			fastboot_okay("");
		}
		//udelay(500);
		usb_driver_exit();
		/* the last time to write log before reboot */
		//write_log_last();
		reboot_devices(CMD_FASTBOOT_MODE);
	}
}

extern int powp_verify_data_sha256_ori(unsigned char * encrypt_data, int encrypt_data_len, unsigned char * data, int data_len);
int powp_verify_and_set_flag(void)
{
    unsigned char encrypt_data[MISCDATA_POWP_DATA_LEN];
    unsigned char *sn;
    int ret2=0;
    if(oem_repair_read_mmc_ex("powp", encrypt_data, MISCDATA_POWP_DATA_LEN) < 0)
        return VERIFY_FAIL;

	//debugf("powp_verify:  2 %d\n",MISCDATA_POWP_DATA_LEN);
		
	debugf("powp_verify: %s\n",encrypt_data);
    //debugPrintf_Hex1(encrypt_data, WT_POWP_LEN, "encrypt_data_read from miscdata");
    sn = get_product_sn();
    if(!sn)
        return VERIFY_FAIL;

    debugf("============= sn = %s =============\n",sn);
    
    ret2 = !powp_verify_data_sha256_ori(encrypt_data, 2048,sn, strlen(sn));
	POWP_flag = ret2;
	debugf("1=enable,0=disable the POWP_flag=%d\n",ret2);
	int ret=0;
	#ifdef CONFIG_EMMC_WP
	if(ret2){
		ret = mmc_set_pwr_wp(EMMC,0x00024000,1);  
	}
	#endif
	if (ret) {
		debugf("write powpflag fail\n");
    }else{
		debugf("write powpflag success\n");
	}
	return ret2;
}

//modify by ysong for diag oem fastboot begin
void cmd_oem_diag_lock(const char *arg, void *data, unsigned size)
{
	int ret = -1;
	char diag_enable[MISCDATA_CALI_DATA_LEN] = {0};

	ret = oem_repair_write_mmc_ex("cali",diag_enable);
	if(ret){
		fastboot_fail("clear diag bin fail");
	}else{
		fastboot_okay("");
	
		//udelay(500);
		usb_driver_exit();
		/* the last time to write log before reboot */
		//write_log_last();
		reboot_devices(CMD_FASTBOOT_MODE);
	}
}
//modify by ysong for diag oem fastboot end

//modify by ysong for cali mode begin
//#if defined(CONFIG_CALI_MODE)
extern int cali_verify_data_sha256_ori(unsigned char * encrypt_data, int encrypt_data_len, unsigned char * data, int data_len);
int cali_verify_and_set_flag(void)
{
    unsigned char encrypt_data[MISCDATA_CALI_DATA_LEN];
    unsigned char *sn;
    int ret2=0;
	
    oem_repair_read_mmc_ex("cali", encrypt_data, MISCDATA_CALI_DATA_LEN) ;
    
    //debugf("powp_verify:  2 %d\n",MISCDATA_POWP_DATA_LEN);
		
    //debugf("cali_verify: %s\n",encrypt_data);
    //debugPrintf_Hex1(encrypt_data, WT_POWP_LEN, "encrypt_data_read from miscdata");

    sn = get_product_sn();
    if(!sn)
        return 0;

    debugf("============= sn = %s =============\n",sn);
    
    ret2 = cali_verify_data_sha256_ori(encrypt_data, 2048,sn, strlen(sn));
	debugf("1=enable,0=disable the CAli mode=%d\n",ret2);

	return ret2;
}
//#endif
//modify by ysong for cali mode end

//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission begin
int fdl_download_verify_data(void)
{
    unsigned char encrypt_data[MISCDATA_FDL_VERIFY_DATA_LEN] = {0};
    unsigned char *sn;
    int ret=0;
	
    ret = common_raw_read("miscdata", MISCDATA_FDL_VERIFY_DATA_LEN, MISCDATA_FDL_VERIFY_BASE, encrypt_data);
	if (ret) {
		debugf("partition <miscdata> read data error\n");
		return 0;
    }

    sn = get_product_sn();
    if(!sn)
        return 0;

    debugf("============= sn = %s =============\n",sn);
    
    ret = (0 != hmd_rsa_decrypt_data("repair", encrypt_data, sn, 1) ||  0 != hmd_rsa_decrypt_data("factory", encrypt_data, sn, 1));
	debugf("1=enable,0=disable download mode=%d\n",ret);

	return ret;
}

int erase_fdl_download_auth_data(void)
{
	char miscdata_buf[MISCDATA_FDL_VERIFY_DATA_LEN] = {0};
	if (common_raw_write("miscdata", (uint64_t)MISCDATA_FDL_VERIFY_DATA_LEN, (uint64_t)0, MISCDATA_FDL_VERIFY_BASE, miscdata_buf)) {
		errorf("erase miscdata data %d fail on offset %lld.\n", MISCDATA_FDL_VERIFY_DATA_LEN, MISCDATA_FDL_VERIFY_BASE);
		return 1;
	}
	return 0;
}
//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission end

int cmd_fastboot_oem_handle(const char *subcmd, const char *arg, void *data, uint64_t size) {
	struct fastboot_cmd_desc cmd_list[] = {
		{ "getdllname", cmd_oem_getdllname },
		{ "auth_start", cmd_oem_auth_start },
		{ "permission",cmd_oem_permission },
		{ "getpermissions",cmd_oem_getpermissions },
		{ "repair", cmd_oem_repair },
		{ "hef", cmd_oem_hef },
		{ "get_devinfo", cmd_oem_get_devinfo },
		{ "powp_enable", cmd_oem_powp_enable },
		{ "diag_lock", cmd_oem_diag_lock },//modify by ysong for diag oem fastboot begin
#ifdef XX_OEM_TEST
		{ "powp_xx_enable", cmd_oem_powp_xx_enable },
#endif
		{ "sw_pub_key", cmd_oem_sw_pub_key },
		{ "factory_auth_start", cmd_oem_factory_auth_start },
		{ "zeroflag", cmd_oem_zeroflag },
		{ "enter_calibration", cmd_oem_enter_calibration },
		{ "reboot-edl", fb_cmd_reboot_edl },
	};

	int cmds_cnt = sizeof(cmd_list) / sizeof(cmd_list[0]);
	for (int i = 0; i < cmds_cnt; ++i) {
		if(!strcmp(subcmd, cmd_list[i].name)) {
			cmd_list[i].cb(arg, data, size);
			return 0;
		}
	}

	return -1;
}

void oem_fastboot_register_commands(void) {
	int i;
	struct fastboot_cmd_desc cmd_list[] = {
		{ "reboot-emergency", fb_cmd_reboot_edl },
		{ "upload", fb_cmd_upload },
	};

	fastboot_publish("product", HMD_PROJECT_NAME);
    fastboot_publish("unlocked", get_lock_status() ? "yes":"no");
	int cmds_cnt = sizeof(cmd_list) / sizeof(cmd_list[0]);
	for (i = 0; i < cmds_cnt; i++) {
		fastboot_register(cmd_list[i].name, cmd_list[i].cb);
	}
}

#endif


