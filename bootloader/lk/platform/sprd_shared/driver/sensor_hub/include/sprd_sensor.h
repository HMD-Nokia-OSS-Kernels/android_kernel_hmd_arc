#ifndef __SPRD_SENSOR_H__
#define __SPRD_SENSOR_H__

#include <asm/arch/common.h>

#define I2C_SPEED		(100000)
#define SENSOR_NAME_LEN		320
#define SENSOR_LEN_MAX		8
#define SENSOR_I2C_MAX		2

struct sensor_phypara {
	u8 sensor_slave_addr;
	u16 sensor_reg;
	u16 sensor_id;
	const char *sensor_name;
	int bus_num;
};

int sprd_sensor_bandlist(struct sensor_phypara **sensorlist);

#endif
