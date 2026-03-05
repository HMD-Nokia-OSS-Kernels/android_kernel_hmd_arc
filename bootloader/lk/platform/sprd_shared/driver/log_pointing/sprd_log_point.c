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

/*
 * The driver program applies to all versions of sprd full time log.
 * Update History:
 *  Version Number        Author            Date
 *      v0.1            sitao.chen          2022.10
 *
 */

#include <config.h>
#include <sprd_common.h>
#include <linux/compiler.h>
#include <kernel/spinlock.h>
#include "sprd_log_point.h"
volatile uint32_t g_last_step = 0;

#ifdef CONFIG_ENABLE_LOGPOINT
//#define FTL_DEBUG
#ifdef FTL_DEBUG
#define point_debug(fmt, args...) do { dprintf(INFO,"log_point: %s(): ", __func__);dprintf(INFO,fmt, ##args); } while (0)
#else
#define point_debug(fmt, args...)
#endif

volatile spin_lock_t logpoint_lock = SPIN_LOCK_INITIAL_VALUE;

#ifdef CONFIG_SOC_ETB_PROJECT
/* get aon sys frt count */
u32 sprd_get_aon_sysfrt_cnt(void)
{
    return CHIP_REG_GET(SPRD_AON_SYSFRT_BASE + 0x4);
}

static inline void ram_enable(void)
{
    /* unlock write access */
    CHIP_REG_SET(ETB_LOCK_OFFSET_KEY, CS_LOCK_KEY_VALUE);

    /* disable etb */
    CHIP_REG_SET(ETB_ENABLE, 0);
}

static inline void rametb_set_wr_ptr(u32 pos)
{
    CHIP_REG_SET(ETB_RWP, pos);
}

static inline void rametb_set_rd_ptr(u32 pos)
{
    CHIP_REG_SET(ETB_RRP, pos);
}

static inline void rametb_wr_data(u32 data)
{
    CHIP_REG_SET(ETB_RWD, data);
}

static inline u32 rametb_rd_data(void)
{
    return CHIP_REG_GET(ETB_RRD);
}
static u32 rametb_read_data(u32 pos, u32 cnt)
{
    u32 i = 0, data;

    rametb_set_rd_ptr(pos);
    while (i++ < cnt) {
        data = rametb_rd_data();
    }
    return data;
}

static void write_first_step_and_ts(void)
{
    u32 ts, temp, i, length;
    char text[8] = {0};

    ts = sprd_get_aon_sysfrt_cnt();
    point_debug("first timestamp = 0x%x\n", ts);

    length = strlen("LKSTAG");
    if (length > 8)
        length = 8;
    memcpy(text, "LKSTAG", length);

    rametb_set_wr_ptr(FIRST_TS_BASE);

    rametb_wr_data(ts);
    rametb_wr_data(NUM_FIRST_STEP);
    for(i = 0; i < 8; i+=4) /* write 8 byte */
    {
        temp =  text[i] | (text[i+1] << 8) | (text[i+2] << 16) | (text[i+3] << 24);
        rametb_wr_data(temp);
    }
}

static void write_last_step_and_ts(u32 laststep_num)
{
    u32 ts;
    u32 coreid, orig_data, orig_ts;
    u32 core_data;

    ts = sprd_get_aon_sysfrt_cnt();
    coreid = arch_curr_cpu_num();

    point_debug("last timestamp = 0x%x\n", ts);
    point_debug("current coreid = %d\n", coreid);

    rametb_set_wr_ptr(LAST_TS_BASE);
    if (coreid == 0x0) {
        core_data = rametb_read_data(LAST_TS_BASE, 2);
        core_data |= (0x1 << laststep_num);
        orig_ts = rametb_read_data(LAST_TS_BASE, 3);
        orig_data= rametb_read_data(LAST_TS_BASE, 4);

        rametb_wr_data(ts);
        rametb_wr_data(core_data);
        rametb_wr_data(orig_ts);
        rametb_wr_data(orig_data);
    }
    else if (coreid == 0x1) {
        orig_ts = rametb_read_data(LAST_TS_BASE, 1);
        orig_data= rametb_read_data(LAST_TS_BASE, 2);
        core_data = rametb_read_data(LAST_TS_BASE, 4);
        core_data |=  (0x1 << laststep_num);

        rametb_wr_data(orig_ts);
        rametb_wr_data(orig_data);
        rametb_wr_data(ts);
        rametb_wr_data(core_data);
    }

    point_debug("orig_data = 0x%x\n", orig_data);
    point_debug("core_data = 0x%x\n", core_data);

}

static void write_save_point(u32 save_num)
{
    u32 ts_ago, ts_now, delta;

    ts_now = sprd_get_aon_sysfrt_cnt();

    rametb_set_rd_ptr(SPT_PUBLIC_TS_BASE);
    ts_ago = rametb_rd_data();

    delta = ts_now - ts_ago;

    point_debug("savepoint timestamp ago = 0x%x\n", ts_ago);
    point_debug("savepoint timestamp now = 0x%x\n", ts_now);
    point_debug("savepoint timestamp delta = 0x%x\n", delta);
    point_debug("savepoint save_num = 0x%x\n", save_num);

    rametb_set_wr_ptr(SPT_PUBLIC_TS_BASE);

    rametb_wr_data(ts_now);
    rametb_wr_data(delta);
    rametb_wr_data(save_num);
    rametb_wr_data(0);
}

static void write_16byte_chardesc(char *desc)
{
    char text[16] = {0};
    u32 length, temp;
    u32 i;

    length = strlen(desc);
    if (length > 16)
        length = 16;
    memcpy(text, desc, length);

    rametb_set_wr_ptr(SPT_CHAR_DESCRI_BASE);
    for(i = 0; i < 16; i+=4) {  /* write 16 byte */
        temp =  text[i] | (text[i+1] << 8) | (text[i+2] << 16) | (text[i+3] << 24);
        rametb_wr_data(temp);
    }
}

static void point_data_cpy2lk_backup(void)
{
    int i;
    u32 content[16] = {0};
    u32 data;

    /* read lk log point data */
    rametb_set_rd_ptr(FIRST_TS_BASE);
    for (i = 0; i < 16; i++) {
        content[i] = rametb_rd_data();
    }

    #if defined(FTL_DEBUG)
    for (i=0; i<16; i++) {
        point_debug("content[%d] = 0x%x\n", i, content[i]);
    }
    #endif

    rametb_set_wr_ptr(LK_BAKEUP_BASE);
    for (i = 0; i < 16; i++) {
        rametb_wr_data(content[i]);
    }

}

static void clear_region(void)
{
    u32 temp_base = FIRST_TS_BASE;

	point_data_cpy2lk_backup();
    while(temp_base < LOG_PONIT_LK_END) {
        rametb_set_wr_ptr(temp_base);
        rametb_wr_data(0x0);
        rametb_wr_data(0x0);
        rametb_wr_data(0x0);
        rametb_wr_data(0x0);
        temp_base += 0x10;
    }

}

static u32 boot_count(void)
{
    u32 cnt;

    cnt = rametb_read_data(CHIPRAM_BAKEUP_BASE, 4);
    point_debug("current reboot count is %d\n", cnt);

    return cnt;
}

/* flag default 0 */
static void lk_panic_flag_write(u32 flag, u32 addr)
{
    u32 bak_base = CHIPRAM_BAKEUP_BASE;
    u32 data_ts, data_flag, data_char, data_reboot;

    rametb_set_rd_ptr(bak_base);
    data_ts = rametb_rd_data();
    data_flag = rametb_rd_data();
    data_char = rametb_rd_data();
    data_reboot = rametb_rd_data();

    point_debug("current data_flag is %d\n", data_flag);
    point_debug("current reboot count is %d\n", data_reboot);

    rametb_set_wr_ptr(bak_base);
    rametb_wr_data(data_ts);
    if(flag == 0x1){    /* lk-self write 1 at start */
        rametb_wr_data(0x1);
    }
    else {
        rametb_wr_data(0);
    }
    rametb_wr_data(data_char);
    rametb_wr_data(data_reboot);

    rametb_set_rd_ptr(bak_base+0x20);
    data_ts = rametb_rd_data();
    data_flag = rametb_rd_data();
    data_char = rametb_rd_data();
    data_reboot = rametb_rd_data();

    point_debug("current data_flag is %d\n", data_flag);
    point_debug("current reboot count is %d\n", data_reboot);

    rametb_set_wr_ptr(bak_base+0x20);
    rametb_wr_data(data_ts);
    rametb_wr_data(data_flag);
    rametb_wr_data(data_char);
    rametb_wr_data(addr);
}

#endif /* SOC ETB doc */

#ifdef CONFIG_IRAM_PROJECT   /* AON IRAM doc */

#define CHIPRAM_BAKEUP_BASE			(LOG_POINT_SAVE_BASE-0x40)
/* get aon sys frt count */
u32 sprd_get_aon_sysfrt_cnt(void)
{
    return CHIP_REG_GET(SPRD_AON_SYSFRT_BASE + 0x4);
}

static inline void ram_enable(void) {}

static void write_first_step_and_ts(void)
{
    u32 ts;

    ts = sprd_get_aon_sysfrt_cnt();

    point_debug("First ts = 0x%x\n", ts);

    CHIP_REG_SET(FIRST_TS_BASE, ts);
    CHIP_REG_SET(FIRST_STEP_BASE, NUM_FIRST_STEP);
    memcpy((void*)(FIRST_STEP_BASE+0x4), "LK-STAG", 7);
}

/* use bit num to record, only time ponit stag */
static void write_last_step_and_ts(u32 laststep_num)
{
    char text[8] = {0};
    u32 length, ts, data;
    u32 coreid;

    ts = sprd_get_aon_sysfrt_cnt();
    coreid = arch_curr_cpu_num();

    point_debug("last_ts = 0x%x\n", ts);
    point_debug("current coreid = 0x%x\n", coreid);

    if (0 == coreid) {
        data = CHIP_REG_GET(LAST_STEPCORE0_BASE);
        data |= (0x1 << laststep_num);
        CHIP_REG_SET(LAST_TS_BASE, ts);
        CHIP_REG_SET(LAST_STEPCORE0_BASE, data);
    } else if (0x1 == coreid){
        data = CHIP_REG_GET(LAST_STEPCORE1_BASE);
        data |= (0x1 << laststep_num);
        CHIP_REG_SET(LAST_TSCORE1_BASE, ts);
        CHIP_REG_SET(LAST_STEPCORE1_BASE, data);
    }

    point_debug("data = 0x%x\n", data);
}

static void write_save_point(u32 save_num)
{
    u32 ts_ago, ts_now, delta;

    ts_now = sprd_get_aon_sysfrt_cnt();
    ts_ago = CHIP_REG_GET(SPT_PUBLIC_TS_BASE);

    delta = ts_now - ts_ago;

    point_debug("spt_ts_now = 0x%x\n", ts_now);
    point_debug("spt_ts_ago = 0x%x\n", ts_ago);
    point_debug("spt_ts_delta = 0x%x\n", delta);
    point_debug("spt_save_num = 0x%x\n", save_num);

    CHIP_REG_SET(SPT_PUBLIC_TS_BASE, ts_now);
    CHIP_REG_SET(SPT_TS_DELTA_BASE, delta);
    CHIP_REG_SET(SPT_PUBLIC_FLAG_BASE, save_num);
    CHIP_REG_SET(SPT_PUBLIC_FLAG_BASE + 0x4, 0);

}

static void write_16byte_chardesc(char *desc)
{
    char text[16] = {0};
    u32 length;

    length = strlen(desc);
    if (length > 16)
        length = 16;
    memcpy(text,desc,length);
    memcpy((void*)SPT_CHAR_DESCRI_BASE, text, 16);
}

static void point_data_cpy2lk_backup(void)
{
    memcpy((void*)LK_BAKEUP_BASE, (void*)FIRST_TS_BASE, 64);
}

static void clear_region(void)
{
    u32 i, temp_base = FIRST_TS_BASE;

    point_data_cpy2lk_backup();
    memset((void*)FIRST_TS_BASE, 0x0, 0x40);
}

static u32 boot_count(void)
{
    u32 cnt;

    cnt = CHIP_REG_GET(CHIPRAM_BAKEUP_BASE+0xC);
    point_debug("current reboot count is %d\n", cnt);

    return cnt;
}

static void lk_panic_flag_write(u32 flag, u32 addr)
{
    if (flag == 0x1) {    /* lk-self write 1 at start */
        CHIP_REG_SET(CHIPRAM_BAKEUP_BASE+0x4, 0x1);
    }
    else {
        CHIP_REG_SET(CHIPRAM_BAKEUP_BASE+0x4, 0x0);
    }

    CHIP_REG_SET(CHIPRAM_BAKEUP_BASE+0x2C, addr);
}

#endif  /* IRAM DEFINE END */

/*******************************************************/
/*****
*   [Function] FTL_Reboot_count
*   [Description]  acquire reboot count
*   [Input] NA
*   [Return] reboot count
*****/
u32 FTL_Reboot_count(void) 
{
    return boot_count();
}

/*****
*   [Function] FTL_Panic_Flag_Write
*   [Description] read lk panic flag for save lk error log
*   [Input] NA
*   [Return] lk panic flag value
*****/
void FTL_Panic_Flag_Write(u32 flag, u32 addr)
{
    ram_enable();
    lk_panic_flag_write(flag, addr);
}


/*****
*   [Function] FTL_Savepoint_Private
*   [Description] write  step num in fixed ram for marking code order.this interface
*                 only be used for chipram platform owner
*   [Input] laststep:0~31
*   [Return] NA
*****/
static bool first_wr_flag_lk;
void FTL_Savepoint_Private(point_phase laststep)
{

	sprd_spin_lock(&logpoint_lock);
	g_last_step |= (1 << laststep);
	ram_enable();

    /* write first part */
    if (!first_wr_flag_lk) {
		clear_region();         //reboot will clear lk point data
		write_first_step_and_ts();
        first_wr_flag_lk = 1;
    }

    /*  write last part */
    write_last_step_and_ts(laststep);

	sprd_spin_unlock(&logpoint_lock);
}

/*****
*   [Function] FTL_Savepoint_Public
*   [Description] write  step num in fixed ram for marking code order.this interface
*                 only be used for all owner
*   [Input] public_num:1~2^32   str:up to 16 byte
*   [Return] NA
*****/
void FTL_Savepoint_Public(u32 public_num, char *str)
{
	sprd_spin_lock(&logpoint_lock);
    ram_enable();

    write_save_point(public_num);

    /* write pbulic char description */
    write_16byte_chardesc(str);

	sprd_spin_unlock(&logpoint_lock);
}

#else   /* CONFIG_ENABLE_LOGPOINT END */
void FTL_Savepoint_Private(point_phase laststep)
{
    g_last_step |= (1 << laststep);
}
void FTL_Savepoint_Public(u32 public_num, char *str) {}
u32 FTL_Reboot_count(void) { return 0;}
void FTL_Panic_Flag_Write(u32 flag, u32 addr) {}
#endif  //ENABLE_LOGPOINT
