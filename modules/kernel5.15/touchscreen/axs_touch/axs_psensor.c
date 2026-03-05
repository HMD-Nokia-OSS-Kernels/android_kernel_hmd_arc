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
#if AXS_PROXIMITY_SENSOR_EN

static struct mutex psensor_mutex;

void axs_report_psensor_state(struct input_dev *input_dev, u8 state)
{

	//warning:Implement this function
	printk("axs_report_psensor_state %d \n" , state);
    mutex_lock(&psensor_mutex);
    input_report_abs(input_dev, ABS_DISTANCE, state?0:1);
    #if !AXS_MT_PROTOCOL_B_EN
		input_mt_sync(input_dev);
	#endif
    input_sync(input_dev);
    axs_release_all_finger();
    mutex_unlock(&psensor_mutex);
}

int axs_read_psensor_enable(u8 *pEnable)
{
    u8 cmd_type[1] = {AXS_REG_PSENSOR_READ};
    printk("axs_read_psensor_enable %d", pEnable);
    return axs_read_regs(cmd_type, 1, pEnable, 1);
}
int axs_write_psensor_enable(u8 enable)
{   
    int ret = 0;
    u8 cmd_type[2] = {AXS_REG_PSENSOR_WRITE,enable};
    mutex_lock(&psensor_mutex);
    ret = axs_write_buf(cmd_type, 2);
    mutex_unlock(&psensor_mutex);
    return ret;
}
#endif

#if AXS_PROXIMITY_SENSOR_SHUB_EN

int axs_read_psensor_enable(u8 *pEnable)
{
    u8 cmd_type[1] = {AXS_REG_PSENSOR_READ};
    printk("axs_read_psensor_enable %d", pEnable);
    return axs_read_regs(cmd_type, 1, pEnable, 1);
}
int axs_write_psensor_enable(u8 enable)
{   
    int ret = 0;
    u8 cmd_type[2] = {AXS_REG_PSENSOR_WRITE,enable};

    ret = axs_write_buf(cmd_type, 2);

    return ret;
}
#endif