/*
 * AXS touchscreen driver.
 *
 * Copyright (c) 2020-2021 AiXieSheng Technology. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_AXS_CORE_H__
#define __LINUX_AXS_CORE_H__
#include "axs_platform.h"
#include "axs_config.h"
#include <soc/sprd/board.h>

#define AXS_DRIVER_VERSION                  "V2.1.9"
#define AXS_DRIVER_NAME "axs_ts"

#define kfree_safe(pbuf) do {\
    if (pbuf) {\
        kfree(pbuf);\
        pbuf = NULL;\
    }\
} while(0)

#define vfree_safe(pbuf) do {\
    if (pbuf) {\
        vfree(pbuf);\
        pbuf = NULL;\
    }\
} while(0)

/*MAX SUPPORT POINTS*/
#define AXS_MAX_TOUCH_NUMBER    5

/*
#define CTP_BUTTON_KEY_Y   1320
#define MENU_CTP_BUTTON_X  108
#define HOME_CTP_BUTTON_X  324
#define BACK_CTP_BUTTON_X  612
*/
#ifdef ZCFG_LCM_WIDTH
#define SCREEN_MAX_X    ZCFG_LCM_WIDTH
#else
#define SCREEN_MAX_X	600
#endif

#ifdef ZCFG_LCM_HEIGHT
#define SCREEN_MAX_Y    ZCFG_LCM_HEIGHT
#else
#define SCREEN_MAX_Y	1280
#endif

	//#define AXS_GESTURE

#if defined(HAVE_TOUCH_KEY)
#define AXS_MAX_KEYS                        4

#define KEY_ARRAY   \
    {KEY_MENU, 30,50 ,970, 1000},  \
    {KEY_HOMEPAGE, 110, 130, 970, 1000}, \
    {KEY_BACK, 190, 210, 970, 1000}
#endif

#define AXS_ONE_TCH_LEN         6

//POS 0:gesture id;  1:point num;  2:event+X_H;  3:X_L;  4:ID+Y_H;  5:Y_L;  6:WEIGHT;  7:AREA;
#define AXS_MAX_ID                          0x05
#define AXS_TOUCH_GESTURE_POS               0
#define AXS_TOUCH_POINT_NUM                 1
#define AXS_TOUCH_EVENT_POS                 2
#define AXS_TOUCH_X_H_POS                   2
#define AXS_TOUCH_X_L_POS                   3
#define AXS_TOUCH_ID_POS                    4
#define AXS_TOUCH_Y_H_POS                   4
#define AXS_TOUCH_Y_L_POS                   5
#define AXS_TOUCH_WEIGHT_POS                6
#define AXS_TOUCH_AREA_POS                  7

#define AXS_TOUCH_DOWN                      0
#define AXS_TOUCH_UP                        1
#define AXS_TOUCH_CONTACT                   2
#define EVENT_DOWN(flag)                    ((AXS_TOUCH_DOWN == flag) || (AXS_TOUCH_CONTACT == flag))
#define EVENT_UP(flag)                      (AXS_TOUCH_UP == flag)
//#define EVENT_NO_DOWN(data)                 (!data->point_num)

#define AXS_REG_ESD_READ                0X0B
#define AXS_REG_VERSION_READ            0X0C
#define AXS_REG_GESTURE_READ            0X0D
#define AXS_REG_GESTURE_WRITE           0X0E
//AXS_PROXIMITY_SENSOR_EN
#define AXS_REG_PSENSOR_READ            0X10
#define AXS_REG_PSENSOR_WRITE           0X11

#define AXS_FREG_RAWDATA_READ           0X02
#define AXS_FREG_TXRX_NUM_READ          0X03
#define AXS_FREG_DIFF_READ              0X82
#define AXS_FREG_DEBUG_LEN_READ         0X83
#define AXS_FREG_DEBUG_STR_READ         0X84
#define AXS_FREG_TRACE_MODE_WRITE       0X85

// tp exception code
#define RAWDATA_EXCEPTION 0x80
#define SCAN_EXCEPTION 0x40
#define ESD_EXCEPTION 0x20

struct key_data
{
    u16 key;
    u16 x_min;
    u16 x_max;
    u16 y_min;
    u16 y_max;
};

struct axs_ts_platform_data
{
    int irq_gpio;
    int reset_gpio;
    //int power_en_gpio_number;
    const char *vdd_name;
    int TP_MAX_X;
    int TP_MAX_Y;
};

struct ts_event
{
    int x;      /*x coordinate */
    int y;      /*y coordinate */
    int weight;      /* pressure */
    int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
    int id;     /*touch ID */
    int area;
};

struct axs_ts_data
{
    struct i2c_client   *client;
    struct spi_device *spi;
    struct device *dev;
    struct input_dev    *input_dev;
#ifdef ZCFG_MK_TP_PROXIMITY_SHUB
    struct input_dev *ps_input_dev;
#endif
    struct axs_ts_platform_data *pdata;
    struct workqueue_struct *ts_workqueue;
    struct work_struct fwupg_work;
    struct work_struct  pen_event_work;
    struct delayed_work esd_check_work;
    struct delayed_work pen_event_check_work;
#if defined(ZCFG_TP_NOT_USE_SUSPEND_RESUME_NODE)
	struct notifier_block drm_notifier;
#endif
    spinlock_t irq_lock;
//    struct mutex report_mutex;
    struct mutex bus_mutex;
    int irq;
    bool suspended;
    bool irq_disabled;
    u8 gesture_enable;      /* gesture enable or disable, default: disable */
    u8 psensor_enable;      /* proximity sensor enable,default: 0 */
    u8 psensor_state;      /* proximity sensor state,0:away,1:proximity*/
    u8 esd_host_enable;
    u8 pen_event_check_enable;

    struct ts_event events[AXS_MAX_TOUCH_NUMBER];
    u8 *bus_tx_buf;
    u8 *bus_rx_buf;
    u8 *debug_tx_buf;
    u8 *debug_rx_buf;
    int bus_type;
#if AXS_FIRMWARE_LOG_EN
	u8 point_buf[AXS_FIRMWARE_LOG_LEN];
#else
    u8 point_buf[AXS_MAX_TOUCH_NUMBER*AXS_ONE_TCH_LEN+3];
#endif
    u16 pnt_buf_size;
    int touchs;
    int key_state;
    int touch_point;
//  int point_num ;
#if 0//defined(CONFIG_FB)
    struct work_struct resume_work;
    struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_suspend;
#endif
    bool fw_loading;
    u8 tp_no_touch_500ms_count;
    u8 tp_report_interval_500ms;
    struct proc_dir_entry *proc_dir;
    u8 proc_opmode;
};

#if AXS_DEBUG_LOG_EN
extern u8 g_log_en;

#define AXS_DEBUG(fmt, args...) if(g_log_en){ \
   printk("[AXS]%s:"fmt"\n", __func__, ##args); \
}

#define AXS_FUNC_ENTER() if(g_log_en){ \
  printk("[AXS]%s: Enter\n", __func__); \
}

#define AXS_FUNC_EXIT() if(g_log_en){ \
    printk("[AXS]%s: Exit(%d)\n", __func__, __LINE__); \
}

#define AXS_ERROR(fmt, args...) if(g_log_en){ \
    printk(KERN_ERR "[AXS ERROR]%s:"fmt"\n", __func__, ##args); \
}

#else /* #if AXS_DEBUG_LOG_EN*/
#define AXS_DEBUG(fmt, args...)
#define AXS_FUNC_ENTER()
#define AXS_FUNC_EXIT()
#define AXS_ERROR(fmt, args...)
#endif


enum _AXS_BUS_TYPE
{
    BUS_TYPE_NONE,
    BUS_TYPE_I2C,
    BUS_TYPE_SPI,
    BUS_TYPE_SPI_V2,
};

extern struct axs_ts_data *g_axs_data;
// i2c/spi write/read
int axs_write_bytes_read_bytes(u8 *cmd, u16 cmdlen, u8 *data, u16 datalen);
int axs_write_bytes(u8 *writebuf, u16 writelen);
int axs_read_bytes(u8 *rdbuf, u16 rdlen);
#if AXS_BUS_SPI
int axs_write_bytes_read_bytes_onecs(u8 *cmd, u16 cmdlen, u8 *data, u16 datalen);
#endif

int axs_bus_init(struct axs_ts_data *ts_data);
int axs_bus_exit(struct axs_ts_data *ts_data);

#if AXS_GESTURE_EN
bool axs_gesture_init(struct axs_ts_data *ts_data);
void axs_gesture_report(struct input_dev *input_dev, int gesture_id);
int axs_read_gesture_enable(u8 *enable);
int axs_write_gesture_enable(u8 enable);
#endif

int axs_read_fw_version(u8 *ver);

#if AXS_AUTO_UPGRADE_EN
bool axs_fwupg_init(void);
#endif

#if AXS_DEBUG_SYSFS_EN
bool axs_debug_create_sysfs(struct axs_ts_data *ts_data);
int axs_debug_remove_sysfs(struct axs_ts_data *ts_data);
int axs_read_file(char *file_name, u8 **file_buf);
void axs_debug_download_app(char *fw_name);
void axs_debug_fwupg(char *fw_name);
#endif

#if AXS_DEBUG_PROCFS_EN
int axs_create_proc_file(struct axs_ts_data *);
void axs_release_proc_file(struct axs_ts_data *);
#endif

#if AXS_DOWNLOAD_APP_EN
bool axs_download_init(void);
#endif

void axs_reset_proc(int hdelayms);
//int axs_wait_tp_to_valid(void);
void axs_release_all_finger(void);

void axs_irq_disable(void);
void axs_irq_enable(void);

void axs_reset_level(u8 level);

extern int tp_device_id(int id);
extern void ctp_lock_mutex(void);
extern void ctp_unlock_mutex(void);

#if AXS_ESD_CHECK_EN
int axs_esd_check_init(struct axs_ts_data *ts_data);
int axs_esd_check_suspend(void);
int axs_esd_check_resume(void);
//bool axs_esd_error(int exception_code);
void axs_esd_reset_process(void);
#endif
bool axs_fwupg_get_ver_in_tp(u16 *ver);
//buf/reg read/write
int axs_read_regs(u8 *reg, u16 reg_len, u8 *rd_buf, u16 rd_len);
int axs_write_buf(u8 *wt_buf, u16 wt_len);
int axs_read_buf(u8 *rd_buf, u16 rd_len);
#if AXS_PROXIMITY_SENSOR_EN
void axs_report_psensor_state(struct input_dev *input_dev, u8 state);
int axs_read_psensor_enable(u8 *pEnable);
int axs_write_psensor_enable(u8 enable);
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN
int axs_read_psensor_enable(u8 *pEnable);
int axs_write_psensor_enable(u8 enable);
#endif
#endif /* __LINUX_AXS_CORE_H__ */


