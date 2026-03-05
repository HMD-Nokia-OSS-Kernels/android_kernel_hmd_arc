//#include <common.h>
#include <boot_mode.h>
#include <part_efi.h>
//#include <loader_common.h>
#include <sprd_common_rw.h>
#include <miscdata_def.h>
#include "sprd_cpcmdline.h"
//ZOVERLAY_TAG_HMD_ONEIMAGE

#ifdef CONFIG_HMD_FASTBOOT
extern int atoi(const char *src);
extern int tolower(int c) ;
/*
int tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
	{
		return c + 'a' - 'A';
	}
	else
	{
		return c;
	}
}*/

int htoi(char s[])
{
	int i;
	int n = 0;

	for (i = 0; (s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') || (s[i] >='A' && s[i] <= 'Z');++i)
	{
		if (tolower(s[i]) > '9')
		{
			n = 16 * n + (10 + tolower(s[i]) - 'a');
		}
		else
		{
			n = 16 * n + (tolower(s[i]) - '0');
		}
	}
	return n;
}

/*
int atoi(const char* str) {
   int flag = 1;
	int result = 0;
	
	if(str == NULL)
		return 0;
	while(*str == ' ' || *str == '\t')
		str++;
	
	if(*str == '-')
	{
		flag = -1;
		str++;
	}
	
	while(*str != '\0')
	{
		if(*str >= 0 && *str <= '9')
			result = result*10 + (*str - '0');
		
		else
		{
			break;
		}
		
		str++;
	}
	
	return result * flag;
}
*/

int oem_repair_read_mmc_ex(const char *type,unsigned char *buf,int len) {
	uint32_t ret = -1;
	uint64_t miscdata_address;
	uint64_t miscdata_len;
	char t_ch;

	char miscdata_buf[1024] = {0};

	debugf("oem repair read mmc get type %s\n", type);

	if(len > sizeof(miscdata_buf)) {
		errorf("oem_repair_read_mmc_ex read NV len error\n");
		return -1;
	}
	else if(!strcmp(type, "block_fastboot_mode")) {
		miscdata_address = MISCDATA_BLOCK_FASTBOOT_BASE;
		miscdata_len = MISCDATA_BLOCK_FASTBOOT_DATA_LEN;
	}
	else if(!strcmp(type, "block_factory_reset")) {
		miscdata_address = MISCDATA_BLOCK_FACTORY_RESET_BASE;
		miscdata_len = MISCDATA_BLOCK_FACTORY_RESET_DATA_LEN;
	}
	else if(!strcmp(type, "zeroflag")) {
		miscdata_address = MISCDATA_ZEROFLAG_BASE;
		miscdata_len = MISCDATA_ZEROFLAG_DATA_LEN;
	}
	else if(!strcmp(type, "powp")) {
		miscdata_address = MISCDATA_POWP_BASE;
		miscdata_len = MISCDATA_POWP_DATA_LEN;
	}
	else if(!strcmp(type, "hef")) {
		miscdata_address = MISCDATA_HEF_FLAG_BASE;
		miscdata_len = MISCDATA_HEF_FLAG_DATA_LEN;
	}
	else if(!strcmp(type, "cali")) {
		miscdata_address = MISCDATA_CALI_BASE;
		miscdata_len = MISCDATA_CALI_DATA_LEN;
	}
	else if(!strcmp(type, "tacode")) {
		miscdata_address = MISCDATA_TA_CODE_BASE;
		miscdata_len = MISCDATA_TA_CODE_DATA_LEN;
	}
	else if(!strcmp(type, "skuid") || !strcmp(type, "sku")) {
		miscdata_address = MISCDATA_SKU_ID_BASE;
		miscdata_len = MISCDATA_SKU_ID_DATA_LEN;
	}
	else if (!strcmp(type, "colorid") || !strcmp(type, "wallpapered")) {
		miscdata_address = MISCDATA_WALLPAPER_ID_BASE;
		miscdata_len = MISCDATA_WALLPAPER_ID_DATA_LEN;
	}
	else if (!strcmp(type, "fastboot_reboot_edl")) {
		miscdata_address = MISCDATA_EDL_BASE;
		miscdata_len = MISCDATA_EDL_DATA_LEN;
	}
	else if (!strcmp(type, "battery")) {
		miscdata_address = MISCDATA_BATTERY_SN_BASE;
		miscdata_len = MISCDATA_BATTERY_SN_BASE_LEN;
	}
	else {
		errorf("ERROR: unsupport repair type\n");
		return -1;
	}

	ret = common_raw_read("miscdata", miscdata_len, miscdata_address, miscdata_buf);
	if (ret) {
		errorf("partition <miscdata> read NV data error\n");
		return ret;
    }

	if (!strcmp(type, "colorid") || !strcmp(type, "wallpapered")) {
		t_ch = miscdata_buf[0];
		memset(miscdata_buf, 0, sizeof(miscdata_buf));
		sprintf(miscdata_buf,"0x%x",t_ch);
		miscdata_len = strlen(miscdata_buf)+1;
	}

	if(!strcmp(type, "hef") && (0 == miscdata_buf[0])) {
		memset(miscdata_buf, 0, sizeof(miscdata_buf));
		sprintf(miscdata_buf,"%s","0");
		miscdata_len = strlen(miscdata_buf)+1;
	}

	if(!strcmp(type, "skuid") || !strcmp(type, "sku")) {
		int isGo64 = 0;
		if(!strcmp(SPRD_BOARD_INFO_ID, "sp9863a 1h10 go")) {
			isGo64 = 1;
		}
		if ((strcmp(miscdata_buf, "100ZA") && strcmp(miscdata_buf, "100EEA") && strcmp(miscdata_buf, "100M0") && strcmp(miscdata_buf, "1HMWW") && strcmp(miscdata_buf, "1NPWW") && strcmp(miscdata_buf, "1NPM0") && strcmp(miscdata_buf, "1NPEEA")) 
		  || ((!strcmp(miscdata_buf, "100ZA") && isGo64) || ((!strcmp(miscdata_buf, "100M0") || !strcmp(miscdata_buf, "1HMWW") || !strcmp(miscdata_buf, "1NPWW") || !strcmp(miscdata_buf, "1NPM0") || !strcmp(miscdata_buf, "1NPEEA")) && !isGo64))) {
			memset(miscdata_buf, 0, sizeof(miscdata_buf));
			sprintf(miscdata_buf,"%s","100WW");
			miscdata_len = strlen(miscdata_buf)+1;
		}
	}

OUT:
	debugf("miscdata_buf %s\n", miscdata_buf);
	memcpy(buf, miscdata_buf, (len > 0) ? len : miscdata_len);

	return ret;
}
extern int POWP_flag;
extern int powp_size;
int oem_repair_write_mmc_ex(const char *type,unsigned char *buf) {
	char t_ch;
	int t_val=0;
	uint32_t ret = -1;
	uint64_t miscdata_address;
	uint64_t miscdata_len;
	char t_buf[8] = {0} ;
	char miscdata_buf[1024] = {0};


	debugf("oem repair write mmc get type %s\n", type);
	//only skuid wallpapered hef tacode should be protect by powp
	if(!strcmp(type, "skuid") || !strcmp(type, "sku") ||
	   !strcmp(type, "colorid") || !strcmp(type, "wallpapered")||
	   !strcmp(type, "hef")||!strcmp(type, "tacode")
	   )
	{
		if (POWP_flag) {
			debugf("powp status is enabled\n");
			return ret;
	    }
	}

	//modify by ysong for cali fastboot begin
	if((!strcmp(type, "powp")) || (!strcmp(type, "cali"))) {
		memcpy(miscdata_buf, buf, powp_size);
	}else{
		memcpy(miscdata_buf, buf, strlen(buf));
	}

	if(!strcmp(type, "powp")) {
		miscdata_address = MISCDATA_POWP_BASE;
		miscdata_len = MISCDATA_POWP_DATA_LEN;
 	}
	 else if(!strcmp(type, "zeroflag")) {
		miscdata_address = MISCDATA_ZEROFLAG_BASE;
		miscdata_len = MISCDATA_ZEROFLAG_DATA_LEN;
	}else if(!strcmp(type, "hef")) {
		miscdata_address = MISCDATA_HEF_FLAG_BASE;
		miscdata_len = MISCDATA_HEF_FLAG_DATA_LEN;
	}else if(!strcmp(type, "cali")) {
		miscdata_address = MISCDATA_CALI_BASE;
		miscdata_len = MISCDATA_CALI_DATA_LEN;
	}else if(!strcmp(type, "tacode")) {
		miscdata_address = MISCDATA_TA_CODE_BASE;
		miscdata_len = MISCDATA_TA_CODE_DATA_LEN;
	}else if(!strcmp(type, "skuid") || !strcmp(type, "sku")) {
		miscdata_address = MISCDATA_SKU_ID_BASE;
		miscdata_len = MISCDATA_SKU_ID_DATA_LEN;
	}else if (!strcmp(type, "colorid") || !strcmp(type, "wallpapered")) {
		miscdata_address = MISCDATA_WALLPAPER_ID_BASE;
		miscdata_len = MISCDATA_WALLPAPER_ID_DATA_LEN;

		if (!strncmp(miscdata_buf,"0x",2)) {
           	memcpy(t_buf,miscdata_buf+2,2);
			t_val =  htoi(t_buf);
		}else{
			memcpy(t_buf,miscdata_buf,2);
			t_val =  atoi(t_buf);
		}
		memset(miscdata_buf, 0, sizeof(miscdata_buf));
		//t_val =  mystrtoint(t_buf);
		if(-1 == t_val){
			errorf("ERROR: write colorid or wallpapered error\n");
			return -1;
		}else{
			miscdata_buf[0] = (char)t_val;
		}
	}
	else if (!strcmp(type, "fastboot_reboot_edl")) {
		miscdata_address = MISCDATA_EDL_BASE;
		miscdata_len = MISCDATA_EDL_DATA_LEN;
	}
	else if (!strcmp(type, "battery")) {
		miscdata_address = MISCDATA_BATTERY_SN_BASE;
		miscdata_len = MISCDATA_BATTERY_SN_BASE_LEN;
	}
	else {
		errorf("ERROR: unsupport repair type\n");
		return -1;
	}
	debugf("miscdata_buf[0] is == %d,miscdata_len = %d\n", miscdata_buf[0],miscdata_len);
	ret = common_raw_write("miscdata", miscdata_len, (u64)0, miscdata_address, miscdata_buf);
	if (ret) {
		errorf("partition <miscdata> write NV data error\n");
		return ret;
    }

	debugf("buf %s\n", buf);

EXIT:
	return ret;
}

#endif