/*
 * aw87319_audio.c   aw87319 pa module
 *
 * Version: v1.2.2
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <linux/math64.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/workqueue.h>
#include <linux/of_address.h>
#include <linux/random.h>
#include <linux/pm_wakeup.h>

#define BCT3236_I2C_NAME    "bct3236"

#define BCT3236_DRIVER_VERSION  "v1.2.2"
#define AW_I2C_RETRIES 5

//Antai <AI_BSP_LED> <qudg> <2024-1-25> LED mode select start
#define LED_BREATH_MODE_FAST_FLASH 1
#define LED_BREATH_MODE_CHARGING 2 
#define LED_BREATH_MODE_POWERONOFF 3

#define LED_SYSTEM_MESSAGE 4
#define LED_SHORT_MESSAGE 5
#define LED_THIRD_APP_MESSAGE 6
#define LED_CALL_MODE 7
#define LED_TIMER 8
#define LED_ALARM_CLOCK 9

#define LED_SOUND_MODE1 10
#define LED_SOUND_MODE2 11
#define LED_SOUND_MODE3 12
#define LED_SOUND_MODE4 13
#define LED_FLASHLIGHT_MODE 14

//Antai <AI_BSP_LED> <qudg> <2024-1-25> LED mode select end

#define LED_NUMBERS 8

#define LED_ON  0x01
#define LED_OFF 0x00

//Antai <AI_BSP_LED> <qudg> <2024-1-25> LED mode select start
struct bct3236_t{
    struct i2c_client *i2c_client;
    unsigned char init_flag;
    unsigned char hwen_flag;
	struct work_struct bct_mmi_work;
	struct workqueue_struct *bct_mmi_workqueue;
    struct work_struct bct_soundlevel_work;
	struct workqueue_struct *bct_soundlevel_workqueue;
    struct work_struct bct_flashing_work;
	struct workqueue_struct *bct_flashing_workqueue;
	struct work_struct bct_color_work;
	struct workqueue_struct *bct_color_workqueue;
    struct wakeup_source *wake_lock;
    int brightness;
    int level;
    int light_num;
    int led_mode_number;
    int sound_mode_flag;
	int sound_mode_on;
    bool mmi_flag;
	int color_level_select;
	int led_num_select;
	int led_num_delay;
};
//Antai <AI_BSP_LED> <qudg> <2024-1-25> LED mode select end

struct led_msg{
    unsigned char led_color_addr;
    unsigned char led_color_data;
    unsigned char led_level_addr;
    unsigned char led_level_data;
};

struct bct3236_t *bct3236 = NULL;
struct led_msg g_left[8];
struct led_msg g_right[8];

static void led_msg_color_init()
{
    int i;
    unsigned char color_addr = 0x01;
	
	for(i=0;i<LED_NUMBERS;i++)
    {
        g_left[i].led_color_addr = color_addr;
		g_left[i].led_color_data = 0xFF;
		g_right[i].led_color_addr = color_addr+LED_NUMBERS;
		g_right[i].led_color_data = 0xFF;
        color_addr += 0x01;
    }
}

static void led_msg_level_init()
{
    int i;
    unsigned char level_addr = 0x26;
    for(i=0;i<LED_NUMBERS;i++)
    {
		g_left[i].led_level_addr = level_addr;
		g_left[i].led_level_data = g_left[i].led_level_data | 0x01;   // init led on
		
		g_right[i].led_level_addr = level_addr + LED_NUMBERS;
		g_right[i].led_level_data = g_right[i].led_level_data | 0x01; // init led on
		
		level_addr += 0x01;
    } 
}

static void led_msg_init()
{
    led_msg_color_init();
    led_msg_level_init();
}

static int i2c_write_reg(unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_write_byte_data(bct3236->i2c_client, reg_addr, reg_data);
        if(ret < 0) {
            pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
        mdelay(2);
    }

    return ret;
}

static void led_shutdown(void)
{
    i2c_write_reg(0x4f,0x00); // reset register value
    i2c_write_reg(0x4a,0x01); // 0: normal operation; 1:shutdown all LEDs
}

static void set_all_onoff_bit(unsigned char val)
{
    int i;
    for(i=0;i<LED_NUMBERS;i++)
    {
		g_left[i].led_level_data = ((g_left[i].led_level_data)&0xFE) | (val&0x01);
		g_right[i].led_level_data = ((g_right[i].led_level_data)&0xFE) | (val&0x01);
	
    }

}
static void led_brightness_select(int brightness, struct led_msg *msg1)
{
	msg1->led_color_data = brightness&0xFF;
}
static void led_level_select(int level,struct led_msg *msg1)
{
	/*control register 0x26~0x49
	  bit[1~2]:led current;
          00 : Imax
          01 : Imax/2
          10 : Imax/3
          11 : Imax/4		  
      bit[0]:led state;
          0 : Led off
          1 : Led on		  
	*/
	msg1->led_level_data = ((level<<1)&0x06) | (msg1->led_level_data&0x01);  
}
static void led_onoff_config(struct led_msg *msg1, unsigned char val)
{
	msg1->led_level_data = ((msg1->led_level_data)&0xFE) | (val&0x01);
}

static void led_send_level_msg()
{
    int i;
	i2c_write_reg(0x00,0x01); //normal operation

	for(i=0;i<LED_NUMBERS;i++)
    {
		i2c_write_reg(g_left[i].led_level_addr,g_left[i].led_level_data);
		i2c_write_reg(g_right[i].led_level_addr,g_right[i].led_level_data);
    }
    i2c_write_reg(0x25,0x00); //brightness register update
	i2c_write_reg(0x4a,0x00); //normal operation
}

static void led_send_brightness_msg()
{
	int i;
	i2c_write_reg(0x00,0x01); //normal operation

	for(i=0;i<LED_NUMBERS;i++)
    {
		i2c_write_reg(g_left[i].led_color_addr,g_left[i].led_color_data);
		i2c_write_reg(g_right[i].led_color_addr,g_right[i].led_color_data);
    }
    i2c_write_reg(0x25,0x00); //brightness register update
	i2c_write_reg(0x4a,0x00); //normal operation
}

static void led_send_msg()
{
    int i;
	i2c_write_reg(0x00,0x01);
	
	for(i=0;i<LED_NUMBERS;i++)

    {
		//left led config
        i2c_write_reg(g_left[i].led_color_addr,g_left[i].led_color_data);
		i2c_write_reg(g_left[i].led_level_addr,g_left[i].led_level_data);
		//right led config
		i2c_write_reg(g_right[i].led_color_addr,g_right[i].led_color_data);
        i2c_write_reg(g_right[i].led_level_addr,g_right[i].led_level_data);

    }
    i2c_write_reg(0x25,0x00); //brightness register update
	i2c_write_reg(0x4a,0x00); //normal operation
}


//动画实现

// static void led_send_msg_by_one(struct led_msg *msg1)
// {
// 	//i2c_write_reg(0x00,0x01);
// 	i2c_write_reg(msg1->led_color_addr,msg1->led_color_data);
//     i2c_write_reg(msg1->led_level_addr,msg1->led_level_data);
// 	//i2c_write_reg(0x25,0x00);
// }

//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode1 start

static void open_all_light(struct work_struct *work)
{
    int j;
    pr_info("%s Enter bct3236->mmi_flag = %d\n", __func__,bct3236->mmi_flag);
    led_msg_init();
    for(j=0;j<LED_NUMBERS;j++){
        led_level_select(3,&g_left[j]);
		led_level_select(3,&g_right[j]);
    }
	
    while(bct3236->mmi_flag){
        led_send_msg();
		mdelay(1000);
    }
    led_shutdown();
}

//sound mode 1
static void led_sound_work_mode1()
{
	int i = 0;
	int left_buf[8] = {7,5,1,3,0,2,4,6};
	int right_buf[8] = {1,0,2,3,6,7,4,5};
	pr_info("%s Enter\n", __func__);
	led_msg_color_init();
	set_all_onoff_bit(0x00);
	do{
		
		for(i = 0;i<LED_NUMBERS;i++){
			led_onoff_config(&g_left[left_buf[i]],0x01);
			led_onoff_config(&g_right[right_buf[i]],0x01);
	        led_send_msg();
	        mdelay(100); //100
			if(bct3236->led_mode_number != LED_SOUND_MODE1 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		for(i = 0;i<LED_NUMBERS;i++){
			led_onoff_config(&g_left[left_buf[i]],0x00);
			led_onoff_config(&g_right[right_buf[i]],0x00);
	        led_send_msg();
	        mdelay(100); //100
			if(bct3236->led_mode_number != LED_SOUND_MODE1 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
	}while(bct3236->led_mode_number == LED_SOUND_MODE1);
	pr_info("%s exit\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode1 end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode2 start
//sound mode 2
static void led_sound_work_mode2()
{
   	int i;
	int left_buf[8] = {7,5,1,3,0,2,4,6};
	int right_buf[8] = {1,0,2,3,6,7,4,5};
	pr_info("%s enter\n", __func__);
	led_msg_color_init();
	do{
		set_all_onoff_bit(0x00);
		for(i = 0;i<LED_NUMBERS;i++){
			led_onoff_config(&g_left[left_buf[i]],0x01);
			led_onoff_config(&g_right[right_buf[i]],0x01);
	        led_send_msg();
	        mdelay(100); //100
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		for(i = 0;i<LED_NUMBERS;i++){
			led_onoff_config(&g_left[left_buf[i]],0x00);
			led_onoff_config(&g_right[right_buf[i]],0x00);
	        led_send_msg();
	        mdelay(100); //100
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_left[i], 0x01);
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		led_send_msg();
		mdelay(100);
		
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_right[i], 0x01);
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		led_send_msg();
		mdelay(100);
		
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_left[i], 0x00);	
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		led_send_msg();
		mdelay(100);

		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_right[i], 0x00); 
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		led_send_msg();
		mdelay(100);
		
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_right[i], 0x01);	
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;
		}
		led_send_msg();
		mdelay(100);

		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_left[i], 0x01);	
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;
		}
		led_send_msg();
		mdelay(100);

		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_right[i], 0x00);	
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		led_send_msg();
		mdelay(100);

		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_left[i], 0x00);	
			if(bct3236->led_mode_number != LED_SOUND_MODE2 && bct3236->led_mode_number != LED_SOUND_MODE4)
					break;	
		}
		led_send_msg();
		mdelay(100);

		set_all_onoff_bit(0x01);
		led_send_msg();
		mdelay(1000);
		
	}while(bct3236->led_mode_number == LED_SOUND_MODE2);
	pr_info("%s exit\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode2 end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct_sound_level_mode start
static void bct_sound_level_mode(struct work_struct *work)
{
	int i, j;
	int color_buf[6] = {255, 63, 96, 128, 48, 130};
	pr_info("%s enter\n", __func__);
	do{
		for(j = 0; j < 6; j++){
			if(bct3236->led_mode_number != LED_SOUND_MODE2)
				break;
				
			for(i = 0; i < LED_NUMBERS; i++){
				led_brightness_select(color_buf[j], &g_right[i]);
				led_brightness_select(color_buf[j], &g_left[i]);
			}
			led_send_msg();
			mdelay(200);
		}
	}while(bct3236->led_mode_number == LED_SOUND_MODE2);
	pr_info("%s exit\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct_sound_level_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode3 start
//sound mode 3 
static void led_sound_work_mode3()
{
	int i;
	unsigned int num, delaynum, a;
	int left_buf[8] = {7,5,1,3,0,2,4,6};
	int right_buf[8] = {1,0,2,3,6,7,4,5};
	pr_info("%s enter\n", __func__);
	led_msg_color_init();
	set_all_onoff_bit(0x00);
	do{
		a = get_random_u32();
		//num = get_random_int() % 7 + 1;
		//delaynum = get_random_int() % 100 + 50;
		//get_random_bytes(&a, sizeof(a));
		num = a % 8;
		delaynum = 50 + a % 100;
		for(i = 0; i <= num; i++){
			led_onoff_config(&g_right[right_buf[i]],0x01);
			led_onoff_config(&g_left[left_buf[i]],0x01);
			led_send_msg();
			mdelay(delaynum);
		}
		
		for(i = num; i >= 0; i--){
			led_onoff_config(&g_right[right_buf[i]],0x00);
			led_onoff_config(&g_left[left_buf[i]],0x00);
			led_send_msg();
			mdelay(delaynum);
		}
		/*
		set_all_onoff_bit(0x00);
		led_send_msg();
		*/
	}while(bct3236->led_mode_number == LED_SOUND_MODE3);
	pr_info("%s exit\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode3 end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode4 start
static void led_sound_work_mode4()
{
	unsigned int a, num;
	pr_info("%s enter\n", __func__);
	while(bct3236->led_mode_number == LED_SOUND_MODE4){
		get_random_bytes(&a, sizeof(a));
		num = a % 3;
		switch (num)
			{
			case 0:
				led_sound_work_mode1();
				break;
			case 1:
				queue_work(bct3236->bct_soundlevel_workqueue, &bct3236->bct_soundlevel_work);
				led_sound_work_mode2();
				break;
			case 2:
				led_sound_work_mode3();
				break;
			default:
				break;
			}
	}
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_sound_work_mode4 end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_power_on_work start
static void led_power_on_work()
{
    int i = 0;
	//Antai <AI_BSP_LED> <qudg> <2024-1-03> Startup animation begin
	int left_buf[8] = {4,6,7,5,1,3,0,2}; 
	int right_buf[8] = {4,5,1,0,2,3,6,7};
	led_msg_color_init();
	set_all_onoff_bit(0x00);

	for(i = 0; i < LED_NUMBERS; i++){
		led_onoff_config(&g_right[right_buf[i]],0x01);
	    led_send_msg();
	    mdelay(100); //100
		led_onoff_config(&g_right[right_buf[i]],0x00);
		if(bct3236->led_mode_number != LED_BREATH_MODE_POWERONOFF)
			break;
	}

	for(i = 0;i<LED_NUMBERS;i++){
		led_onoff_config(&g_left[left_buf[i]],0x01);
	    led_send_msg();
	    mdelay(100); //100
		led_onoff_config(&g_left[left_buf[i]],0x00);
		if(bct3236->led_mode_number != LED_BREATH_MODE_POWERONOFF)
			break;
	}

	for(i = 0; i < LED_NUMBERS; i++){
		led_onoff_config(&g_right[right_buf[i]],0x01);
	    led_send_msg();
	    mdelay(100); //100
		if(bct3236->led_mode_number != LED_BREATH_MODE_POWERONOFF)
			break;
	}

	for(i = 0;i<LED_NUMBERS;i++){
		led_onoff_config(&g_left[left_buf[i]],0x01);
	    led_send_msg();
	    mdelay(100); //100
		if(bct3236->led_mode_number != LED_BREATH_MODE_POWERONOFF)
			break;
	}

    while (bct3236->led_mode_number == LED_BREATH_MODE_POWERONOFF)
    {
		led_send_msg();
		mdelay(500);
    }
	//Antai <AI_BSP_LED> <qudg> <2024-1-03> Startup animation end
    pr_info("%s exit\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_power_on_work end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_flashing_mode_work start
static void led_flashing_mode_work()
{
	pr_info("%s enter\n", __func__);
	
    led_msg_color_init();
	set_all_onoff_bit(0x01);
    led_send_msg();
    
	while (bct3236->led_mode_number == LED_BREATH_MODE_FAST_FLASH){
        i2c_write_reg(0x4a,0x00);
        mdelay(100);
        i2c_write_reg(0x4a,0x01);
        mdelay(100);
    }
    pr_info("%s exit\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_flashing_mode_work end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> thirdapp_msg_mode start
static void thirdapp_msg_mode()
{
	int i,j;
	pr_info("%s enter\n", __func__);
	__pm_stay_awake(bct3236->wake_lock);
	led_msg_color_init();
	set_all_onoff_bit(0x00);

	for(j = 0; j <= 255; j += 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_left[i], 0x01);
			led_brightness_select(j, &g_left[i]);
		}
		led_send_msg();
		mdelay(40);
	}

	for(j = 0; j <= 255; j += 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_right[i], 0x01);
			led_brightness_select(j,&g_right[i]);
		}
		led_send_msg();
		mdelay(40);
	}
	
	for(j = 255; j >= 0; j -= 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_brightness_select(j, &g_left[i]);
		}
		led_send_msg();
		mdelay(40);
	}
	
	for(j = 255; j >= 0; j -= 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_brightness_select(j,&g_right[i]);
		}
		led_send_msg();
		mdelay(40);
	}
	bct3236->led_mode_number = 0;
	__pm_relax(bct3236->wake_lock);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> thirdapp_msg_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> short_msg_mode start
static void short_msg_mode() //
{
	int i,j;
	pr_info("%s enter\n", __func__);
	__pm_stay_awake(bct3236->wake_lock);
    led_msg_init(); 	
	for(i=0;i<LED_NUMBERS;i++)
    {
		i2c_write_reg(g_left[i].led_level_addr,g_left[i].led_level_data);
		i2c_write_reg(g_right[i].led_level_addr,g_right[i].led_level_data);
    }
	for(j = 0; j <= 250; j += 25){
		for(i=0;i<LED_NUMBERS;i++) {
			led_brightness_select(j,&g_left[i]);
			led_brightness_select(j,&g_right[i]);
		}
		led_send_brightness_msg();
		mdelay(100);
		if(bct3236->led_mode_number != LED_SHORT_MESSAGE)
			break;	
	}

	for(j = 250; j >= 0; j -= 25){
		for(i=0;i<LED_NUMBERS;i++) {
			led_brightness_select(j,&g_left[i]);
			led_brightness_select(j,&g_right[i]);
			if(bct3236->led_mode_number != LED_SHORT_MESSAGE)
				break;
		}
		led_send_brightness_msg();
		mdelay(100);
		if(bct3236->led_mode_number != LED_SHORT_MESSAGE)
			break;
	}
	__pm_relax(bct3236->wake_lock);
	if(bct3236->led_mode_number == LED_SHORT_MESSAGE)
		bct3236->led_mode_number = 0;
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> short_msg_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> system_msg_mode start
static void system_msg_mode()
{
	int i,j;
	pr_info("%s enter\n", __func__);
	__pm_stay_awake(bct3236->wake_lock);
	led_msg_color_init();
	set_all_onoff_bit(0x00);

	for(j = 0; j <= 255; j += 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_right[i],0x01);
			led_brightness_select(j, &g_right[i]);
			if(bct3236->led_mode_number != LED_SYSTEM_MESSAGE)
				return;
		}
		led_send_msg();
		mdelay(40);
	}

	for(j = 0; j <= 255; j += 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_onoff_config(&g_left[i],0x01);
			led_brightness_select(j,&g_left[i]);
			if(bct3236->led_mode_number != LED_SYSTEM_MESSAGE)
				return;
		}
		led_send_msg();
		mdelay(40);
	}
	
	for(j = 255; j >= 0; j -= 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_brightness_select(j, &g_right[i]);
			if(bct3236->led_mode_number != LED_SYSTEM_MESSAGE)
				return;
		}
		led_send_msg();
		mdelay(40);
	}
	
	for(j = 255; j >= 0; j -= 51){
		for(i = 0; i < LED_NUMBERS; i++){
			led_brightness_select(j,&g_left[i]);
			if(bct3236->led_mode_number != LED_SYSTEM_MESSAGE)
				return;
		}
		led_send_msg();
		mdelay(40);
	}	
	if(bct3236->led_mode_number == LED_SYSTEM_MESSAGE)
		bct3236->led_mode_number = 0;
	__pm_relax(bct3236->wake_lock);
	pr_info("%s go out\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> system_msg_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> call_notification_mode start
static void call_notification_mode()
{
	int i,j;
	pr_info("%s enter\n", __func__);
	__pm_stay_awake(bct3236->wake_lock);
    led_msg_color_init();
	set_all_onoff_bit(0x01);
	while(bct3236->led_mode_number == LED_CALL_MODE){
		for(j = 0; j <= 250; j += 25){
			for(i=0;i<LED_NUMBERS;i++) {
				led_brightness_select(j,&g_left[i]);
				led_brightness_select(j,&g_right[i]);
			}
			led_send_msg();
			mdelay(100);
			if(bct3236->led_mode_number != LED_CALL_MODE)
				break;	
		}

		for(j = 250; j >= 0; j -= 25){
			for(i=0;i<LED_NUMBERS;i++) {
				led_brightness_select(j,&g_left[i]);
				led_brightness_select(j,&g_right[i]);
				if(bct3236->led_mode_number != LED_CALL_MODE)
					break;
			}
			led_send_msg();
			mdelay(100);
			if(bct3236->led_mode_number != LED_CALL_MODE)
				break;
		}
	}
	__pm_relax(bct3236->wake_lock);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> call_notification_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> timer_mode start
static void timer_mode()
{
	int i;
	int count = 0;
	int right_buf[3] = {3, 6, 7};
	int left_bug[3] = {0, 2, 3};
	pr_info("%s enter\n", __func__);
	led_msg_color_init();
	set_all_onoff_bit(0x00);
	while(bct3236->led_mode_number == LED_TIMER){
		if(count < 7){
			set_all_onoff_bit(0x01);
			led_send_msg();
			mdelay(500);
			set_all_onoff_bit(0x00);
			led_send_msg();
			mdelay(500);
		}else{
			set_all_onoff_bit(0x01);
			led_send_msg();
			mdelay(1000);
			
			led_onoff_config(&g_right[4], 0x00);
			led_onoff_config(&g_right[5], 0x00);
			led_onoff_config(&g_left[4], 0x00);
			led_onoff_config(&g_left[6], 0x00);
			led_send_msg();
			mdelay(1000);
			
			//set_all_onoff_bit(0x01);
			for(i = 0; i < 3; i++){
				led_onoff_config(&g_left[left_bug[i]], 0x00);
				led_onoff_config(&g_right[right_buf[i]], 0x00);
			}
			led_send_msg();
			mdelay(1000);

			set_all_onoff_bit(0x00);
			led_send_msg();
			break;
		}
		count++;
	}
	pr_info("%s go out\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> timer_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> alarm_clock_mode start
static void alarm_clock_mode()
{
	pr_info("%s enter\n", __func__);
	led_msg_color_init();
	set_all_onoff_bit(0x01);
	led_send_msg();
	while(bct3236->led_mode_number == LED_ALARM_CLOCK){
        i2c_write_reg(0x4a,0x00);
        mdelay(1000);
        i2c_write_reg(0x4a,0x01);
        mdelay(1000);
	}
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> alarm_clock_mode end

void BCT3236_PROFILE_INIT(struct timespec64 *ptv)
{
	ktime_get_real_ts64(ptv);
}

unsigned int BCT3236_PROFILE(struct timespec64 *ptv, char *tag)
{
	struct timespec64 now, diff;
	unsigned long long diff_ns;
	unsigned int diff_ms;
	
	ktime_get_real_ts64(&now);
	diff = timespec64_sub(now, *ptv);
	diff_ns = timespec64_to_ns(&diff);
	diff_ms = diff_ns/1000000;
	pr_info("[%s]Profile = %llu ns, %u\n", tag, diff_ns, diff_ms);
	
	return diff_ms;
}

//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_recharge_mode_work start
static void led_recharge_mode_work()
{
    int i = 0;
	//<Antai> <Ai_BSP_LED> <qudg> <2024-1-03> recharge mode display begin
	int j = 0;
	int left_buf[8] = {7,5,1,3,0,2,4,6};
	int right_buf[8] = {1,0,2,3,6,7,4,5};
	//<Antai> <Ai_BSP_LED> <qudg> <2024-1-03> recharge mode display end
	// struct timespec64 profile_time;
	// unsigned int reg_time = 0;
	pr_info("%s enter\n", __func__);
	__pm_stay_awake(bct3236->wake_lock);
    led_msg_init();
	set_all_onoff_bit(0x00);
	for(i = 0; i < bct3236->light_num * 2 - 2; i++){
			led_onoff_config(&g_left[left_buf[i]],0x01);
			led_onoff_config(&g_right[right_buf[i]],0x01);
	        led_send_msg();
	        mdelay(100); //100	
	}
	set_all_onoff_bit(0x00);
	
    while (bct3236->led_mode_number == LED_BREATH_MODE_CHARGING)
    {	
		if(bct3236->light_num >= 5) {
			for(i = 0 ; i < LED_NUMBERS; i++) {
				led_brightness_select(255,&g_left[left_buf[i]]);
				led_brightness_select(255,&g_right[right_buf[i]]);
				led_onoff_config(&g_left[left_buf[i]], 0x01);
				led_onoff_config(&g_right[right_buf[i]], 0x01);
			}
			led_send_msg();
		}
		//<Antai> <Ai_BSP_LED> <qudg> <2024-1-03> recharge mode display begin
		if(bct3236->light_num > 0){
			for(i = 0; i < bct3236->light_num * 2 && bct3236->light_num < 5; i++){
				led_brightness_select(255,&g_left[left_buf[i]]);
				led_brightness_select(255,&g_right[right_buf[i]]);
				led_onoff_config(&g_left[left_buf[i]], 0x01);
				led_onoff_config(&g_right[right_buf[i]], 0x01);
				if(bct3236->led_mode_number != LED_BREATH_MODE_CHARGING)
				break;
			}

			for(j = 0; j <= 250; j += 25){
				for(i = bct3236->light_num * 2 - 2; i < bct3236->light_num * 2 && bct3236->light_num < 5; i++) {
					led_brightness_select(j,&g_left[left_buf[i]]);
					led_brightness_select(j,&g_right[right_buf[i]]);
				}
				if(bct3236->led_mode_number != LED_BREATH_MODE_CHARGING)
					break;
				led_send_msg();
				mdelay(100);
			}

			for(j = 250; j >= 0; j -= 25){
				for(i = bct3236->light_num * 2 - 2; i < bct3236->light_num * 2 && bct3236->light_num < 5; i++) {
					led_brightness_select(j,&g_left[left_buf[i]]);
					led_brightness_select(j,&g_right[right_buf[i]]);
				}
				if(bct3236->led_mode_number != LED_BREATH_MODE_CHARGING)
					break;
				led_send_msg();
				mdelay(100);
			}
		}
    }
	
	if(bct3236->light_num >= 5){
		for(i = 7; i >= 0; i--){
			led_onoff_config(&g_left[left_buf[i]], 0x00);
			led_onoff_config(&g_right[right_buf[i]], 0x00);
			led_send_msg();
			mdelay(100);
		}
	}else{
		for(i = bct3236->light_num * 2 - 1; i >= 0; i--){
			led_onoff_config(&g_left[left_buf[i]], 0x00);
			led_onoff_config(&g_right[right_buf[i]], 0x00);
			led_send_msg();
			mdelay(100);
		}
	}	
	__pm_relax(bct3236->wake_lock);
	//<Antai> <Ai_BSP_LED> <qudg> <2024-1-03> recharge mode display end
	pr_info("%s exit\n", __func__);
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> led_recharge_mode_work end

//Antai <AI_BSP_LED> <qudg> <2024-2-2> color_level_select start
static void color_level_select_mode()
{
	int i;
	led_msg_init();
	//led_send_msg();
	if(bct3236->sound_mode_on == 1 && bct3236->led_mode_number == 0){
		if(bct3236->color_level_select > 0 && bct3236->color_level_select < 17){
			for(i = 0; i < LED_NUMBERS; i++){
				led_brightness_select(16 * bct3236->color_level_select - 1, &g_right[i]);
				led_brightness_select(16 * bct3236->color_level_select - 1, &g_left[i]);
			}
			led_send_msg();	
		}
	}
	if(0 == bct3236->color_level_select){
		led_shutdown();
	}
}
//Antai <AI_BSP_LED> <qudg> <2024-2-2> color_level_select end

//Antai <AI_BSP_LED> <qudg> <2024-1-31> led_num_select_mode start
static void led_num_select_mode()
{
	int i;
	int left_buf[8] = {7,5,1,3,0,2,4,6};
	int right_buf[8] = {1,0,2,3,6,7,4,5};
	pr_info("%s enter\n", __func__);
	led_msg_color_init();
	set_all_onoff_bit(0x00);
	led_send_msg();
	if(bct3236->sound_mode_on == 2 && bct3236->led_mode_number == 0){
		if(bct3236->led_num_select > 0 && bct3236->led_num_select < 9){
			
			for(i = 0; i < bct3236->led_num_select; i++){
				led_onoff_config(&g_right[right_buf[i]],0x01);
				led_onoff_config(&g_left[left_buf[i]],0x01);
				led_send_msg();
				mdelay(bct3236->led_num_delay);
			}
			for(i = bct3236->led_num_select - 1; i >= 0; i--){
				led_onoff_config(&g_right[right_buf[i]],0x00);
				led_onoff_config(&g_left[left_buf[i]],0x00);
				led_send_msg();
				mdelay(bct3236->led_num_delay);
			}
		}
			set_all_onoff_bit(0x00);
		if(bct3236->led_num_select == 0)
			led_send_msg();
	}
}

static void music_rhythm_mode(struct work_struct *work)
{
	pr_info("%s enter\n", __func__);
	switch (bct3236->sound_mode_on){
	case 1:
		color_level_select_mode();
		break;
	case 2:
		led_num_select_mode();
		break;
	default:
		led_shutdown();
   		break;
	}
}
//Antai <AI_BSP_LED> <qudg> <2024-1-31> led_num_select_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> light_on start
static void light_on(struct work_struct *work)
{
	pr_info("%s,led_mode_number:%d\n",__func__, bct3236->led_mode_number);
    switch (bct3236->led_mode_number){
    case LED_BREATH_MODE_FAST_FLASH: 
		led_flashing_mode_work();
		break;
    case LED_BREATH_MODE_CHARGING: 
		led_recharge_mode_work();
		break;
	case LED_BREATH_MODE_POWERONOFF: 
		led_power_on_work();
		break;
	case LED_SYSTEM_MESSAGE:
		system_msg_mode();
		break;
	case LED_SHORT_MESSAGE:
		short_msg_mode();
		break;
	case LED_THIRD_APP_MESSAGE:
		thirdapp_msg_mode();
		break;
	case LED_CALL_MODE:
		call_notification_mode();
		break;
	case LED_TIMER:
		timer_mode();
		break;
	case LED_ALARM_CLOCK:
		alarm_clock_mode();
		break;
	case LED_SOUND_MODE1:
		led_sound_work_mode1();
		break;
	case LED_SOUND_MODE2:
		led_sound_work_mode2();
		break;
	case LED_SOUND_MODE3:
		led_sound_work_mode3();
		break;
	case LED_SOUND_MODE4:
		led_sound_work_mode4();
		break;
    default:
        led_shutdown();
        break;
    }   
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> light_on end
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
extern bool g_is_fwdl_flag;
#endif
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end
//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_selectcolor_function start
static ssize_t bct3236_selectcolor_function(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
	//Antai <Ai_BSP_LED> <qudg> <2024-01-03> recharge mode display begin
    int j = 0;
	int ret = 0;
    int val = 0;
	//Antai <Ai_BSP_LED> <qudg> <2024-01-03> recharge mode display end
    ret = sscanf(buf, "%d", &val);	
    bct3236->brightness = val;
    if(val > 0)
    {
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin	
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
        pr_info("bct3236->brightness is %d, g_is_fwdl_flag:%d\n",bct3236->brightness,g_is_fwdl_flag);
#else
        pr_info("bct3236->brightness is %d\n",bct3236->brightness);
#endif
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end
        for(j=0;j<LED_NUMBERS;j++){
            led_brightness_select(bct3236->brightness, &g_left[j]);
			led_brightness_select(bct3236->brightness, &g_right[j]);
        }
		//Antai <AI_BSP_LED> <qudg> <2024-1-25> control led light begine
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin		
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
		if(!g_is_fwdl_flag)
#endif
			led_send_msg();
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end
		//Antai <AI_BSP_LED> <qudg> <2024-1-25> control led light end
        pr_info("%d %d\n",g_left[0].led_color_data,g_right[0].led_color_data);
    }
    else if(val == 0)
        led_shutdown();
    else
        pr_err("this is unuse number!!!!");
    pr_info("%s Enter 2 \n", __func__);
    return len;
}


static ssize_t bct3236_set_level_function(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
	int ret = 0;
    int j = 0;
    int level = 0;
	
    ret = sscanf(buf,"%d",&level);
    bct3236->level = level;
	
    if(level!=0)
    {
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin	
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
        pr_info("bct3236->level is %d,g_is_fwdl_flag=%d\n",bct3236->level,g_is_fwdl_flag);
#else
        pr_info("bct3236->level is %d",bct3236->level);
#endif
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end
        for(j=0;j<LED_NUMBERS;j++){
            led_level_select(bct3236->level-1,&g_left[j]);
			led_level_select(bct3236->level-1,&g_right[j]);
        }
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin	
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5		
        if(bct3236->led_mode_number != 0 && (!g_is_fwdl_flag))
#else
        if(bct3236->led_mode_number != 0)
#endif
			led_send_level_msg();
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end
    }
    else
        pr_err("this is unuse number!!!!");
    pr_info("%s Enter\n", __func__);
    return len;
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_selectcolor_function end


//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_mmi_test_open start
static ssize_t bct3236_mmi_test_open(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
    int ret = 0;
	pr_info("%s, buf[0] : %c\n", __func__, buf[0]);
    if(buf[0] == '1')
    {
        bct3236->mmi_flag = true;
        ret = queue_work(bct3236->bct_mmi_workqueue,&bct3236->bct_mmi_work);     
    }
    else{
        bct3236->mmi_flag = false;
    }
    pr_info("%s Enter\n", __func__);
    return len;
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_mmi_test_open end

//Antai <AI_BSP_LED> <qudg> <2024-1-30> bct3236_sound_mode start
static ssize_t bct3236_sound_mode(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
	int ret = 0;
	int onoff = 0;
	ret = sscanf(buf,"%u",&onoff);
	pr_info("%s, onoff:%d\n",__func__, onoff);
	bct3236->sound_mode_on = onoff;	
    return len;
}
//Antai <AI_BSP_LED> <qudg> <2024-1-30> bct3236_sound_mode end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_select_mode start
static ssize_t bct3236_select_mode(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
    // int ret = 0;
	unsigned int mode = 0;
	
	sscanf(buf, "%d", &mode);
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin	
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
	pr_info("%s,mode:%d,g_is_fwdl_flag:%d\n",__func__,mode,g_is_fwdl_flag);
#else
    pr_info("%s,mode:%d\n",__func__,mode);
#endif
	bct3236->led_mode_number = mode;
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
	if(!g_is_fwdl_flag) {
#endif
		queue_work(bct3236->bct_flashing_workqueue,&bct3236->bct_flashing_work);
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
	}
#endif
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
	if(bct3236->led_mode_number == LED_SOUND_MODE2 && (!g_is_fwdl_flag))
#else
    if(bct3236->led_mode_number == LED_SOUND_MODE2)
#endif
		queue_work(bct3236->bct_soundlevel_workqueue, &bct3236->bct_soundlevel_work);
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end

    pr_info("bct3236->led_mode_number is %d",bct3236->led_mode_number);   
    return len;
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_select_mode end

static ssize_t bct3236_recharge_data_show(struct device *dev,struct device_attribute *attr,char *buf)
{
   
	pr_info("%s, light_num:%d\n", __func__, bct3236->light_num);
	return sprintf(buf, "%d\n", bct3236->light_num);
 
}

static ssize_t bct3236_recharge_data(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
    int ret = 0;
	ret = sscanf(buf,"%d", &bct3236->light_num);
	pr_info("%s, light_num:%d, ret = %d\n", __func__, bct3236->light_num, ret);
    return len;
}

//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_color_level_select start
static ssize_t bct3236_color_level_select(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
	int ret, val;
	ret = sscanf(buf,"%d", &val);
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin	
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
	pr_info("%s : val = %d, g_is_fwdl_flag=%d", __func__, val,g_is_fwdl_flag);
	bct3236->color_level_select = val;
	if(1 == bct3236->sound_mode_on &&(!g_is_fwdl_flag))
		queue_work(bct3236->bct_color_workqueue,&bct3236->bct_color_work);
#else
	pr_info("%s : val = %d", __func__, val);
	bct3236->color_level_select = val;
	if(1 == bct3236->sound_mode_on)
	queue_work(bct3236->bct_color_workqueue,&bct3236->bct_color_work);
#endif
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end
	return len;
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_color_level_select end

//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_led_num_select start
static ssize_t bct3236_led_num_select(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
	int ret, val;
	ret = sscanf(buf,"%d", &val);
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue begin	
#ifdef CONFIG_AI_BSP_WIRELESS_CVS80X5
	pr_info("%s : val = %d, g_is_fwdl_flag=%d", __func__, val,g_is_fwdl_flag);
	bct3236->led_num_select = val;
	if(2 == bct3236->sound_mode_on && (!g_is_fwdl_flag))
		queue_work(bct3236->bct_color_workqueue,&bct3236->bct_color_work);
#else
	pr_info("%s : val = %d", __func__, val);
	bct3236->led_num_select = val;
	if(2 == bct3236->sound_mode_on)
	    queue_work(bct3236->bct_color_workqueue,&bct3236->bct_color_work);
#endif
//Antai <AI_BSP_CHG> <yaoyc> <2024-04-24> add for fix CVS8055 firmware upgrade fail issue end
	return len;
}
//Antai <AI_BSP_LED> <qudg> <2024-1-25> bct3236_led_num_select end

static ssize_t bct3236_delay_led_num(struct device *dev,struct device_attribute *attr,const char *buf,size_t len)
{
	int ret, val;
	ret = sscanf(buf,"%d", &val);
	pr_info("%s : val = %d", __func__, val);
	bct3236->led_num_delay = val;
	return len;
}

//Antai <AI_BSP_LED> <qudg> <2024-1-31> add color_level node start

//Antai <AI_BSP_SNS> <chenht> <2024-04-08> create file for bct3236 begin
static DEVICE_ATTR(open,0664,NULL,bct3236_mmi_test_open); //点亮所有灯level = IMax
static DEVICE_ATTR(select_color_function,0664,NULL,bct3236_selectcolor_function);
static DEVICE_ATTR(set_level_function,0664,NULL,bct3236_set_level_function);
static DEVICE_ATTR(sound_mode,0664,NULL,bct3236_sound_mode);
static DEVICE_ATTR(select_mode,0664,NULL,bct3236_select_mode);
static DEVICE_ATTR(recharge_data,0664,bct3236_recharge_data_show,bct3236_recharge_data);
static DEVICE_ATTR(color_level_select,0664,NULL,bct3236_color_level_select);
static DEVICE_ATTR(led_num_select,0664,NULL,bct3236_led_num_select);
static DEVICE_ATTR(delay_led_num,0664,NULL,bct3236_delay_led_num);
/*
static struct attribute *bct3236_led_attributes[] = {
    &dev_attr_open.attr,
    &dev_attr_select_color_function.attr,
    &dev_attr_set_level_function.attr,
    &dev_attr_sound_mode.attr,
    &dev_attr_select_mode.attr,
    &dev_attr_recharge_data.attr,
    &dev_attr_color_level_select.attr,
    &dev_attr_led_num_select.attr,
    &dev_attr_delay_led_num.attr,
    NULL
};
	
//Antai <AI_BSP_LED> <qudg> <2024-1-31> add color_level node end
static struct attribute_group bct3236_led_attribute_group = {
	.attrs = bct3236_led_attributes
};
*/

static struct device_attribute *pickup_light_attr_list[] = {
    &dev_attr_open,
    &dev_attr_select_color_function,
    &dev_attr_set_level_function,
    &dev_attr_sound_mode,
    &dev_attr_select_mode,
    &dev_attr_recharge_data,
    &dev_attr_color_level_select,
    &dev_attr_led_num_select,
    &dev_attr_delay_led_num,
};
static int led_create_attr(struct device *dev)
{
    int idx, err = 0;
    int num = (int)(sizeof(pickup_light_attr_list)/sizeof(pickup_light_attr_list[0]));
	
    if (dev == NULL)
    {
        return -EINVAL;
    }
	pr_info("%s Enter----\n", __func__);
    for(idx = 0; idx < num; idx++)
    {
        err = device_create_file(dev, pickup_light_attr_list[idx]);
        if(err)
        {
            pr_err("%s driver_create_file failed\n", __func__);
            break;
        }
    }
    pr_info("%s driver_create_file success\n", __func__);
    return err;
}

static struct platform_device ai_led_pickup_light_device = {
       .name   = "led_pickup_light",
       .id     = -1,
};
//Antai <AI_BSP_SNS> <chenht> <2024-04-08> create file for bct3236 end
//Antai <AI_BSP_SNS> <wutj> <2022-09-23> add mmi test attr start
static int bct3236_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
   // *np = client->dev.of_node;
    int ret = -1;
    pr_info("%s Enter\n", __func__);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "%s: check_functionality failed\n", __func__);
        ret = -ENODEV;
        goto exit_check_functionality_failed;
    }

    bct3236 = devm_kzalloc(&client->dev, sizeof(struct bct3236_t), GFP_KERNEL);
    if (bct3236 == NULL) {
        ret = -ENOMEM;
        goto exit_devm_kzalloc_failed;
    }
    led_msg_init();

	//Antai <AI_BSP_LED> <qudg> <2024-1-25> add bct_color_workqueue start
    //赋值
    bct3236->brightness = 0;
    bct3236->level = 1;
    bct3236->led_mode_number = 0;
    bct3236->light_num = 0;
    bct3236->init_flag = 1;
    bct3236->i2c_client = client;
    bct3236->mmi_flag = false;
    bct3236->sound_mode_flag = 0;
	bct3236->sound_mode_on = 0;
	bct3236->color_level_select = 0;
	bct3236->led_num_select = 0;
	bct3236->led_num_delay = 0;
    i2c_set_clientdata(client, bct3236);
	/*Create workqueyue*/
	bct3236->bct_mmi_workqueue =
	create_singlethread_workqueue("bctmmi");
	INIT_WORK(&bct3236->bct_mmi_work, open_all_light);
	if (!bct3236->bct_mmi_workqueue) {
		pr_info("Error: Create dinit workqueue failed\n");
		ret = -1;
		goto err_device_create;
	}
	
    bct3236->bct_soundlevel_workqueue =
	create_singlethread_workqueue("bctcolor");
	INIT_WORK(&bct3236->bct_soundlevel_work, bct_sound_level_mode);
	if (!bct3236->bct_soundlevel_workqueue) {
		pr_info("Error: Create dinit workqueue failed\n");
		ret = -1;
		goto err_device_create;
	}

    bct3236->bct_color_workqueue =
	 create_singlethread_workqueue("bctsendcolor");
	INIT_WORK(&bct3236->bct_color_work, music_rhythm_mode);
	if (!bct3236->bct_color_workqueue) {
		pr_info("Error: Create dinit workqueue failed\n");
		ret = -1;
		goto err_device_create;
	}
	//Antai <AI_BSP_LED> <qudg> <2024-1-25> add bct_color_workqueue end
	
    // bct3236->bct_send_level_workqueue =
	// create_singlethread_workqueue("bctsendlevel");
	// INIT_WORK(&bct3236->bct_send_level_work, led_send_level_msg);
	// if (!bct3236->bct_send_level_workqueue) {
	// 	pr_info("Error: Create dinit workqueue failed\n");
	// 	ret = -1;
	// 	goto err_device_create;
	// }

    bct3236->bct_flashing_workqueue =
	create_singlethread_workqueue("bctflashing");

	INIT_WORK(&bct3236->bct_flashing_work, light_on);

	if (!bct3236->bct_flashing_workqueue) {
		pr_info("Error: Create dinit workqueue failed\n");
		ret = -1;
		goto err_device_create;
	}

    bct3236->wake_lock = wakeup_source_create("bct_wake_lock");
    if (!bct3236->wake_lock) {
        pr_err("wakeup_source_create failed.");
        goto err_device_create;
	} else {
        wakeup_source_add(bct3236->wake_lock);
	}
	
    //Antai <AI_BSP_SNS> <wutj> <2022-09-23> add mmi test attr start
	//Antai <AI_BSP_SNS> <chenht> <2024-04-08> create file for bct3236 begin
	/*
    ret = sysfs_create_group(&client->dev.kobj, &bct3236_led_attribute_group);
	if (ret < 0) {
		dev_err(&client->dev, "error creating sysfs attr files");
		goto err_sysfs;
	}
	*/
	ret = platform_device_register(&ai_led_pickup_light_device);
	if (ret < 0) {
		dev_err(&client->dev, "create ai_led_pickup_light_device failed\n");
		goto err_sysfs;
	}

	ret = led_create_attr(&(ai_led_pickup_light_device.dev));
	if (ret < 0) {
		dev_err(&client->dev, "create ai_led_pickup_light attr failed\n");
		goto err_sysfs;
	}
	//Antai <AI_BSP_SNS> <chenht> <2024-04-08> create file for bct3236 end
	
    led_shutdown();
    return 0;
err_device_create:
exit_devm_kzalloc_failed:
exit_check_functionality_failed:
err_sysfs:
	return ret;
	//Antai <AI_BSP_SNS> <wutj> <2022-09-23> add mmi test attr start
}

static int bct3236_i2c_remove(struct i2c_client *client)
{
	if(bct3236 == NULL) 
		return 0;
		
    led_shutdown();
    return 0;
}

static void bct3236_shutdown(struct i2c_client *client)
{
	pr_info("%s enter\n",__func__);
	if(bct3236 == NULL) 
		return ;
		
	bct3236->led_mode_number = 0;
	bct3236->sound_mode_on = 0;
    led_shutdown();
}

static const struct i2c_device_id bct3236_i2c_id[] = {
    { BCT3236_I2C_NAME, 0 },
    { }
};


static const struct of_device_id extpa_of_match[] = {
    {.compatible = "jx,bct3236-i2c"},
    {},
};


static struct i2c_driver bct3236_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = BCT3236_I2C_NAME,
        .of_match_table = extpa_of_match,
    },
    .probe = bct3236_i2c_probe,
    .remove = bct3236_i2c_remove,
    .shutdown = bct3236_shutdown,
    .id_table    = bct3236_i2c_id,
};

static int __init bct3236_init(void) {

    int ret;
    pr_info("%s enter\n", __func__);
    pr_info("%s: driver version: %s\n", __func__, BCT3236_DRIVER_VERSION);

    ret = i2c_add_driver(&bct3236_i2c_driver);
    if (ret) {
        pr_info("****[%s] Unable to register driver (%d)\n",
                __func__, ret);
        return ret;
    }
    else
        pr_info("bct3236 is seccuessful");
		

    return 0;
}

static void __exit bct3236_exit(void) {
    pr_info("%s enter\n", __func__);
    led_shutdown();
    i2c_del_driver(&bct3236_i2c_driver);
}

module_init(bct3236_init);
module_exit(bct3236_exit);
MODULE_DESCRIPTION("bct3236 driver");
MODULE_LICENSE("GPL");
