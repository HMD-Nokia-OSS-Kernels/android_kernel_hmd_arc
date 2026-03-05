#include <stdio.h>
#include <sprd_common.h>
#include <i2c.h>
#include <lk/debug.h>
#include <sprd_sensor.h>

static u32 default_value;

char sensorname[SENSOR_NAME_LEN] = {0};

static void sprd_set_i2c_matrix(void)
{
    default_value = readl(CONFIG_SENSOR_I2C_MATRIX_BASE);
    writel(CONFIG_SENSOR_I2C_MATRIX_VALUE, CONFIG_SENSOR_I2C_MATRIX_BASE);
}

static void sprd_recover_i2c_matrix(void)
{
    writel(default_value, CONFIG_SENSOR_I2C_MATRIX_BASE);
}

unsigned int sprd_sensor_get_id_name(int bus_num, u8 addr, u16 reg, u8 * value)
{
    int ret;
    u8 reg_addr[2] = {0};

    reg_addr[0] = reg & 0xFF;
    reg_addr[1] = (reg >> 8) & 0xFF;
    ret = i2c_read_write(bus_num, addr, reg_addr, 1, value, 1);
    if (ret < 0) {
        dprintf(INFO, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
        return ret;
    }

    return 0;
}

static void sprd_sensor_name_to_kernel(void)
{
    static int sensor_num, i, ret;
    u8 tmp = 0;
    struct sensor_phypara *sensor_phylist_info = NULL;

    sprd_set_i2c_matrix();
    sensor_num = sprd_sensor_bandlist(&sensor_phylist_info);
    for (i = 0; i < sensor_num; i++) {
        ret = sprd_sensor_get_id_name(sensor_phylist_info[i].bus_num, sensor_phylist_info[i].sensor_slave_addr,
                                      sensor_phylist_info[i].sensor_reg, &tmp);
	if (ret == 0) {
		if (tmp == sensor_phylist_info[i].sensor_id) {
			strcat(sensorname, sensor_phylist_info[i].sensor_name);
			strcat(sensorname, ",");
		} else {
			dprintf(INFO, "%s hard fault, error tmp(%x) id(%x)\n", __func__, tmp, sensor_phylist_info[i].sensor_id);
		}
	}
    }
    sprd_recover_i2c_matrix();
}

const char *load_sensor_to_kernel(void)
{
    sprd_sensor_name_to_kernel();

    if (sensorname[0])
        return sensorname;
    else
        return NULL;
}

