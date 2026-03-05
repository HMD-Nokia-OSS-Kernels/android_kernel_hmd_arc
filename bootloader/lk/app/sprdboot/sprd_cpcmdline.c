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
#include <sprd_common.h>
#include <sprd_cpcmdline.h>
#include <boot_mode.h>
#include <sprd_boardid.h>
#include "boot_parse.h"
#include <sprd_common_rw.h>
#include <miscdata_def.h>
#ifdef CONFIG_UDC
#include <udc.h>
#endif

char *g_CPcmdlineBuf = NULL;
#if defined( CONFIG_KERNEL_BOOT_CP )
char CPcmdlineBuf[MAX_CP_CMDLINE_LEN];
#endif
extern int sprd_get_pcbversion(void);


static char bootconfig_mode_return[64];

static char bootconfig_chipid_return[64];

static const char *cmd_arr[] = {
	BOOT_MODE,
	CALIBRATION_MODE,
	LTE_MODE,
	AP_VERSION,
	RF_BOARD_ID,
	RF_HW_INFO,
	K32_LESS,
	AUTO_TEST,
	CRYSTAL_TYPE,
	RF_HW_ID,
#ifdef CONFIG_SC9833
	RF_CHIP_ID,
#endif
#ifdef CONFIG_PMIC_CHIP_ID
	PMIC_CHIP_ID,
#endif
	CHIPUID,
	MODEM_BOOT_METHOD,
	WCN_CLK_ID,
	PCB_VERSION,
	ANDROIDBOOT_HARDWARE,
	CPCMDLINE,
	POWER_MODE,
	DCDC_ID,
	SPRDBOOT_MODE,
	UOB_BOARD_ID,
	ANDROIDBOOT_SKUID,
	ANDROIDBOOT_WALLPAPER,
	ANDROIDBOOT_TA_CODE,
	ANDROIDBOOT_REAL_TA_CODE,
	ANDROIDBOOT_SKUNAME,
	ANDROIDBOOT_GUID,
	ANDROIDBOOT_HEF,
	ANDROIDBOOT_FACTORY_RESET_TIME,
	ANDROIDBOOT_PRODUCT_VENDOR_SKU,
	ANDROIDBOOT_NSRP,
	PCB_VERSION,
	TYPC_BOARD_ID, //add by ysong for typec
#ifdef CONFIG_WCN_BOARD_ID
	WCN_BOARD_ID,
#endif
#ifdef CONFIG_UDC  //add  by jinqiang for udc
	UDC,
#endif
	NULL
};

static bool is_invalid_cmd(const char *cmd)
{
	int i = 0;
	while(NULL != cmd_arr[i])
	{
		if(0 == strcmp(cmd_arr[i], cmd)){
			return true;
		}
		i++;
	}
	return false;
}

static void cpcmdline_get_lte_mode(char *value)
{
#ifdef CONFIG_SUPPORT_TDLTE
	strcpy(value, "tcsfb");
#elif defined CONFIG_SUPPORT_WLTE
	strcpy(value, "fcsfb");
#elif defined CONFIG_SUPPORT_LTE
	strcpy(value, "lcsfb");
#endif
}

static void cpcmdline_get_modem_boot_method(char *value)
{
#ifdef CONFIG_DDR_BOOT
	strcpy(value, "ddrboot");
#elif defined CONFIG_EMMC_BOOT
	strcpy(value, "emmcboot");
#elif defined CONFIG_NAND_BOOT
	strcpy(value, "nandboot");
#endif
}


static void cmdline_prepare(void)
{
#if defined( CONFIG_KERNEL_BOOT_CP )
	g_CPcmdlineBuf = CPcmdlineBuf;
#else
#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
extern void *parse_cpcmdline_addr(void);
	g_CPcmdlineBuf = (char*)parse_cpcmdline_addr();
#else
#ifdef CALIBRATION_FLAG_CP0
	g_CPcmdlineBuf = (char*)CALIBRATION_FLAG_CP0;
#endif
#ifdef CALIBRATION_FLAG_CP1
	g_CPcmdlineBuf = (char*)CALIBRATION_FLAG_CP1;
#endif
#endif
#endif
	if (g_CPcmdlineBuf)
		memset(g_CPcmdlineBuf, 0, MAX_CP_CMDLINE_LEN);
	debugf("g_CPcmdlineBuf = 0x%p\n" , g_CPcmdlineBuf);
}

static void cmdline_add_cp_cmdline(const char *cmd, const char* value)
{
	char *p;
	int len;

	if(!is_invalid_cmd(cmd))return;

	if (NULL == g_CPcmdlineBuf)
		return;

	len = strlen(g_CPcmdlineBuf);
	p = g_CPcmdlineBuf + len;
	if ((len + strlen(cmd) + strlen(value)) < MAX_CP_CMDLINE_LEN)
		snprintf(p, MAX_CP_CMDLINE_LEN - len, "%s=%s ", cmd, value);
	else
		panic("cp cmdline size is overflow.\n");
}

char *bootconfig_get_bootmode(void)
{
	return bootconfig_mode_return;
}

char *bootconfig_get_chipid(void)
{
	return bootconfig_chipid_return;
}

static void fixup_bootmode(char *value)
{
#ifndef CONFIG_BOOTCONFIG
                cmdline_add_cp_cmdline(BOOT_MODE, value);
#else
                memset(bootconfig_mode_return, 0, sizeof(bootconfig_mode_return));
                cmdline_add_cp_cmdline(SPRDBOOT_MODE, value);
                sprintf(bootconfig_mode_return, "androidboot.mode=%s\n", value);
#endif
		return;
}

static void fixup_pmic_chipid(void)
{
        char buf[30]={0};
#ifdef CONFIG_PMIC_CHIP_ID
        /* get pmic chipid */
        extern int sprd_get_pmic_chipid(void);

        unsigned int pmicid = sprd_get_pmic_chipid();
        sprintf(buf,"%x",pmicid);

#ifndef CONFIG_BOOTCONFIG
        cmdline_add_cp_cmdline(PMIC_CHIP_ID, buf);
#else
       	memset(bootconfig_chipid_return, 0, sizeof(bootconfig_chipid_return));
        sprintf(bootconfig_chipid_return,"androidboot.pmic.chipid=%s\n",buf);
#endif
#endif
        return;
}
/*add by jinqiang for udc */
#ifdef  CONFIG_UDC
extern unsigned short   * udc_buffer;
extern unsigned long int                         udc_length;
int fdt_fixup_udc_cmdline(void)
{
        char buf[128];
        int str_len;
        int ret=0;
        memset(buf, 0, 128);
        sprintf(buf, "0x%x, 0x%x, 0x%x ", (long) udc_buffer,  udc_length, udc_get_lcd_offset());
		errorf("udc_buffer=0x%x  ,udc_length=0x%x   udc_get_lcd_offset()=0x%x \n", (long) udc_buffer,  udc_length, udc_get_lcd_offset());
		cmdline_add_cp_cmdline(UDC, buf);
        return ret;
}
#endif

/*
#define BUF_SIZE (MISCDATA_SKU_ID_DATA_LEN+MISCDATA_WALLPAPER_ID_DATA_LEN+MISCDATA_TA_CODE_DATA_LEN+MISCDATA_GUID_DATA_LEN)
void fdt_fixup_hmd_cmdline(void)
{
	char buf_t[128], buf_p[128] = {0},buf_sku[128] = {0},buf_tacode[32] = {0};
	char buf[30] = {0};
	int boardid = 0;
	int pcb_version = 0;
	int efuse_val_read = 0;
	memset(buf_p, 0, 128);
	memset(buf_t, 0, 128);
	if (common_raw_read("miscdata", (uint64_t)MISCDATA_SKU_ID_DATA_LEN, MISCDATA_SKU_ID_BASE, buf_p)){	    
		errorf("read hmd sku id data error from miscdata...\n");
	}
	if ((strcmp(buf_p, "100ZA") != 0)&&(strcmp(buf_p, "100EEA") != 0)&&(strcmp(buf_p, "100M0") != 0)) {
	errorf("wrong skuid,using 100WW...\n");
	strcpy(buf_p,"100WW");
	}
	cmdline_add_cp_cmdline(ANDROIDBOOT_SKUID, buf_p);
	strcpy(buf_sku,buf_p);
	cmdline_add_cp_cmdline(ANDROIDBOOT_PRODUCT_VENDOR_SKU, buf_p);
	memset(buf_p, 0, 128);


	if (common_raw_read("miscdata", (uint64_t)MISCDATA_WALLPAPER_ID_DATA_LEN, MISCDATA_WALLPAPER_ID_BASE, buf_p)){	    
		errorf("read hmd wallpaper data error from miscdata...\n");
	}
	sprintf(buf_t, "%d",buf_p[0]);
	cmdline_add_cp_cmdline(ANDROIDBOOT_WALLPAPER, buf_t);
	memset(buf_t, 0, 128);
	memset(buf_p, 0, 128);
	boardid = sprd_get_bandinfo();
	if(boardid==4){
		strcpy(buf_tacode,"TA-1697");
	}else if(boardid==3){
		strcpy(buf_tacode,"TA-1682");
	}else{
		strcpy(buf_tacode,"unknown");
	}

	cmdline_add_cp_cmdline(ANDROIDBOOT_TA_CODE, buf_tacode);
	memset(buf_p, 0, 128);
	//The hardware speakers used for HMD projects are reversed,for ozo param adjustment
#if 0
	memset(buf_tacode, 0, 32);
	//boardid = sprd_get_bandinfo_real();
	if(boardid==0){
		strcpy(buf_tacode,"TA-1457");
	}else if(boardid==1){
		strcpy(buf_tacode,"TA-1462");
	}else if(boardid==2){
		strcpy(buf_tacode,"TA-1472");
	}else if(boardid==3){
		strcpy(buf_tacode,"TA-1503");
	}else if(boardid==4){
		strcpy(buf_tacode,"TA-1512");
	}else{
		strcpy(buf_tacode,"unknown");
	}
    cmdline_add_cp_cmdline(ANDROIDBOOT_REAL_TA_CODE, buf_tacode);


	if((strcmp(buf_sku, "600WW") == 0)&&(strcmp(buf_tacode, "TA-1472") == 0)){
		strcpy(buf_p,"600WW_WIFI");
	}else if((strcmp(buf_sku, "600EEA") == 0)&&(strcmp(buf_tacode, "TA-1472") == 0)){
		strcpy(buf_p,"600EEA_WIFI");
	}else if((strcmp(buf_sku, "600RU") == 0)&&(strcmp(buf_tacode, "TA-1472") == 0)){
		strcpy(buf_p,"600RU_WIFI");
	}else{
		strcpy(buf_p,buf_sku);
	}
	cmdline_add_cp_cmdline(ANDROIDBOOT_SKUNAME, buf_p);
	memset(buf_p, 0, 128);
	
	if (common_raw_read("miscdata", (uint64_t)MISCDATA_GUID_DATA_LEN, MISCDATA_GUID_BASE, buf_p)){	    
		errorf("read hmd guid data error from miscdata...\n");
	}
	cmdline_add_cp_cmdline(ANDROIDBOOT_GUID, buf_p);
	memset(buf_p, 0, 128);
#endif
	
	if (common_raw_read("miscdata", (uint64_t)MISCDATA_HEF_FLAG_DATA_LEN, MISCDATA_HEF_FLAG_BASE, buf_p)){	    
		errorf("read hmd hef data error from miscdata...\n");
	}
	if(buf_p[0]==0)
	{
		sprintf(buf_t, "%s","0");	
	}else{
		sprintf(buf_t, "%s", buf_p);
	}
	cmdline_add_cp_cmdline(ANDROIDBOOT_HEF, buf_t);
	memset(buf_t, 0, 128);
	memset(buf_p, 0, 128);
	
	if ((NULL != g_env_bootmode) && (strcmp(g_env_bootmode, "cali") != 0)) {
		if (common_raw_read("miscdata", (uint64_t)MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_DATA_LEN, MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_BASE, buf_p)){	    
			errorf("read hmd factory reset time data error from miscdata...\n");
		//	return -1;
		}
		cmdline_add_cp_cmdline(ANDROIDBOOT_FACTORY_RESET_TIME, buf_p);
	}


	pcb_version = sprd_get_pcb_version();
	if(pcb_version==1){
		strcpy(buf,"V0.1");
	}
	else if(pcb_version==2){
		strcpy(buf,"V0.2");
	}
	else if(pcb_version==3){
		strcpy(buf,"V0.3");
	}
	else if(pcb_version==4){
		strcpy(buf,"V0.4");
	}
	cmdline_add_cp_cmdline(PCB_VERSION,buf);
	memset(buf, 0, 30);
	efuse_val_read = nsrp_efuse_read();
	sprintf(buf, "%d",efuse_val_read);
	cmdline_add_cp_cmdline(ANDROIDBOOT_NSRP,buf);

}*/

void cp_cmdline_fixup(void)
{
	char val[30] = {0};
	int boardid = 0;
	int boardid_typec = 0; //add by ysong for hmd typec
	int value_less = 0;
	char buf[30] = {0};
	int uob_boardid = 0;
	int value_datamode = 0;
	char value_simmode = 0;
	int value_wifi_gpio = 0;
	char *value;
	int wcn_boardid = 0;

	cmdline_prepare();

	// androd boot mode
	value = g_env_bootmode;
	if (NULL != value)
		fixup_bootmode(value);
	// calibration parameters
	value = get_calibration_parameter();
	if (NULL != value) {
		if (NULL != strstr(value, CALIBRATION_MODE)) {
			// "calibration=%d,%d,146 ", must skipp calibration=
			value += strlen(CALIBRATION_MODE) + 1;
			cmdline_add_cp_cmdline(CALIBRATION_MODE, value);
		} else if (NULL != strstr(value, AUTO_TEST)) {
			// "autotest=1", must skipp autotest=
			value += strlen(AUTO_TEST) + 1;
			cmdline_add_cp_cmdline(AUTO_TEST, value);
		}
	}

	// lte mode
	memset(val, '\0', sizeof(val));
	cpcmdline_get_lte_mode(val);
	if (strlen(val))
		cmdline_add_cp_cmdline(LTE_MODE, val);

	//chip uid
#ifdef CONFIG_READ_UID
	{
		unsigned int block0;
		unsigned int block1;
		get_efuse_uid_ex(&block0,&block1);
		sprintf(buf,"0x%08x,0x%08x",block0,block1);
		cmdline_add_cp_cmdline(CHIPUID,buf);
	}
#endif
/* crystal and rf band auto adaption used on some platforms, such as sharkl, pike series */
/* Using RF band adaption in each board configuration and print information in cpcmdline*/
#if (defined(CONFIG_BOARD_ID) || defined(CONFIG_BAND_DETECT))
	//rf band auto adaption
	boardid = sprd_get_bandinfo();
	sprintf(buf,"%d",boardid);
	cmdline_add_cp_cmdline(RF_BOARD_ID, buf);
#endif
	//add by ysong for hmd typec
	boardid_typec = sprd_get_pcb_version();
	sprintf(buf,"%d",boardid_typec);
	cmdline_add_cp_cmdline(TYPC_BOARD_ID, buf);
#ifdef CONFIG_ANDROIDBOOT_HARDWARE
	value_wifi_gpio = sprd_get_wifi_mode();
	value_datamode = sprd_get_data_mode();
	value_simmode = sprd_get_sim();

	if ( value_wifi_gpio == 1 ) {
		if( value_datamode ==1 || value_simmode == 1 ) {
			debugf("error config: value_wifi_gpio = %d, value_datamode = %d, value_simmode = %d \n",
			value_wifi_gpio, value_datamode, value_simmode);
		} else {
			sprintf(buf,"%s","wifionly");
			cmdline_add_cp_cmdline(ANDROIDBOOT_HARDWARE, buf);
		}
	} else if ( value_datamode ==1 || value_simmode == 1 ) {
		if(value_datamode ==1 && value_simmode == 1)
			sprintf(buf,"%s","dataonly_singlesim");
		else
			sprintf(buf,"%s",(value_simmode?"singlesim":"dataonly"));

		cmdline_add_cp_cmdline(ANDROIDBOOT_HARDWARE, buf);
	}
#endif/* Printing the clock and crystal type in cpcmdline by clk & crystal adaption */
#ifdef CONFIG_UOB_BOARD_ID
	uob_boardid = sprd_get_uob_boardid();
	if (snprintf(buf, sizeof(buf), "%d", uob_boardid)) {
		cmdline_add_cp_cmdline(UOB_BOARD_ID,buf);
	} else {
		errorf("%s %s line %d has error.\n uob_boardid = %d\n", __FILE__, __FUNCTION__, __LINE__, uob_boardid);
	}

#endif
#if (defined(CONFIG_BOARD_ID) || defined(CONFIG_ADIE_SC2730))
	//board id
	value_less=sprd_get_boardid();
	sprintf(buf,"%d",value_less);
	cmdline_add_cp_cmdline(RF_HW_ID, buf);

	//26m crystal auto adaption
	sprintf(buf,"%d", sprd_get_crystal());
	cmdline_add_cp_cmdline(CRYSTAL_TYPE, buf);
#endif
	//32k crystal auto adaption
#if !defined(CONFIG_BOARD_ID) && !defined(CONFIG_32K_TYPE)
	sprintf(buf,"%d", 1);   // 1 K_LESS
#else
	sprintf(buf,"%d", sprd_get_32k());
#endif
	cmdline_add_cp_cmdline(K32_LESS, buf);
#ifdef CONFIG_WCN_DETECT
	//wcn crystal auto adaption
	sprintf(buf,"%d", sprd_get_wcn_crystal());
	cmdline_add_cp_cmdline(WCN_CLK_ID,buf);
#endif
#ifdef CONFIG_PCB_VERSION
	sprintf(buf,"%d",sprd_get_pcbversion());
	cmdline_add_cp_cmdline(PCB_VERSION, buf);
#endif
/* check the power mode thought option/gpio */
#ifdef CONFIG_POWER_MODE
	snprintf(buf,sizeof(buf),"%d", sprd_get_power_mode());
	cmdline_add_cp_cmdline(POWER_MODE, buf);
#endif

#ifdef CONFIG_MUTI_DCDC
	snprintf(buf,sizeof(buf),"%d", sprd_get_dcdc_muti());
	cmdline_add_cp_cmdline(DCDC_ID, buf);
#endif

#ifdef CONFIG_WCN_BOARD_ID
	wcn_boardid = sprd_get_wcn_boardid();
	sprintf(buf,"%d", wcn_boardid);
	cmdline_add_cp_cmdline(WCN_BOARD_ID,buf);
#endif

#ifdef CONFIG_SC9833
	{
		extern u8 DRV_RF_Get_Type(void);

		u8 type = DRV_RF_Get_Type();
		sprintf(buf, "%d", type);
		cmdline_add_cp_cmdline(RF_CHIP_ID, buf);
	}
#endif
//add by jinqiang for udc
#ifdef  CONFIG_UDC
	fdt_fixup_udc_cmdline();
#endif
	//fdt_fixup_hmd_cmdline();
	fixup_pmic_chipid();
	// modem boot method
	memset(val, '\0', sizeof(val));
	cpcmdline_get_modem_boot_method(val);
	if(strlen(val))
	{
		cmdline_add_cp_cmdline(MODEM_BOOT_METHOD, val);
	}
	cmdline_add_cp_cmdline(CPCMDLINE,"end");
	debugf("cp cmdline: %s\n", g_CPcmdlineBuf);
}

char *cp_getcmdline(void)
{
	return g_CPcmdlineBuf;
}

