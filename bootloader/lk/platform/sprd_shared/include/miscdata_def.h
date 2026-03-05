/*
 *MISCDATA_DEF_H - use field defined on partition of 'miscdata'
 *
 *  Copyright (C) 2021 Unisoc Communications Inc.
 *  History:
 *      Tue Feb 9 10:10:36 2021 zhenxiong.lai
 *      Description
 *
 */

#ifndef __MISCDATA_DEF_H__
#define __MISCDATA_DEF_H__

/*
 *Function:Verified Boot                                                     8K~8K+512
 *
 *Function:For uboot to read miscdata                                        9K~9K+1K
 *  Configure uboot to read loglevel                                         9K~9K+32
 *  Configure uboot to read the boot mode                                    9K+32~9K+64
 *  Store sysdump switch                                                     9K+64~9K+74
 *  Store the switch for uboot to read the selinux permission check          9k+352~9k+384
 *  Store the virtual boardid to match dtbo                                  9k+384~9k+388
 *  Configure the number of CPUs to be booted by kernel                      9k+388~9k+390
 *  Configure the authentication enable of the flag bit in calibration mode  9k+390~9k+394
 *  Write download failure information to the download flag                  9k+640~9k+672
 *  For uboot to save timestamps                                             9K+768~9K+800
 *  Record the watchdog enable menu                                          9K+800~9K+832
 *  Record the watchdog deep sleep enable menu                               9K+832~9K+864
 *  Enable uart baudrate and configurate parameters                          9K+864~9K+896
 *  Record usb_pin_mux configuration menu                                    9K+896~9K+912
 *  Record ddr_debug configuration menu                                      9K+912~9K+938
 *  Record apcpu_dvfs debug configuration menu                               9K+938~9K+942
 *  Record download record flag                                              9K+942~9K+1006
 *  Record download mode flag                                                9K+1006~9K+1022
 *  Configure the cdac tsx calibration data from caliration mode	     10k+384~10k+416
 *Function:Customer defined area                                             768K~1024K
 */
//ZOVERLAY_TAG_HMD_ONEIMAGE
#define MISCDATA_VERIFIED_BOOT		(8 * 1024)
#define PDT_INFO_LOCK_FLAG_OFFSET 	(MISCDATA_VERIFIED_BOOT)
#define PDT_INFO_LOCK_FLAG_SECTION_SIZE	(512 - 36)

#define PDT_INFO_DMVERITY_FLAG_OFFSET	(PDT_INFO_LOCK_FLAG_OFFSET + 476)
#define PDT_INFO_DMVERITY_FLAG_SECTION_SIZE	(32 + 4)

/** UBOOT dedicated area */
#define MISCDATA_UBOOT_BASE			(9 * 1024)
#define MISCDATA_UBOOT_BASE2			(520 * 1024)

/* loglevel */
#define DEBUG_INFO_OFFSET			MISCDATA_UBOOT_BASE
#define DEBUG_INFO_LEN    			(32)

/* first mode */
#define SET_FIRST_MODE_OFFSET 		(MISCDATA_UBOOT_BASE + 32)
#define SET_FIRST_MDOE_LEN 			(0x4)
#define SET_FIRST_MODE_MAGIC 		(0x53464d00)

/* sysdump */
#define MISCDATA_DUMP_DATA_START 	 (MISCDATA_UBOOT_BASE + 64)///fixme
#define FULLDUMP_PARTITION_MAGIC_LEN (10)

/*selinux flag*/
#define SELINUX_SWITCH_OFFSET (MISCDATA_UBOOT_BASE + 352)
#define SELINUX_INFO_LEN    32

/* virtual board id */
#define SET_VIRTUAL_BOARD_ID_OFFSET	(MISCDATA_UBOOT_BASE + 384)
#define SET_VIRTUAL_BOARD_ID_LEN	(0x4)
#define SET_VIRTUAL_BOARD_ID_MAGIC	(0x5AA50000)

/* core0 startup flag */
#define CORE_STARTUP_FLAG_OFFSET	(MISCDATA_UBOOT_BASE + 388)
#define CORE_STARTUP_FLAG_LEN		(0x2)

/* mode startup block flags */
#define STARTUP_BLOCK_FLAG_OFFSET	(MISCDATA_UBOOT_BASE + 390)
#define STARTUP_BLOCK_FLAG_LEN		(0x4)

/* enter download flags */
#define DL_PROCESS_OFFSET		(MISCDATA_UBOOT_BASE + 640)
#define DL_PROCESS_LEN		(0x20)

/* timestamp offset on partition miscdata */
#define SET_TIMESTAMP_OFFSET		(MISCDATA_UBOOT_BASE + 768)
#define SET_TIMESTAMP_MAGIC		(0x5445537e)


/*wdt enable flags */
#define WDTEN_DATA_OFFSET		(MISCDATA_UBOOT_BASE + 800)
#define WDTEN_DATA_LEN			(0x20)
#define WDTEN_MAGIC			(0xe551)

/*deepsleep wdt enable flags */
#define DSWDTEN_DATA_OFFSET             (MISCDATA_UBOOT_BASE + 832)
#define DSWDTEN_DATA_LEN		(0x20)

/*baudrate config flags */
#define CONS_BAUDRATE_OFFSET            (MISCDATA_UBOOT_BASE + 864)
#define CONS_BAUDRATE_LEN       (0x20)

/*usb_pin_mux config flags */
#define USBMUX_DATA_OFFSET		(MISCDATA_UBOOT_BASE + 896)
#define USBMUX_DATA_LEN			(0x10)
#define USBMUX_USB_DEFAULT		(0x0)
#define USBMUX_JTAG			(0xe)
#define USBMUX_UART			(0x4)
#define USBMUX_JTAG_APWDG		(0x1)
#define USBMUX_UNASSIGNED		(0xf)

/*usb2spuart config miscdata */
#define USB2SPUART_DATA_LEN			(0x03)
#define USB2SPUART_LENGTH_ENABLE	(0x06)
#define USB2SPUART_LENGTH_DISABLE	(0x07)

/* ddr debug config paras*/
#define MISCDATA_DDR_DEBUG_OFFSET	(MISCDATA_UBOOT_BASE + 912)
#define MISCDATA_DDR_DEBUG_LEN		(0x1A)

/* apcpu_dvfs debug config flags */
#define DVFS_SET_OFFSET			(MISCDATA_UBOOT_BASE + 938)
#define DVFS_SET_LEN			(0x4)

/* record download partition flag */
#define DOWNLOAD_RECORD_OFFSET	(MISCDATA_UBOOT_BASE + 942)
#define DOWNLOAD_RECORD_LEN	GPT_ENTRY_NUMBERS

/* distinguish download/autodloader mode */
#define DL_MODE_OFFSET		(MISCDATA_UBOOT_BASE + 1006)
#define DL_MODE_DATA_LEN	(0x10)
/* ===============MISCDATA_UBOOT_BASE2 (520 * 1024) Begin================*/

#define DL_IQ_DYN_OFFSET	(MISCDATA_UBOOT_BASE2)
#define DL_IQ_DYN_DATA_LEN	(0x40)


/* ===============MISCDATA_UBOOT_BASE2 (520 * 1024) End=================*/

/* usr base */
#define MISCDATA_USR_BASE  		(512 * 1024)

/* ADC data storage */
#define ADC_DATA_OFFSET  		(MISCDATA_USR_BASE)
#define ADC_DATA_START  		(ADC_DATA_OFFSET)

/* save the creation time of pac */
#define DATETIME_OFFSET 		(MISCDATA_USR_BASE + 5120)
#define DATETIME_LEN			(0x400)

/* tsx calibration data */
#define TSX_CALI_DATA_OFFSET		(10 * 1024 + 384)
#define TSX_CALI_DATA_LEN		(0x4)

/* HMD 普通分区 */
/*跟Simlock解锁码有关，写入的数据类型待确认（目前了解一个32字节字符串）*/
#define MISCDATA_GUID_BASE									(768*1024)
#define MISCDATA_GUID_DATA_LEN							(32)

#define MISCDATA_ZEROFLAG_BASE   (MISCDATA_GUID_BASE + MISCDATA_GUID_DATA_LEN)
#define MISCDATA_ZEROFLAG_DATA_LEN   (1)

#define MISCDATA_BLOCK_FASTBOOT_BASE			(MISCDATA_ZEROFLAG_BASE + MISCDATA_ZEROFLAG_DATA_LEN)
#define MISCDATA_BLOCK_FASTBOOT_DATA_LEN			(1)
#define MISCDATA_BLOCK_FASTBOOT_FLAG			"1"

#define MISCDATA_BLOCK_FACTORY_RESET_BASE		(MISCDATA_BLOCK_FASTBOOT_BASE + MISCDATA_BLOCK_FASTBOOT_DATA_LEN)
#define MISCDATA_BLOCK_FACTORY_RESET_DATA_LEN		(1)
#define MISCDATA_BLOCK_FACTORY_RESET_FLAG		"1"

#define MISCDATA_POWP_BASE  (MISCDATA_BLOCK_FACTORY_RESET_BASE + MISCDATA_BLOCK_FACTORY_RESET_DATA_LEN)
#define MISCDATA_POWP_DATA_LEN     (300)

#define MISCDATA_POWPFLAG_BASE   (MISCDATA_POWP_BASE + MISCDATA_POWP_DATA_LEN)
#define MISCDATA_POWPFLAG_DATA_LEN   (1)

#define MISCDATA_EDL_BASE	     (MISCDATA_POWPFLAG_BASE + MISCDATA_POWPFLAG_DATA_LEN)
#define MISCDATA_EDL_DATA_LEN	     (1)

/* off-mode-charge flag */
#define MISCDATA_OFF_MODE_CHARGE_FLAG_BASE     (MISCDATA_EDL_BASE + MISCDATA_EDL_DATA_LEN)
#define MISCDATA_OFF_MODE_CHARGE_FLAG_DATA_LEN        (1)

/* hmd_off_mode_charge flag */
#define MISCDATA_HMD_OFF_MODE_CHARGE_FLAG_BASE     (MISCDATA_OFF_MODE_CHARGE_FLAG_BASE + MISCDATA_OFF_MODE_CHARGE_FLAG_DATA_LEN)
#define MISCDATA_HMD_OFF_MODE_CHARGE_FLAG_DATA_LEN        (1)

/* System Property ro.boot.factoryresettime: the timestamp of last factory reset. e.g.  1600122249577 */
#define MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_BASE     (MISCDATA_HMD_OFF_MODE_CHARGE_FLAG_BASE + MISCDATA_HMD_OFF_MODE_CHARGE_FLAG_DATA_LEN)
#define MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_DATA_LEN        (14)

/* hmd_block_factory_reset flag */
#define MISCDATA_HMD_BLOCK_FACTORY_RESET_FLAG_BASE     (MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_BASE + MISCDATA_HMD_FACTORY_RESET_TIME_FLAG_DATA_LEN)
#define MISCDATA_HMD_BLOCK_FACTORY_RESET_FLAG_DATA_LEN        (1)

/* hmd_block_download_mode flag */
#define MISCDATA_HMD_BLOCK_DOWNLOAD_MODE_FLAG_BASE     (MISCDATA_HMD_BLOCK_FACTORY_RESET_FLAG_BASE + MISCDATA_HMD_BLOCK_FACTORY_RESET_FLAG_DATA_LEN)
#define MISCDATA_HMD_BLOCK_DOWNLOAD_MODE_FLAG_DATA_LEN        (1)

/* hmd_lock_status flag */
#define MISCDATA_HMD_LOCK_STATUS_FLAG_BASE     (MISCDATA_HMD_BLOCK_DOWNLOAD_MODE_FLAG_BASE + MISCDATA_HMD_BLOCK_DOWNLOAD_MODE_FLAG_DATA_LEN)
#define MISCDATA_HMD_LOCK_STATUS_FLAG_DATA_LEN        (1024)

/* hmd fastboot down flag */
#define MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE     (MISCDATA_HMD_LOCK_STATUS_FLAG_BASE + MISCDATA_HMD_LOCK_STATUS_FLAG_DATA_LEN)
#define MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE_DATA_LEN     (1)

/* hmd battery info */
/*battery sn*/
#define MISCDATA_BATTERY_SN_BASE     (MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE + MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE_DATA_LEN)
#define MISCDATA_BATTERY_SN_BASE_LEN     (32)

/*battery production date 2024-07-09*/
#define MISCDATA_BATTERY_PRODUCTION_DATE_BASE    (MISCDATA_BATTERY_SN_BASE + MISCDATA_BATTERY_SN_BASE_LEN)
#define MISCDATA_BATTERY_PRODUCTION_DATE_BASE_LEN     (10)

/*battery old sn*/
#define MISCDATA_BATTERY_OLD_SN_BASE  (MISCDATA_BATTERY_PRODUCTION_DATE_BASE + MISCDATA_BATTERY_PRODUCTION_DATE_BASE_LEN)
#define MISCDATA_BATTERY_OLD_SN_BASE_LEN     (32)


/*battery activation date 2024-07-10*/
#define MISCDATA_BATTERY_ACTIVATION_DATE_BASE    (MISCDATA_BATTERY_OLD_SN_BASE + MISCDATA_BATTERY_OLD_SN_BASE_LEN)
#define MISCDATA_BATTERY_ACTIVATION_DATE_BASE_LEN     (10)

/*battery charge cycles*/
#define MISCDATA_BATTERY_CHARGE_CYCLES_BASE    (MISCDATA_BATTERY_ACTIVATION_DATE_BASE + MISCDATA_BATTERY_ACTIVATION_DATE_BASE_LEN)
#define MISCDATA_BATTERY_CHARGE_CYCLES_BASE_LEN     (6)

//modify by ysong for cali mode begin
/* cali mode data */
#define MISCDATA_CALI_BASE     (MISCDATA_BATTERY_CHARGE_CYCLES_BASE + MISCDATA_BATTERY_CHARGE_CYCLES_BASE_LEN)
#define MISCDATA_CALI_DATA_LEN        (300) 
//modify by ysong for cali mode end

//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission begin
/* fdl verify data */
#define MISCDATA_FDL_VERIFY_BASE     (MISCDATA_CALI_BASE + MISCDATA_CALI_DATA_LEN)
#define MISCDATA_FDL_VERIFY_DATA_LEN       (348) 
//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission end


/* HMD POWP分区 */
/*SW SKU ID：数据长度32字节，例"600WW"*/
#define MISCDATA_SKU_ID_BASE								(7*1024*1024)//EMMC 16M address(0~1M GPT, 1~65M prodnv, 66~77M miscdata)(8M alignment)
#define MISCDATA_SKU_ID_DATA_LEN						(12)

/*Wallpaper ID：数据长度32字节，例："0x1"*/
#define MISCDATA_WALLPAPER_ID_BASE					(MISCDATA_SKU_ID_BASE + MISCDATA_SKU_ID_DATA_LEN)
#define MISCDATA_WALLPAPER_ID_DATA_LEN			(1)
/*TA code:  数据长度32字节，例："TA-1399"*/
#define MISCDATA_TA_CODE_BASE								(MISCDATA_WALLPAPER_ID_BASE + MISCDATA_WALLPAPER_ID_DATA_LEN)
#define MISCDATA_TA_CODE_DATA_LEN						(32)
/* hef flag */
#define MISCDATA_HEF_FLAG_BASE     (MISCDATA_TA_CODE_BASE + MISCDATA_TA_CODE_DATA_LEN)
#define MISCDATA_HEF_FLAG_DATA_LEN        (5)


/*0~63	PHONE SN     size of SP09=256,SP15=504 */
#define MISCDATA_PHONE_SN_BASE	(MISCDATA_HEF_FLAG_BASE + MISCDATA_HEF_FLAG_DATA_LEN)
#define MISCDATA_PHONE_SN_DATA_LEN	(504)

#endif /* __MISCDATA_DEF_H__ */

