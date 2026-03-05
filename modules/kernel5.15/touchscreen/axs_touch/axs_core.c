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

#include "axs_core.h"
#include <soc/sprd/board.h>
#ifdef ZCFG_TP_NOT_USE_SUSPEND_RESUME_NODE
#include <linux/sprd_drm_notifier.h>
int nvt36xx_sysfs_remove_device(struct device *dev);
int nvt36xx_sysfs_add_device(struct device *dev);

#endif

#if  AXS_PROXIMITY_SENSOR_EN
//ioctl cmd
#define FT_IOCTL_MAGIC			0X5D
#define FT_IOCTL_PROX_ON		_IO(FT_IOCTL_MAGIC, 7)
#define FT_IOCTL_PROX_OFF		_IO(FT_IOCTL_MAGIC, 8)


#ifdef ZCFG_USE_INCELL_CTP
extern void incellctp_set_sprd_panel_phone_call_flag(bool enable);
#endif

#endif


#if AXS_PROXIMITY_SENSOR_SHUB_EN
#include <linux/shub_api.h>
static struct wakeup_source *proximity_wake_lock;
static int PROXIMITY_SWITCH =0;
#endif


struct axs_ts_data *g_axs_data=NULL;
u16    g_axs_fw_ver = 0;
#ifdef HAVE_TOUCH_KEY
const struct key_data key_array[]=
{
    KEY_ARRAY
};
#define KEY_NUM   (sizeof(key_array)/sizeof(key_array[0]))
#endif

int axs_read_regs(u8 *reg,u16 reg_len,u8 *rd_buf,u16 rd_len)
{
    return axs_write_bytes_read_bytes(reg,reg_len,rd_buf,rd_len); 
}

int axs_write_buf(u8 *wt_buf,u16 wt_len)
{
    return axs_write_bytes(wt_buf,wt_len);
}

int axs_read_buf(u8 *rd_buf,u16 rd_len)
{
    return axs_read_bytes(rd_buf,rd_len);
}

int axs_read_fw_version(u8 *ver)
{
    u8 cmd_type[1] = {AXS_REG_VERSION_READ};
    return axs_read_regs(cmd_type, 1, ver, 2);
}

bool axs_fwupg_get_ver_in_tp(u16 *ver)
{
    int ret = 0;
    u8 val[2] = { 0 };

    ret = axs_read_fw_version(&val[0]);
    if (ret < 0)
    {
        AXS_DEBUG("get tp fw invaild");
        return false;
    }
    *ver = val[0]<<8|val[1];
    return true;
}

void axs_reset_level(u8 level)
{
    if (g_axs_data->pdata)
    {

        gpio_direction_output(g_axs_data->pdata->reset_gpio, level);
        //axs_gpio_output(g_axs_data->pdata->reset_gpio, level);
    }
}
void axs_reset_proc(int hdelayms)
{
    struct axs_ts_platform_data *pdata = g_axs_data->pdata;
    AXS_DEBUG("axs_reset_proc");
    if (pdata)
    {
        gpio_direction_output(pdata->reset_gpio, 1);
        msleep(1);
        gpio_set_value(pdata->reset_gpio, 0);
        msleep(10);
        gpio_set_value(pdata->reset_gpio, 1);
        if (hdelayms)
            msleep(hdelayms);
    }
    else
    {
        AXS_DEBUG("axs_reset_proc fail!");
    }
}


#if AXS_PROXIMITY_SENSOR_SHUB_EN

#ifdef ZCFG_USE_INCELL_CTP
extern void incellctp_set_sprd_panel_phone_call_flag(bool enable);
#endif

static ssize_t do_proximity_enable_store(unsigned long on_off ,size_t size)
{

    struct axs_ts_data *ts = g_axs_data;

    printk("do_proximity_enable_store %d!\n", on_off);
    if(1 == on_off){
        ts->psensor_enable = 1;
        axs_write_psensor_enable(0x01);
        __pm_stay_awake(proximity_wake_lock);
    }else if(0 == on_off){
        ts->psensor_enable = 0;
        axs_write_psensor_enable(0x00);
        __pm_relax(proximity_wake_lock);
    }else{
        return -EINVAL;
    }

    return size ;
}

static int shub_report_proximity_event(u32 value)
{
  s64 k_timestamp;
  struct shub_event_params event;
  struct axs_ts_data *ts = g_axs_data;

  k_timestamp = ktime_to_us(ktime_get_boottime());

  event.Cmd = 129;
  event.HandleID = 108;
  event.fdata[0] = value;
  event.fdata[1] = 0;
  event.fdata[2] = 0;
  event.Length = sizeof(struct shub_event_params);
  event.timestamp = k_timestamp;

//  dev_info(&g_sensor->sensor_pdev->dev,
  dev_info(&ts->ps_input_dev->dev,
      "sizeof(struct shub_event_params = %zu\n",
      sizeof(struct shub_event_params));
  peri_send_sensor_event_to_iio((u8 *)&event,
      sizeof(struct shub_event_params));

  return 0;
}

static int shub_report_proximity_flush_event(u32 value)
{
  s64 k_timestamp;
  struct shub_event_params event;
  struct axs_ts_data *ts = g_axs_data;

  k_timestamp = ktime_to_us(ktime_get_boottime());

  event.Cmd = 130;//flush
  event.HandleID = 108;
  event.fdata[0] = value;
  event.fdata[1] = 0;
  event.fdata[2] = 0;
  event.Length = sizeof(struct shub_event_params);
  event.timestamp = k_timestamp;

//  dev_info(&g_sensor->sensor_pdev->dev,
  dev_info(&ts->ps_input_dev->dev,
      "sizeof(struct shub_event_params = %zu\n",
      sizeof(struct shub_event_params));
  peri_send_sensor_event_to_iio((u8 *)&event,
      sizeof(struct shub_event_params));

  return 0;
}




static ssize_t show_proximity_sensor(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("show_proximity_sensor buf==%d\n", PROXIMITY_SWITCH);
	return sprintf(buf, "tp proximity enable = %d\n", PROXIMITY_SWITCH);
}

static ssize_t store_proximity_sensor(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ps_enable = 0;
	sscanf(buf, "%d\n",&ps_enable);
    printk("%s sys control tp face mode! ps_enable = %d\n", __func__, ps_enable);//debug
    do_proximity_enable_store(ps_enable,0);
    return count;
}

static DEVICE_ATTR(psensor_enable, 0644, show_proximity_sensor, store_proximity_sensor);

static ssize_t proximity_sensor_flush_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    printk("buf==%d\n", (int)*buf);
    return sprintf(buf, "tp psensor show\n");

}

static ssize_t proximity_sensor_fulsh_store(struct device *dev,
    struct device_attribute *attr,
    const char *buf, size_t count)
{
  //int ps_enable = 0;
  //sscanf(buf, "%d\n",&ps_enable);
  shub_report_proximity_flush_event(0);
  return count;
}

static DEVICE_ATTR(psensor_flush, 0644, proximity_sensor_flush_show, proximity_sensor_fulsh_store);


#endif

static int axs_input_init(struct axs_ts_data *ts_data)
{
    int ret = 0;
#ifdef HAVE_TOUCH_KEY
    int i = 0;
#endif
#if AXS_PROXIMITY_SENSOR_SHUB_EN
    struct input_dev *ps_input_dev;
    struct class *psensor_class;
    struct device *psensor_dev;
#endif
    struct input_dev *input_dev;
    input_dev = input_allocate_device();
    if (!input_dev)
    {
        AXS_ERROR("Failed to allocate memory for input device");
        return  -ENOMEM;
    }
    g_axs_data->input_dev = input_dev;

    input_dev->dev.parent = ts_data->dev;
    input_dev->name = AXS_DRIVER_NAME;      //dev_name(&client->dev)
    if (ts_data->bus_type == BUS_TYPE_I2C)
        input_dev->id.bustype = BUS_I2C;
    else
        input_dev->id.bustype = BUS_SPI;
    set_bit(EV_SYN, input_dev->evbit);
    set_bit(EV_ABS, input_dev->evbit);
    set_bit(EV_KEY, input_dev->evbit);
    set_bit(BTN_TOUCH, input_dev->keybit);
    set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
#if AXS_MT_PROTOCOL_B_EN
    set_bit(BTN_TOOL_FINGER, input_dev->keybit);
    set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
    input_mt_init_slots(input_dev, AXS_MAX_TOUCH_NUMBER, INPUT_MT_DIRECT);
#else
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, (AXS_MAX_TOUCH_NUMBER - 1), 0, 0);
#endif

    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if AXS_REPORT_PRESSURE_EN
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif
#if  AXS_PROXIMITY_SENSOR_EN 
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN
  ps_input_dev = input_allocate_device();
    if(!ps_input_dev){
        ret = -ENOMEM;
        AXS_ERROR("error::failed to allocate ps-input device\n");
        return ret;
    }
    ts_data->ps_input_dev = ps_input_dev;
    ps_input_dev->name = "alps_pxy";
    set_bit(EV_ABS, ps_input_dev->evbit);
    input_set_capability(ps_input_dev, EV_ABS, ABS_DISTANCE); 
    input_set_abs_params(ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);
    ret = input_register_device(ps_input_dev);
    if (ret) {
        AXS_ERROR("failed to register input device\n");
        return ret;
    }
    
    psensor_class = class_create(THIS_MODULE,"sprd_sensorhub_tp");
	if(IS_ERR(psensor_class))
		printk("Failed to create class!\n");

	psensor_dev = device_create(psensor_class, NULL, 0, NULL, "device");
	if(IS_ERR(psensor_dev))
		printk("Failed to create device!\n");

	if(device_create_file(psensor_dev, &dev_attr_psensor_enable) < 0) // /sys/class/sprd_sensorhub_tp/device/psensor_enable
		printk("Failed to create device file(%s)!\n", dev_attr_psensor_enable.attr.name);

	if(device_create_file(psensor_dev, &dev_attr_psensor_flush) < 0) // /sys/class/sprd_sensorhub_tp/device/psensor_enable
		printk("Failed to create device file(%s)!\n", dev_attr_psensor_enable.attr.name);

    //wakeup_source_init(&proximity_wake_lock, "prox_delayed_work");
	proximity_wake_lock=wakeup_source_register(NULL, "proximity_wake_lock");
	if (!proximity_wake_lock) {
		return -ENOMEM;
	}
#endif

#ifdef HAVE_TOUCH_KEY
    for (i = 0; i < KEY_NUM; i++)
    {
        input_set_capability(input_dev, EV_KEY, key_array[i].key);
    }
#endif

    ret = input_register_device(input_dev);
    if (ret)
    {
        AXS_ERROR("Input device registration failed");
        input_free_device(input_dev);
        input_dev = NULL;
        return ret;
    }

    ts_data->input_dev = input_dev;
    return 0;
}

static int axs_read_touchdata(struct axs_ts_data *data)
{
    int ret = 0;
    u8 *buf = data->point_buf;
#if !AXS_FIRMWARE_LOG_EN
    memset(buf, 0xFF, data->pnt_buf_size);
#endif
    //buf[0] = 0x0;
    ret = axs_read_buf(buf, data->pnt_buf_size);
    if (ret < 0)
    {
        AXS_ERROR("axs_read_buf failed, ret:%d", ret);
        return ret;
    }
    return 0;
}
#if AXS_FIRMWARE_LOG_EN
u16 log_index = 0;
void axs_output_firmware_log(u8 *log_buf)
{
	u8 i;
    u16 log_len = (log_buf[0]<<8)|log_buf[1];
	log_index ++;
    printk("touchdata:");
    for(i = 0; i < 32; i++){
        printk( "0x%02x,",buf[i]);
    }
    printk( "\n");

	printk( "log index:%d,log len:%d\n",log_counter,log_len);
	if(log_len>0)
	{
		printk("%s",&log_buf[2]);
	}
}
#endif
/*
error:return < 0
point:return 0
gesture/esd/psensor:return > 0
*/
static int axs_read_parse_touchdata(struct axs_ts_data *data)
{
    int ret = 0;
    int i = 0;
    int point_num;
    u8 pointid = 0;
    int base = 0;
    struct ts_event *events = NULL;
    u8 *buf  = NULL;
    if (!data)
    {
        AXS_DEBUG("!data ret");
        return -1;
    }
    buf = data->point_buf;
    events = data->events;
    ret = axs_read_touchdata(data);
    if (ret< 0)
    {
        AXS_DEBUG("axs_read_touchdata failed, ret:%d", ret);
        return ret;
    }
#if AXS_FIRMWARE_LOG_EN
	buf[AXS_FIRMWARE_LOG_LEN-1] = 0;
	axs_output_firmware_log(&buf[AXS_MAX_TOUCH_NUMBER * AXS_ONE_TCH_LEN + 2]);
#endif

//#if AXS_GESTURE_EN
//if (data->gesture_enable){
    if (0 != buf[AXS_TOUCH_GESTURE_POS])
    {
        AXS_DEBUG("succuss to get gesture data in irq handler");
        return buf[AXS_TOUCH_GESTURE_POS];
    }
//}
//#endif
    point_num = buf[AXS_TOUCH_POINT_NUM] & 0x0F;
    data->touch_point = 0;
//#if AXS_ESD_CHECK_EN
	if(buf[AXS_TOUCH_POINT_NUM]>>4) // report esd normal data :[0,0xff,0xff,0xff,0xff......]
	{
        AXS_DEBUG("report normal esd status,buf[1]=(%d)",buf[AXS_TOUCH_POINT_NUM]);
        return -1;
	}
//#endif
	if (point_num > AXS_MAX_TOUCH_NUMBER)
    {
        AXS_ERROR("invalid point_num(%d)", point_num);
        point_num = AXS_MAX_TOUCH_NUMBER;
        //return -1;
    }
    AXS_DEBUG("point_num=%d\n",point_num);
    for (i = 0; i < point_num; i++)   //AXS_MAX_TOUCH_NUMBER
    {
        base = AXS_ONE_TCH_LEN * i;
        pointid = (buf[AXS_TOUCH_ID_POS + base]) >> 4;
        if (pointid >= AXS_MAX_ID)
        {
            AXS_ERROR("pointid(%d) beyond AXS_MAX_ID", pointid);
            break;
        }
        else if (pointid >= AXS_MAX_TOUCH_NUMBER)
        {
            AXS_ERROR("pointid(%d) beyond max_touch_number", pointid);
            return -1;
        }

        data->touch_point++;
        events[i].x = ((buf[AXS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
                      (buf[AXS_TOUCH_X_L_POS + base]);
        events[i].y = ((buf[AXS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
                      (buf[AXS_TOUCH_Y_L_POS + base]);
        events[i].flag = buf[AXS_TOUCH_EVENT_POS + base] >> 6;
        events[i].id = buf[AXS_TOUCH_ID_POS + base] >> 4;
        events[i].area = buf[AXS_TOUCH_AREA_POS + base] >> 4;
        events[i].weight =  buf[AXS_TOUCH_WEIGHT_POS + base];

        if (EVENT_DOWN(events[i].flag) && (point_num == 0))
        {
            AXS_ERROR("abnormal touch data from fw");
            return -1;
        }
    }

    if (data->touch_point == 0)
    {
        AXS_ERROR("no touch point information");
        return -1;
    }
    return 0;
}

void axs_release_all_finger(void)
{
    struct input_dev *input_dev = g_axs_data->input_dev;
#if AXS_MT_PROTOCOL_B_EN
    u32 finger_count = 0;
    u32 max_touches = AXS_MAX_TOUCH_NUMBER;
#endif

    AXS_DEBUG("release all app point");
//    mutex_lock(&g_axs_data->report_mutex);
#if AXS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < max_touches; finger_count++)
    {
        input_mt_slot(input_dev, finger_count);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_sync(input_dev);
#else
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_mt_sync(input_dev);
    input_sync(input_dev);
#endif
    g_axs_data->touchs = 0;
    g_axs_data->touch_point = 0;
    g_axs_data->key_state = 0;
//    mutex_unlock(&g_axs_data->report_mutex);
}

#ifdef HAVE_TOUCH_KEY
static int axs_input_report_key(struct axs_ts_data *data, int index)
{
    int i = 0;
    int x = data->events[index].x;
    int y = data->events[index].y;

    for (i = 0; i < KEY_NUM; i++)
    {
        if ((x > key_array[i].x_min) && (x < key_array[i].x_max) &&
            (y > key_array[i].y_min) && (y < key_array[i].y_max))
        {
            if (EVENT_DOWN(data->events[index].flag)
                && !(data->key_state & (1 << i)))
            {
                input_report_key(data->input_dev, key_array[i].key, 1);
                data->key_state |= (1 << i);
                AXS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
            }
            else if (EVENT_UP(data->events[index].flag)
                     && (data->key_state & (1 << i)))
            {
                input_report_key(data->input_dev, key_array[i].key, 0);
                data->key_state &= ~(1 << i);
                AXS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
            }
            return 0;
        }
    }
    return -1;
}
#endif


#if AXS_PEN_EVENT_CHECK_EN
int axs_pen_event_check_suspend(void)
{
    AXS_FUNC_ENTER();
    if (g_axs_data->pen_event_check_enable)
    {
        cancel_delayed_work(&g_axs_data->pen_event_check_work);
    }
    AXS_FUNC_EXIT();
    return 0;
}
int axs_pen_event_check_resume(void)
{
    AXS_FUNC_ENTER();
    if (g_axs_data->pen_event_check_enable)
    {
        queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->pen_event_check_work,
                           msecs_to_jiffies(500));
    }
    AXS_FUNC_EXIT();
    return 0;
}

void axs_pen_event_check_func(struct work_struct *work)
{
    if (g_axs_data->tp_report_interval_500ms <= 10)
    {
        g_axs_data->tp_report_interval_500ms += 1;
    }
    AXS_DEBUG("interval_500ms:%d,touchs:%d", g_axs_data->tp_report_interval_500ms,g_axs_data->touchs);

    if ((g_axs_data->tp_report_interval_500ms >= 2) && (g_axs_data->touchs != 0)) // max wait time:1s
    {
        int i = 0;
        for (i = 0; i < AXS_MAX_TOUCH_NUMBER; i++)
        {
            if (BIT(i) & (g_axs_data->touchs))
            {
                AXS_ERROR("missed P%d UP!", i);
                input_mt_slot(g_axs_data->input_dev, i);
                input_mt_report_slot_state(g_axs_data->input_dev, MT_TOOL_FINGER, false);
                g_axs_data->touchs &= ~BIT(i);
            }
        }
        input_report_key(g_axs_data->input_dev, BTN_TOUCH, 0);
        input_sync(g_axs_data->input_dev);
    }

    queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->pen_event_check_work,
                       msecs_to_jiffies(500));

}
int axs_pen_event_check_init(struct axs_ts_data *ts_data)
{
    AXS_FUNC_ENTER();

    if (ts_data->ts_workqueue)
    {
        INIT_DELAYED_WORK(&ts_data->pen_event_check_work, axs_pen_event_check_func);
    }
    else
    {
        AXS_ERROR("ts_workqueue is NULL");
        return -EINVAL;
    }

    g_axs_data->pen_event_check_enable = 1;

    queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->pen_event_check_work,
                       msecs_to_jiffies(500));
    AXS_FUNC_EXIT();
    return 0;
}

#endif


#if AXS_MT_PROTOCOL_B_EN
static int axs_input_report_b(struct axs_ts_data *data)
{
    int i = 0;
    int down_points = 0;
    int up_points = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 max_touch_num = AXS_MAX_TOUCH_NUMBER;

    struct ts_event *events = data->events;
    AXS_DEBUG("[B] report touch begin, point:%2x",data->touch_point);
    for (i = 0; i < data->touch_point; i++)
    {
#ifdef HAVE_TOUCH_KEY
        if (axs_input_report_key(data, i) == 0)
        {
            continue;
        }
#endif
        AXS_DEBUG("P%d [touchs:%x]  !", events[i].id,data->touchs);
        va_reported = true;
        input_mt_slot(data->input_dev, events[i].id);
        if (EVENT_DOWN(events[i].flag))
        {
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if AXS_REPORT_PRESSURE_EN
            if (events[i].weight <= 0)
            {
                events[i].weight = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].weight);
#endif
            if (events[i].area <= 0)
            {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            touchs |= BIT(events[i].id);
            data->touchs |= BIT(events[i].id);

            AXS_DEBUG("[B]P%d(%d, %d)[weight:%d,area:%d] DOWN!",
                      events[i].id,
                      events[i].x, events[i].y,
                      events[i].weight, events[i].area);
            AXS_DEBUG("P%d [touchs:%x]  !", events[i].id,data->touchs);
            down_points++;
        }
        else
        {
            up_points++;
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            data->touchs &= ~BIT(events[i].id);
            AXS_DEBUG("[B]P%d UP!", events[i].id);
        }
    }

    if (unlikely(data->touchs ^ touchs))
    {
        for (i = 0; i < max_touch_num; i++)
        {
            if (BIT(i) & (data->touchs ^ touchs))
            {
                AXS_ERROR("P%d UP, Reup missed point !", i);
                va_reported = true;
                input_mt_slot(data->input_dev, i);
                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            }
        }
    }
    data->touchs = touchs;

    if (va_reported)
    {
        /* touchs==0, there's no point but key */
        if (0 == down_points)
        {
            AXS_DEBUG("[B] BTN_TOUCH UP!");
            input_report_key(data->input_dev, BTN_TOUCH, 0);
        }
        else
        {
            AXS_DEBUG("[B] BTN_TOUCH DOWN!");
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }
    AXS_DEBUG("[B] report touch end,data->touchs:%x",data->touchs);

    input_sync(data->input_dev);
    return 0;
}

#else
static int axs_input_report_a(struct axs_ts_data *data)
{
    int i = 0;
    int down_points = 0;
    int up_points = 0;
    bool va_reported = false;
    struct ts_event *events = data->events;
    //AXS_DEBUG(KERN_EMERG "axs_input_report_a touch_point:%d\n",data->touch_point);
    for (i = 0; i < data->touch_point; i++)
    {
#ifdef HAVE_TOUCH_KEY
        if (axs_input_report_key(data, i) == 0)
        {
            continue;
        }
#endif
        va_reported = true;
        //AXS_DEBUG("axs_input_report_a events[i].flag:%d\n",events[i].flag);
        if (EVENT_DOWN(events[i].flag))
        {
            input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if AXS_REPORT_PRESSURE_EN
            if (events[i].weight <= 0)
            {
                events[i].weight = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].weight);
#endif
            if (events[i].area <= 0)
            {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            input_mt_sync(data->input_dev);

            AXS_DEBUG("[A]P%d(%d, %d)[weight:%d,area:%d] DOWN!",
                      events[i].id,
                      events[i].x, events[i].y,
                      events[i].weight, events[i].area);
            down_points++;
        }
        else
        {
            up_points++;
        }
    }

    /* last point down, current no point but key */
    if (data->touchs && !down_points)
    {
        va_reported = true;
    }
    data->touchs = down_points;

    if (va_reported)
    {
        if (0 == down_points)
        {
            AXS_DEBUG("[A]Points All Up!");
            input_report_key(data->input_dev, BTN_TOUCH, 0);
            input_mt_sync(data->input_dev);
        }
        else
        {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}
#endif

/*int axs_wait_tp_to_valid(void)
{
    msleep(2);
    return 0;
}*/
#if AXS_INTERRUPT_THREAD
static void axs_ts_report_thread_handler(void)
{
    int ret = 0;
    struct axs_ts_data *ts_data = g_axs_data;
#if  AXS_PROXIMITY_SENSOR_EN || AXS_GESTURE_EN
	struct input_dev *input_dev = ts_data->input_dev;
#endif
    //AXS_DEBUG("TP interrupt ");
#if AXS_ESD_CHECK_EN
    g_axs_data->tp_no_touch_500ms_count = 0;
#endif
#if AXS_PEN_EVENT_CHECK_EN
    g_axs_data->tp_report_interval_500ms = 0;
#endif
#if AXS_PROXIMITY_SENSOR_EN
    if (ts_data->psensor_enable) {
        axs_write_psensor_enable(1);
        printk("axs_write_psensor_enable\n");
    }
#endif

#if AXS_BUS_IIC
#if AXS_PROXIMITY_SENSOR_SHUB_EN
    if (ts_data->psensor_enable) {
        axs_write_psensor_enable(1);
        printk("axs_write_psensor_enable\n");
    }
#endif
#endif
    ret = axs_read_parse_touchdata(ts_data);
    printk("axs_ts_report_thread_handler ret = %d \n",ret);
    if (ret > 0)
    {
		
#if AXS_ESD_CHECK_EN
        if (g_axs_data->esd_host_enable)
        {
            if (ret&ESD_EXCEPTION) /*bit 5-7,ret&0xe0*/
            {
                axs_esd_reset_process();
                axs_release_all_finger();
            }
        }
#endif
#if AXS_PROXIMITY_SENSOR_EN
		if (g_axs_data->psensor_enable)
		{
			
			if (ret & 0x10) /*bit 4*/
			{
				if(!g_axs_data->psensor_state)
				{
					g_axs_data->psensor_state = 1;	
				}
			}
			else
			{
				if(g_axs_data->psensor_state)
				{
					g_axs_data->psensor_state = 0;	
				}			
			}
			axs_report_psensor_state(input_dev,g_axs_data->psensor_state);
		}
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN
		if (g_axs_data->psensor_enable)
		{
			
			if (ret & 0x10) /*bit 4*/
			{
				if(!g_axs_data->psensor_state)
				{
					g_axs_data->psensor_state = 1;	
                    shub_report_proximity_event(0);
				}
			}
			else
			{
				if(g_axs_data->psensor_state)
				{
					g_axs_data->psensor_state = 0;
                    shub_report_proximity_event(0x40a00000);
				}			
			}
            
		}
#endif

#if AXS_GESTURE_EN
        if (ts_data->gesture_enable)
        {
            axs_gesture_report(input_dev, ret&0x0f);
        }
#endif
    }
    else if (ret == 0)
    {
//      mutex_lock(&ts_data->report_mutex);
#if AXS_MT_PROTOCOL_B_EN
        axs_input_report_b(ts_data);
#else
        axs_input_report_a(ts_data);
#endif
//      mutex_unlock(&ts_data->report_mutex);
    }
#if AXS_PROXIMITY_SENSOR_EN
	else
	{
			if(g_axs_data->psensor_state)
			{
				g_axs_data->psensor_state = 0;	
				axs_report_psensor_state(input_dev,g_axs_data->psensor_state);
			}	
	}
#endif
#if AXS_PROXIMITY_SENSOR_SHUB_EN
	else
	{
			if(g_axs_data->psensor_state)
			{
				g_axs_data->psensor_state = 0;	
				shub_report_proximity_event(0x40a00000);
			}	
	}
#endif	
}
#else
static void axs_ts_worker(struct work_struct *work)
{
    int ret = 0;
    struct axs_ts_data *ts_data = g_axs_data;
#if  AXS_PROXIMITY_SENSOR_EN || AXS_GESTURE_EN
	struct input_dev *input_dev = ts_data->input_dev;
#endif
#if AXS_ESD_CHECK_EN
    g_axs_data->tp_no_touch_500ms_count = 0;
#endif
#if AXS_PEN_EVENT_CHECK_EN
    g_axs_data->tp_report_interval_500ms = 0;
#endif
    ret = axs_read_parse_touchdata(ts_data);
    if (ret > 0)
    {
#if AXS_ESD_CHECK_EN
        if (g_axs_data->esd_host_enable)
        {
            if (ret&ESD_EXCEPTION) /*bit 5-7,ret&0xe0*/
            {
                axs_esd_reset_process();
                axs_release_all_finger();
            }
        }
#endif
#if AXS_PROXIMITY_SENSOR_EN
		if (g_axs_data->psensor_enable)
		{
			if (ret & 0x10) //bit 4
			{
				if(!g_axs_data->psensor_state)
				{
					g_axs_data->psensor_state = 1;	
				}
			}
			else
			{
				if(g_axs_data->psensor_state)
				{
					g_axs_data->psensor_state = 0;	
				}			
			}
			axs_report_psensor_state(input_dev,g_axs_data->psensor_state);
		}
#endif    
#if AXS_GESTURE_EN
        if (ts_data->gesture_enable)
        {
            axs_gesture_report(input_dev, ret&0x0f);
        }
#endif
    }
    else if (ret == 0)
    {
//        mutex_lock(&ts_data->report_mutex);
#if AXS_MT_PROTOCOL_B_EN
        axs_input_report_b(ts_data);
#else
        axs_input_report_a(ts_data);
#endif
//        mutex_unlock(&ts_data->report_mutex);
    }
#if AXS_PROXIMITY_SENSOR_EN
	else
	{
			if(g_axs_data->psensor_state)
			{
				g_axs_data->psensor_state = 0;	
				axs_report_psensor_state(input_dev,g_axs_data->psensor_state);
			}
	}
#endif	
    enable_irq(g_axs_data->irq);
}
#endif


static irqreturn_t axs_ts_interrupt(int irq, void *dev_id)
{
#if AXS_INTERRUPT_THREAD
    //AXS_DEBUG("axs_ts_interrupt");
    /*avoid writing i2c  during the upgrade time*/
    if (g_axs_data->fw_loading)
    {
        return IRQ_HANDLED;
    }
    axs_ts_report_thread_handler();
    return IRQ_HANDLED;
#else
    struct axs_ts_data *ts_data = (struct axs_ts_data *)dev_id;
    /*avoid writing i2c  during the upgrade time*/
    if (g_axs_data->fw_loading)
    {
        return IRQ_HANDLED;
    }
    disable_irq_nosync(g_axs_data->irq);
    if (!work_pending(&ts_data->pen_event_work))
    {
        queue_work(ts_data->ts_workqueue, &ts_data->pen_event_work);
    }
    return IRQ_HANDLED;
#endif
}

void axs_irq_disable(void)
{
    unsigned long irqflags;

    spin_lock_irqsave(&g_axs_data->irq_lock, irqflags);

    if (!g_axs_data->irq_disabled)
    {
        disable_irq_nosync(g_axs_data->irq);
        g_axs_data->irq_disabled = true;
    }

    spin_unlock_irqrestore(&g_axs_data->irq_lock, irqflags);
}

void axs_irq_enable(void)
{
    unsigned long irqflags = 0;

    spin_lock_irqsave(&g_axs_data->irq_lock, irqflags);

    if (g_axs_data->irq_disabled)
    {
        enable_irq(g_axs_data->irq);
        g_axs_data->irq_disabled = false;
    }

    spin_unlock_irqrestore(&g_axs_data->irq_lock, irqflags);
}
#if AXS_DOWNLOAD_APP_EN
bool power_key = true;
#endif
#if 1//defined(CONFIG_PM_SLEEP)
int axs_ts_suspend(struct device *dev)
{
    struct axs_ts_data *ts_data = g_axs_data;
    printk("axs_ts_suspend begin \n");
	ts_data->suspended = true;

#if AXS_DOWNLOAD_APP_EN    
    power_key = false;
#endif
#if AXS_PEN_EVENT_CHECK_EN
    axs_pen_event_check_suspend();
#endif

    if (ts_data->fw_loading)
    {
        AXS_DEBUG("tp in upgrade state, can't suspend");
        return 0;
    }
	
#if AXS_PROXIMITY_SENSOR_EN
    if (ts_data->psensor_enable) {
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(1);
        #endif
        axs_write_psensor_enable(1);
        printk("axs_ts_suspend psensor_enable\n");
        return 0;
    }else{
        axs_write_psensor_enable(0);
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(0);
        #endif
    }
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN
    if (ts_data->psensor_enable) {
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(1);
        #endif
        axs_write_psensor_enable(1);
        printk("axs_ts_suspend psensor_enable\n");
        return 0;
    }else{
        axs_write_psensor_enable(0);
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(0);
        #endif
    }
#endif

	axs_reset_level(1);
#if AXS_GESTURE_EN
	if(ts_data->gesture_enable)
	{
		axs_write_gesture_enable(1);
		axs_release_all_finger();

		return 0;
	}
#endif
#if AXS_ESD_CHECK_EN
	axs_esd_check_suspend();
#endif
    axs_irq_disable();
    axs_release_all_finger();
#if AXS_DOWNLOAD_APP_EN
    power_key = true;
#endif
    //msleep(100);
    printk("axs_ts_suspend end");
    return 0;
}

#if AXS_POWERUP_CHECK_EN
void axs_powerup_check(void)
{
    int ret = 0;
    u8 val[2] = { 0 };
    ret = axs_read_fw_version(&val[0]);
    if (ret < 0)
    {
        AXS_DEBUG("axs_read_fw_version fail");
        axs_reset_proc(20);
    }
}
#endif
int axs_ts_resume(struct device *dev)
{
    struct axs_ts_data *ts_data = g_axs_data;
    printk("axs_ts_resume begin");
    ts_data->suspended = false;

    axs_release_all_finger();
#if AXS_DOWNLOAD_APP_EN

#if AXS_PROXIMITY_SENSOR_EN
    if ((!ts_data->psensor_enable) || power_key) {
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN
    if ((!ts_data->psensor_enable) || power_key) {
#endif 
        printk("axs_ts_resume axs_download_init enter");
        if (!axs_download_init())
        {
            AXS_ERROR("axs_download_init fail!\n");
        }
#if AXS_PROXIMITY_SENSOR_EN
    }
#endif
#if AXS_PROXIMITY_SENSOR_SHUB_EN
    }
#endif
#else
    axs_reset_proc(20);
#if AXS_POWERUP_CHECK_EN
    axs_powerup_check();
#endif
#endif

#if AXS_ESD_CHECK_EN
    axs_esd_check_resume();
#endif
#if AXS_PEN_EVENT_CHECK_EN
    axs_pen_event_check_resume();
#endif

    axs_irq_enable();
#if AXS_PROXIMITY_SENSOR_EN
    if (ts_data->psensor_enable) {
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(1);
        #endif
#if AXS_DOWNLOAD_APP_EN
        if(!power_key)
#endif
            axs_write_psensor_enable(1);
        printk("axs_ts_resume psensor_enable\n");
    }else{
        axs_write_psensor_enable(0);
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(0);
        #endif
    }
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN
    if (ts_data->psensor_enable) {
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(1);
        #endif
#if AXS_DOWNLOAD_APP_EN
        if(!power_key)
#endif
            axs_write_psensor_enable(1);
        printk("axs_ts_resume psensor_enable\n");
    }else{
        axs_write_psensor_enable(0);
        #ifdef ZCFG_USE_INCELL_CTP
        incellctp_set_sprd_panel_phone_call_flag(0);
        #endif
    }
#endif
    
    printk("axs_ts_resume end");
    return 0;
}
#endif


#if defined(ZCFG_TP_NOT_USE_SUSPEND_RESUME_NODE)
static int jx_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{

   printk("waterworld  event = %d", event);
   if (event == DISPC_POWER_OFF) {
        axs_ts_suspend(g_axs_data->dev);
		printk("DISPC_POWER_OFF\n");
   }else if (event == DISPC_POWER_ON) {
        axs_ts_resume(g_axs_data->dev);
		printk("DISPC_POWER_ON\n");
	}
   else
       printk("waterworld wrong event");
    return 0;
}
#endif 

#if 0//defined(CONFIG_FB)
static void axs_resume_work(struct work_struct *work)
{
    axs_ts_resume(g_axs_data->dev);
}

static int axs_fb_notifier_callback(struct notifier_block *self,
                                    unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank = NULL;
    if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK))
    {
        AXS_DEBUG("event(%lu) do not need process\n", event);
        return 0;
    }

    blank = evdata->data;
    AXS_DEBUG("FB event:%lu,blank:%d", event, *blank);
    switch (*blank)
    {
        case FB_BLANK_UNBLANK:
            if (FB_EARLY_EVENT_BLANK == event)
            {
                AXS_DEBUG("resume: event = %lu, not care\n", event);
            }
            else if (FB_EVENT_BLANK == event)
            {
                queue_work(g_axs_data->ts_workqueue, &g_axs_data->resume_work);
            }
            break;
        case FB_BLANK_POWERDOWN:
            if (FB_EARLY_EVENT_BLANK == event)
            {
                cancel_work_sync(&g_axs_data->resume_work);
                axs_ts_suspend(g_axs_data->dev);
            }
            else if (FB_EVENT_BLANK == event)
            {
                AXS_DEBUG("suspend: event = %lu, not care\n", event);
            }
            break;
        default:
            AXS_DEBUG("FB BLANK(%d) do not need process\n", *blank);
            break;
    }

    return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void axs_ts_early_suspend(struct early_suspend *handler)
{
    axs_ts_suspend(g_axs_data->dev);
}

static void axs_ts_late_resume(struct early_suspend *handler)
{

    axs_ts_resume(g_axs_data->dev);
}
#endif


static void axs_ts_hw_init(struct axs_ts_data *ts_data)
{
#if AXS_POWER_SOURCE_CUST_EN
    struct regulator *reg_vdd =  NULL;
#endif
    struct axs_ts_platform_data *pdata = ts_data->pdata;
    AXS_DEBUG("[irq=%d];[rst=%d]\n",pdata->irq_gpio,pdata->reset_gpio);
    gpio_request(pdata->irq_gpio, "ts_irq_pin");
    gpio_request(pdata->reset_gpio, "ts_rst_pin");
    gpio_direction_output(pdata->reset_gpio, 1);
    gpio_direction_input(pdata->irq_gpio);

    //ts_data->irq = gpio_to_irq(pdata->irq_gpio);

#if AXS_POWER_SOURCE_CUST_EN
    reg_vdd = regulator_get(&client->dev, pdata->vdd_name);
    regulator_set_voltage(reg_vdd, 2800000, 2800000);
    regulator_enable(reg_vdd);
#endif
    //msleep(100);
    axs_reset_proc(120);
}

#ifdef CONFIG_OF
static  bool axs_platform_data_init(struct axs_ts_data *ts_data,struct device *dev)
{
    struct axs_ts_platform_data *pdata = ts_data->pdata;
    struct device_node *np = dev->of_node;
#if AXS_POWER_SOURCE_CUST_EN
    int ret;
#endif
    if (!np)
    {
        AXS_ERROR("axs_platform_data_init !np");
        return false;
    }
    pdata->reset_gpio = of_get_named_gpio(np, "touch,reset-gpio", 0);

    //pdata->reset_gpio = of_get_gpio(np, 0);
    if (!gpio_is_valid(pdata->reset_gpio))
    {
        AXS_ERROR("Parse RST GPIO from dt failed %d", pdata->reset_gpio);
        pdata->reset_gpio = -1;
        return false;
    }
    AXS_DEBUG("  %-12s: %d", "rst gpio", pdata->reset_gpio);
    pdata->irq_gpio = of_get_named_gpio(np, "touch,irq-gpio", 0);
    //pdata->irq_gpio = of_get_gpio(np, 1);
    if (!gpio_is_valid(pdata->irq_gpio))
    {
        AXS_ERROR("Parse INT GPIO from dt failed %d", pdata->irq_gpio);
        pdata->irq_gpio = -1;
        return false;
    }
    AXS_DEBUG("  %-12s: %d", "int gpio", pdata->irq_gpio);
    ts_data->irq = gpio_to_irq(pdata->irq_gpio);
    if (ts_data->irq < 0)
    {
        AXS_ERROR("Parse irq failed %d", ts_data->irq);
        return false;
    }
    AXS_DEBUG("  %-12s: %d", "irq num", ts_data->irq);
#if AXS_POWER_SOURCE_CUST_EN
    ret = of_property_read_string(np, "vdd_name", &pdata->vdd_name);
    if (ret)
    {
        AXS_ERROR("fail to get vdd_name\n");
        return false;
    }
#endif
    return true;
}
#endif


static int axs_report_buffer_init(struct axs_ts_data *ts_data)
{
#if AXS_FIRMWARE_LOG_EN
	ts_data->pnt_buf_size = AXS_FIRMWARE_LOG_LEN;
#else	
    ts_data->pnt_buf_size = AXS_MAX_TOUCH_NUMBER * AXS_ONE_TCH_LEN + 2;
#endif
	return 0;
}

static void axs_ts_probe_entry(struct axs_ts_data *ts_data)
{
    int ret = 0;

#if AXS_GESTURE_EN
    if (!axs_gesture_init(ts_data))
    {
        AXS_ERROR("init gesture fail");
    }
#else
    ts_data->gesture_enable = 0;
#endif

#if AXS_PROXIMITY_SENSOR_EN
	g_axs_data->psensor_enable = 0;// default 0; change this value by sysfs
	g_axs_data->psensor_state = 0;// default sate: away
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN
	g_axs_data->psensor_enable = 0;// default 0; change this value by sysfs
	g_axs_data->psensor_state = 0;// default sate: away
#endif

	g_axs_data->suspended = false;
	
#if AXS_AUTO_UPGRADE_EN
    if (!axs_fwupg_init())
    {
        AXS_ERROR("init fw upgrade fail!\n");
    }
#elif AXS_DOWNLOAD_APP_EN
    if (!axs_download_init())
    {
        AXS_ERROR("axs_download_init fail!\n");
    }
#endif

#if AXS_DEBUG_PROCFS_EN
    ret = axs_create_proc_file(ts_data);
    if (ret)
    {
        AXS_ERROR("axs_create_proc_file fail");
    }
#endif

#if AXS_DEBUG_SYSFS_EN
    if (!axs_debug_create_sysfs(ts_data))
    {
        AXS_ERROR("axs_debug_create_sysfs fail!\n");
    }
#endif
#if AXS_ESD_CHECK_EN
    ret = axs_esd_check_init(ts_data);
    if (ret)
    {
        AXS_ERROR("init esd check fail");
    }
#endif
#if AXS_PEN_EVENT_CHECK_EN
    ret = axs_pen_event_check_init(ts_data);
    if (ret)
    {
        AXS_ERROR("init pen event check fail");
    }
#endif

}

static int check_ctp_chip(void)
{
	ctp_lock_mutex();
	tp_device_id(0x1520);
	ctp_unlock_mutex();
	return 0;
}

static int remove_ctp_chip(void)
{
	ctp_lock_mutex();
	tp_device_id(0xFFFF);
	ctp_unlock_mutex();
	return 0;
}

// static struct kobject *tp_ctrl_kobj = NULL;
static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "axs_ts\n");
}

static ssize_t name_store(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t count)
{
	return count;
}
static DEVICE_ATTR_RW(name);
static struct attribute *cts_dev_name_atts[] = {
		&dev_attr_name.attr,
		NULL
	};

static const struct attribute_group cts_dev_name_atts_group = {
    .attrs = cts_dev_name_atts,
};

#ifndef ZCFG_TP_NOT_USE_SUSPEND_RESUME_NODE
static ssize_t ts_suspend_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%s\n",
	    g_axs_data->suspended ? "true" : "false");
}

static ssize_t ts_suspend_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	printk("%s,%d\n",__func__,buf[0]);
  	if ((buf[0] == 49) && !g_axs_data->suspended)
		axs_ts_suspend(g_axs_data->dev);
	else if ((buf[0] == 48) && g_axs_data->suspended)
		axs_ts_resume(g_axs_data->dev);

  return count;
}
static DEVICE_ATTR(ts_suspend, 0664, ts_suspend_show, ts_suspend_store);


static struct attribute *tp_sysfs_attrs[] = {
  &dev_attr_ts_suspend.attr,
  NULL,
};

static struct attribute_group tp_attr_group = {
  .attrs = tp_sysfs_attrs,
  	
};
#endif
static const struct attribute_group *cts_dev_attr_groups[] = {
#ifndef ZCFG_TP_NOT_USE_SUSPEND_RESUME_NODE
    &tp_attr_group,
#endif
    &cts_dev_name_atts_group,
    NULL
};
// static int tp_sysfs_init(struct axs_ts_data *ts_data)
// {
// 	int ret =0, i;
// 	tp_ctrl_kobj = kobject_create_and_add("touchscreen", NULL);
// 	if (!tp_ctrl_kobj){
// 		return -ENOMEM;
// 	}
// 	   for (i = 0; cts_dev_attr_groups[i]; i++) {
//         ret = sysfs_create_group(tp_ctrl_kobj, cts_dev_attr_groups[i]);
//         if (ret) {
//             while (--i >= 0)
//                 sysfs_remove_group(tp_ctrl_kobj, cts_dev_attr_groups[i]);
//             break;
//         }
//     }
//     if (ret) {
//         AXS_ERROR("Add device attr failed %d", ret);
//         return ret;
//     }
// 	return 0;
// }

static int tp_sysfs_init(struct axs_ts_data *ts_data)
{
	int ret =0, i;

    for (i = 0; cts_dev_attr_groups[i]; i++) {
        ret = sysfs_create_group(&ts_data->dev->kobj, cts_dev_attr_groups[i]);
        if (ret) {
            while (--i >= 0)
                sysfs_remove_group(&ts_data->dev->kobj, cts_dev_attr_groups[i]);
            break;
        }
    }

	ret = sysfs_create_link(NULL, &ts_data->dev->kobj, "touchscreen");
    if (ret) {
        AXS_ERROR("Create sysfs touchscreen link error:%d", ret);
    }


	return 0;
}

extern bool zyt_check_cali_mode(void);


#if  AXS_PROXIMITY_SENSOR_EN
static int TP_face_mode_switch(int on)
{

	printk("axs_ps_ioctl1 %d!\n", on);
    if(1 == on){
        g_axs_data->psensor_enable = 1;
        axs_write_psensor_enable(0x01);
    }else if(0 == on){
        g_axs_data->psensor_enable = 0;
        axs_write_psensor_enable(0x00);
    }else{
        return -EINVAL;
    }

    return 0;
}


static long axs_ps_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
    printk("axs_ps_ioctl1 %d!\n", cmd);//zxb

    switch(cmd){
        case FT_IOCTL_PROX_ON:
			printk("FT_IOCTL_PROX_ON\n");
            TP_face_mode_switch(1);
            break;

        case FT_IOCTL_PROX_OFF:
			printk("FT_IOCTL_PROX_OFF\n");
            TP_face_mode_switch(0);
            break;

        default:
            printk("%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
            return -EINVAL;
    }
    return 0;
}

static struct file_operations axs_ps_fops = {
    .owner				= THIS_MODULE,
    .open				= NULL,
    .release			= NULL,
    .unlocked_ioctl		= axs_ps_ioctl,
};
static struct miscdevice axs_ps_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ctp_proximity",
    .fops = &axs_ps_fops,
};
#endif

#if AXS_BUS_SPI
static int axs_ts_probe(struct spi_device *spi)
{
    struct axs_ts_data *ts_data = NULL;
    int err = 0;
    int pdata_size;
    AXS_DEBUG("axs_ts_probe...");
    if (!spi)
        return -ENOMEM;
    spi->mode = (SPI_MODE_0) ;
    spi->bits_per_word = 8 ;
    //if (!spi->max_speed_hz)
    spi->max_speed_hz = 10000000; // 10000000

    err = spi_setup( spi );
    if ( err != 0 )
    {
        AXS_ERROR( "axs_ts_spi_probe ERROR : \n");
    }
    else
    {
        AXS_DEBUG( "axs_ts_spi_probe Done : %d\n", err);
    }


    ts_data = (struct axs_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data)
    {
        AXS_ERROR("allocate memory for g_axs_data fail");
        return -ENOMEM;
    }

    g_axs_data = ts_data;

    ts_data->spi = spi;
    ts_data->dev = &spi->dev;
    ts_data->bus_type = BUS_TYPE_SPI;
    //i2c_set_clientdata(client, ts_data);
    
    tp_sysfs_init(ts_data);

    // axs_ts_probe_entry begin
    pdata_size = sizeof(struct axs_ts_platform_data);
    ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
    if (!ts_data->pdata)
    {
        AXS_ERROR("allocate memory for platform_data fail");
        goto exit_release_g_axs_data;
    }

#ifdef CONFIG_OF
    if (!axs_platform_data_init(ts_data,&spi->dev))
    {
        AXS_ERROR("axs_platform_data_init fail");
        goto exit_release_pdata;
    }
#endif

    axs_ts_hw_init(ts_data);


#if !AXS_INTERRUPT_THREAD
    INIT_WORK(&ts_data->pen_event_work, axs_ts_worker);
#endif
    ts_data->ts_workqueue = create_singlethread_workqueue("axs_wq");
    if (!ts_data->ts_workqueue)
    {
        AXS_DEBUG("create ts_workqueue fail\n");
        err = -ESRCH;
        goto exit_release_gpio;
    }

    spin_lock_init(&ts_data->irq_lock);
//    mutex_init(&ts_data->report_mutex);
    mutex_init(&ts_data->bus_mutex);

    err = axs_bus_init(ts_data);
    if (err)
    {
        AXS_ERROR("bus initialize fail");
        goto exit_release_bus;
    }

    //maybe read error before download/upgrade;do not move this code;
#if AXS_DOWNLOAD_APP_EN
#elif AXS_AUTO_UPGRADE_EN
#if AXS_UPGRADE_CHECK_VERSION
    if (!axs_fwupg_get_ver_in_tp(&g_axs_fw_ver))
    {
        AXS_DEBUG("firmware read version fail");
		err = -1;
        goto exit_release_bus;
    }
    AXS_DEBUG("firmware version=0x%4x\n",g_axs_fw_ver);
#endif
#else
    if (!axs_fwupg_get_ver_in_tp(&g_axs_fw_ver))
    {
        AXS_DEBUG("firmware read version fail");
		err = -1;
        goto exit_release_bus;
    }

    AXS_DEBUG("firmware version=0x%4x\n",g_axs_fw_ver);
#endif
    if (axs_input_init(ts_data))
    {
        err = -ENOMEM;
        AXS_DEBUG("failed to allocate input device\n");
        goto exit_release_bus;
    }

    err = axs_report_buffer_init(ts_data);
    if (err)
    {
        AXS_ERROR("report buffer init fail");
        goto exit_release_input;
    }

    axs_ts_probe_entry(ts_data);
#if 0//defined(CONFIG_FB)
    if (ts_data->ts_workqueue)
    {
        INIT_WORK(&ts_data->resume_work, axs_resume_work);
    }
    ts_data->fb_notif.notifier_call = axs_fb_notifier_callback;
    err = fb_register_client(&ts_data->fb_notif);
    if (err)
    {
        AXS_ERROR("[FB]Unable to register fb_notifier: %d", err);
    }
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    AXS_DEBUG("register_early_suspend");
    g_axs_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    g_axs_data->early_suspend.suspend = axs_ts_early_suspend;
    g_axs_data->early_suspend.resume  = axs_ts_late_resume;
    register_early_suspend(&ts_data->early_suspend);
#endif

    AXS_DEBUG("IRQ number is %d", g_axs_data->irq);
#if AXS_INTERRUPT_THREAD
    err = request_threaded_irq(g_axs_data->irq, NULL, axs_ts_interrupt, IRQF_TRIGGER_FALLING|IRQF_ONESHOT, AXS_DRIVER_NAME, g_axs_data);
    if (err)
    {
        AXS_ERROR("axs_probe: request irq failed\n");
        goto exit_release_input;
    }
#else
    err = request_irq(g_axs_data->irq, axs_ts_interrupt, IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND, AXS_DRIVER_NAME, g_axs_data);
    if (err)
    {
        AXS_ERROR("axs_probe: request irq failed\n");
        goto exit_release_input;
    }
#endif
    
    

    check_ctp_chip();
	{
		extern void zyt_info_s2(char* s1,char* s2);
		extern void zyt_info_sx(char* c1,int x);

		zyt_info_s2("[TP] : ","AXS_TS");
		zyt_info_sx("[AXS_TS] : FW Version:",g_axs_fw_ver);
	}

	if(zyt_check_cali_mode() == true){
		axs_ts_suspend(g_axs_data->dev);
	}
    AXS_DEBUG("probe successfully");
    return 0;

exit_release_input:
    input_free_device(ts_data->input_dev);
exit_release_bus:
    kfree_safe(ts_data->bus_tx_buf);
    kfree_safe(ts_data->bus_rx_buf);

//exit_release_workqueue:
#if !AXS_INTERRUPT_THREAD
    cancel_work_sync(&ts_data->pen_event_work);
#endif
    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
exit_release_gpio:
    gpio_free(ts_data->pdata->reset_gpio);
    gpio_free(ts_data->pdata->irq_gpio);

exit_release_pdata:
    kfree_safe(ts_data->pdata);
exit_release_g_axs_data:
    kfree_safe(g_axs_data);
    return err;
}

#else
static int axs_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct axs_ts_data *ts_data = NULL;
    int err = 0;
    int pdata_size;
    AXS_DEBUG("axs_ts_probe...");
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        AXS_ERROR("I2C not supported");
        return -ENODEV;
    }

	if(tp_device_id(0)!=0)
	{
		printk("CTP(0x%x)Exist!", tp_device_id(0));
		return -ENODEV;
	}
    /*
    if (client->addr != AXS_I2C_SLAVE_ADDR) {
        AXS_DEBUG("[axs]Change i2c addr 0x%02x to %x",
                 client->addr, AXS_I2C_SLAVE_ADDR);
        client->addr = AXS_I2C_SLAVE_ADDR;
        AXS_DEBUG("[axs]i2c addr=0x%x\n", client->addr);
    }*/

    if (client->addr != AXS_I2C_SLAVE_ADDR)
    {
        AXS_DEBUG("[axs]i2c addr=0x%x\n", client->addr);
        return -ESRCH;
    }

    ts_data = (struct axs_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data)
    {
        AXS_ERROR("allocate memory for g_axs_data fail");
        return -ENOMEM;
    }

    g_axs_data = ts_data;

    ts_data->client = client;
    ts_data->dev = &client->dev;
    ts_data->bus_type = BUS_TYPE_I2C;
    //i2c_set_clientdata(client, ts_data);
    
    tp_sysfs_init(ts_data);

    // axs_ts_probe_entry begin
    pdata_size = sizeof(struct axs_ts_platform_data);
    ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
    if (!ts_data->pdata)
    {
        AXS_ERROR("allocate memory for platform_data fail");
        goto exit_release_g_axs_data;
    }

#ifdef CONFIG_OF
    if (!axs_platform_data_init(ts_data,&client->dev))
    {
        AXS_ERROR("axs_platform_data_init fail");
        goto exit_release_pdata;
    }
#endif

    axs_ts_hw_init(ts_data);

#if !AXS_INTERRUPT_THREAD
    INIT_WORK(&ts_data->pen_event_work, axs_ts_worker);
#endif

    ts_data->ts_workqueue = create_singlethread_workqueue("axs_wq");
    if (!ts_data->ts_workqueue)
    {
        AXS_DEBUG("create ts_workqueue fail\n");
        err = -ESRCH;
        goto exit_release_gpio;
    }

    spin_lock_init(&ts_data->irq_lock);
//    mutex_init(&ts_data->report_mutex);
    mutex_init(&ts_data->bus_mutex);

    err = axs_bus_init(ts_data);
    if (err)
    {
        AXS_ERROR("bus initialize fail");
        goto exit_release_bus;
    }
    //maybe read error before download/upgrade;do not move this code;
#if AXS_DOWNLOAD_APP_EN
#elif AXS_AUTO_UPGRADE_EN
#if AXS_UPGRADE_CHECK_VERSION
    if (!axs_fwupg_get_ver_in_tp(&g_axs_fw_ver))
    {
        AXS_DEBUG("firmware read version fail 1");
		err = -1;
        goto exit_release_bus;
    }
    AXS_DEBUG("firmware version=0x%4x\n",g_axs_fw_ver);
#endif
#else
    if (!axs_fwupg_get_ver_in_tp(&g_axs_fw_ver))
    {
        AXS_DEBUG("firmware read version fail 1");
		err = -1;
        goto exit_release_bus;
    }

    AXS_DEBUG("firmware version=0x%4x\n",g_axs_fw_ver);
#endif

	#if  AXS_PROXIMITY_SENSOR_EN
	err = misc_register(&axs_ps_device);
    if(err < 0){
        printk("%s: axs_ps_device register failed\n", __func__);
        goto exit_release_bus;
    }
	#endif
	
    if (axs_input_init(ts_data))
    {
        err = -ENOMEM;
        AXS_DEBUG("failed to allocate input device\n");
        goto exit_release_bus;
    }



    err = axs_report_buffer_init(ts_data);
    if (err)
    {
        AXS_ERROR("report buffer init fail");
        goto exit_release_input;
    }

    axs_ts_probe_entry(ts_data);

#if 0//defined(CONFIG_FB)
    if (ts_data->ts_workqueue)
    {
        INIT_WORK(&ts_data->resume_work, axs_resume_work);
    }
    ts_data->fb_notif.notifier_call = axs_fb_notifier_callback;
    err = fb_register_client(&ts_data->fb_notif);
    if (err)
    {
        AXS_ERROR("[FB]Unable to register fb_notifier: %d", err);
    }
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    AXS_DEBUG("register_early_suspend");
    g_axs_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
    g_axs_data->early_suspend.suspend = axs_ts_early_suspend;
    g_axs_data->early_suspend.resume    = axs_ts_late_resume;
    register_early_suspend(&ts_data->early_suspend);
#endif


    AXS_DEBUG("%s IRQ number is %d", client->name, g_axs_data->irq);
#if AXS_INTERRUPT_THREAD
    err = request_threaded_irq(g_axs_data->irq, NULL, axs_ts_interrupt, IRQF_TRIGGER_FALLING|IRQF_ONESHOT, AXS_DRIVER_NAME, g_axs_data);
    if (err)
    {
        AXS_ERROR("axs_probe: request irq failed\n");
        goto exit_release_input;
    }
#else
    err = request_irq(g_axs_data->irq, axs_ts_interrupt, IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND, AXS_DRIVER_NAME, g_axs_data);
    if (err)
    {
        AXS_ERROR("axs_probe: request irq failed\n");
        goto exit_release_input;
    }
#endif
	
	check_ctp_chip();
	{
		extern void zyt_info_s2(char* s1,char* s2);
		extern void zyt_info_sx(char* c1,int x);

		zyt_info_s2("[TP] : ","AXS_TS");
		zyt_info_sx("[AXS_TS] : FW Version:",g_axs_fw_ver);
	}

	if(zyt_check_cali_mode() == true){
		axs_ts_suspend(g_axs_data->dev);
	}

    #if defined(ZCFG_TP_NOT_USE_SUSPEND_RESUME_NODE)
    g_axs_data->drm_notifier.notifier_call = jx_drm_notifier_callback;
	err = disp_notifier_register(&g_axs_data->drm_notifier);

	if(err < 0){
    	AXS_DEBUG("Init drm notifier failed %d", err);
    	goto exit_release_input;
    }
#endif 

    AXS_DEBUG("probe successfully");
    return 0;

exit_release_input:
    input_free_device(ts_data->input_dev);
exit_release_bus:

    kfree_safe(ts_data->bus_tx_buf);
    kfree_safe(ts_data->bus_rx_buf);
//exit_release_workqueue:
#if !AXS_INTERRUPT_THREAD
    cancel_work_sync(&ts_data->pen_event_work);
#endif

#if  AXS_PROXIMITY_SENSOR_EN
	misc_deregister(&axs_ps_device);
#endif

    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
exit_release_gpio:
    gpio_free(ts_data->pdata->reset_gpio);
    gpio_free(ts_data->pdata->irq_gpio);
exit_release_pdata:
    kfree_safe(ts_data->pdata);
exit_release_g_axs_data:
    kfree_safe(g_axs_data);
    return err;
}
#endif

#if AXS_BUS_IIC
static const struct i2c_device_id axs_ts_id[] =
{
    {AXS_DRIVER_NAME, 0},{ }
};
MODULE_DEVICE_TABLE(i2c, axs_ts_id);
#else
struct spi_device_id axs_ts_id[] ={
    {AXS_DRIVER_NAME,0},
    {}
};
#endif

static const struct of_device_id axs_of_match[] =   // custom 1
{
    {.compatible = "axs15205,axs15205_touch",},
    { }
};

MODULE_DEVICE_TABLE(of, axs_of_match);

#if AXS_BUS_SPI
static int axs_ts_remove(struct spi_device *spi)
{
    AXS_DEBUG("axs_ts_remove");
    
    remove_ctp_chip();

#if AXS_DEBUG_SYSFS_EN
    axs_debug_remove_sysfs(g_axs_data);
#endif

#if AXS_DEBUG_PROCFS_EN
    axs_release_proc_file(g_axs_data);
#endif

    axs_bus_exit(g_axs_data);

#if 0//defined(CONFIG_FB)
    if (fb_unregister_client(&g_axs_data->fb_notif))
        AXS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    unregister_early_suspend(&g_axs_data->early_suspend);
#endif
    free_irq(g_axs_data->irq, g_axs_data);
    input_unregister_device(g_axs_data->input_dev);
#if !AXS_INTERRUPT_THREAD
    cancel_work_sync(&g_axs_data->pen_event_work);
#endif
    destroy_workqueue(g_axs_data->ts_workqueue);

    kfree_safe(g_axs_data->pdata);
    kfree_safe(g_axs_data);
    return 0;
}

static struct spi_driver axs_ts_driver =
{
    .probe    = axs_ts_probe,
    .remove   = axs_ts_remove,
	.id_table	= axs_ts_id,
    .driver = {
        .name   = AXS_DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(axs_of_match),
    },
};

#else
static int axs_ts_remove(struct i2c_client *client)
{
	AXS_DEBUG("axs_ts_remove");

	remove_ctp_chip();
		
#if AXS_DEBUG_SYSFS_EN
    axs_debug_remove_sysfs(g_axs_data);
#endif

#if defined(ZCFG_TP_NOT_USE_SUSPEND_RESUME_NODE)
 	disp_notifier_unregister(&g_axs_data->drm_notifier);
#endif

#if AXS_DEBUG_PROCFS_EN
    axs_release_proc_file(g_axs_data);
#endif

    axs_bus_exit(g_axs_data);

#if 0//defined(CONFIG_FB)
    if (fb_unregister_client(&g_axs_data->fb_notif))
        AXS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    unregister_early_suspend(&g_axs_data->early_suspend);
#endif
    free_irq(g_axs_data->irq, g_axs_data);
    input_unregister_device(g_axs_data->input_dev);
#if !AXS_INTERRUPT_THREAD
    cancel_work_sync(&g_axs_data->pen_event_work);
#endif
    destroy_workqueue(g_axs_data->ts_workqueue);

    kfree_safe(g_axs_data->pdata);
    kfree_safe(g_axs_data);
    return 0;
}

static struct i2c_driver axs_ts_driver =
{
    .probe      = axs_ts_probe,
    .remove     = axs_ts_remove,
    .id_table   = axs_ts_id,
    .driver = {
        .name   = AXS_DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = axs_of_match,
    },
};
#endif

static int __init axs_ts_init(void)
{
	//if(strcmp(m_lcd_id,"ID15205") == 0) { // custom 2
	AXS_DEBUG("axs_ts_init Driver version: %s", AXS_DRIVER_VERSION);
	if(tp_device_id(0)!=0)
	{
		printk("CTP(0x%x)Exist!", tp_device_id(0));
		return -ENODEV;
	}
#if AXS_BUS_SPI
    return spi_register_driver(&axs_ts_driver);
#else
    return  i2c_add_driver(&axs_ts_driver);
#endif
    //}
    //else{
    //  AXS_DEBUG("lcd isn't matched;lcd id:%s\n",m_lcd_id);
    //  return -1;
    //}
}

static void __exit axs_ts_exit(void)
{
#if AXS_BUS_SPI
    spi_unregister_driver(&axs_ts_driver);
#else
    i2c_del_driver(&axs_ts_driver);
#endif
}

module_init(axs_ts_init);
module_exit(axs_ts_exit);

MODULE_AUTHOR("AiXieSheng Technology.");
MODULE_DESCRIPTION("AXS TouchScreen Driver");
MODULE_LICENSE("GPL");






