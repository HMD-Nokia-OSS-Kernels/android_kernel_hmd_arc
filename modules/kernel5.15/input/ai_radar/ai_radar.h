#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/leds.h>

#define RADAR_SUCCESS		0
#define BIN_NUM_MAX   100
#define FLASH_32KB 32768

#define K60168_WAKELOCK_TIMEOUT         2000

enum k60168_irq_trigger_gesture { TRIGGER_0 = 0x0, TRIGGER_1 = 0x1, TRIGGER_2 = 0x2, TRIGGER_3 = 0x3,TRIGGER_4 = 0xff,};

enum k60168_err_flags {
	MALLOC_FAILED = 200,
	CHIPID_FAILED = 201,
	IRQIO_FAILED = 202,
	IRQ_REQUEST_FAILED = 203,
	INPUT_ALLOCATE_FILED = 204,
	INPUT_REGISTER_FAILED = 205,
	CFG_LOAD_TIME_FAILED = 206,
	TRIM_ERROR = 207,
	CHANNEL_FUNC_ERR,
};

enum k60168_i2c_flags {
	K60168_I2C_WR = 0,
	K60168_I2C_RD = 1,
};

struct bin_container {
	unsigned int len; /* The size of the bin file obtained from the firmware */
	unsigned char data[]; /* Store the bin file obtained from the firmware */
};

struct bin_header_info {
	unsigned int header_len; /* Frame header length */
	unsigned int check_sum; /* Frame header information-Checksum */
	unsigned int header_ver; /* Frame header information-Frame header version */
	unsigned int bin_data_type; /* Frame header information-Data type */
	unsigned int bin_data_ver; /* Frame header information-Data version */
	unsigned int bin_data_len; /* Frame header information-Data length */
	unsigned int ui_ver; /* Frame header information-ui version */
	unsigned char chip_type[8]; /* Frame header information-chip type */
	unsigned int reg_byte_len; /* Frame header information-reg byte len */
	unsigned int data_byte_len; /* Frame header information-data byte len */
	unsigned int device_addr; /* Frame header information-device addr */
	unsigned int valid_data_len; /* Length of valid data obtained after parsing */
	unsigned int valid_data_addr; /* The offset address of the valid data obtained after parsing relative to info */

	unsigned int reg_num; /* The number of registers obtained after parsing */
	unsigned int reg_data_byte_len; /* The byte length of the register obtained after parsing */
	unsigned int download_addr; /* The starting address or download address obtained after parsing */
	unsigned int app_version; /* The software version number obtained after parsing */
};

struct k60168_bin {
	char *p_addr; /* Offset pointer (backward offset pointer to obtain frame header information and important information) */
	unsigned int all_bin_parse_num; /* The number of all bin files */
	unsigned int multi_bin_parse_num; /* The number of single bin files */
	unsigned int single_bin_parse_num; /* The number of multiple bin files */
	struct bin_header_info header_info[BIN_NUM_MAX]; /* Frame header information and other important data obtained after parsing */
	struct bin_container info; /* Obtained bin file data that needs to be parsed */
};

struct k60168 {
	uint8_t cali_flag;
	uint8_t node;
	const char *chip_name;

	int32_t irq_gpio;
	int32_t to_irq;
	uint32_t irq_status;
	uint32_t hostirqen;
	uint32_t first_irq_flag;
	uint32_t spedata[8];
	uint32_t nvspe_data[8];
	uint32_t channel_func;
	bool pwprox_dete;
	bool firmware_flag;
	signed rst_gpio;
	signed pwr_gpio;
	signed enable_gpio;
	

	struct workqueue_struct *ts_workqueue;
	struct work_struct fwupg_work;
	
	struct delayed_work cfg_work;
	struct i2c_client *i2c;
	struct device *dev;
	struct input_dev *input;
	struct delayed_work dworker; /* work struct for worker function */
//	struct wakeup_source wake_lock;
	struct wakeup_source *radar_ws;
//	struct k60168_pinctrl pinctrl;
};
