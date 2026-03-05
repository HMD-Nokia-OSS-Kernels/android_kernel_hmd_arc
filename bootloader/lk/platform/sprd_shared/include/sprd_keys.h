#ifndef _SPRD_KEYS_H
#define _SPRD_KEYS_H

#include <boot_mode.h>

#ifndef PWR_KEY_DETECT_CNT
#define PWR_KEY_DETECT_CNT 2
#endif

#define KEY_PRESSED		0
#define KEY_NOT_PRESSED		1
#define KEY_RESERVED		0
#define KEY_HOME		102
#define KEY_VOLUMEDOWN		114
#define KEY_VOLUMEUP		115
#define KEY_VOLUMEUP_PRESS		1
#define KEY_VOLUMEDOWN_PRESS	0

#ifdef SPRD_VOLUMEDOWN_GPIO
#define SPRD_VOLUME_GPIO	SPRD_VOLUMEDOWN_GPIO
#elif defined(SPRD_VOLUMEUP_GPIO)
#define SPRD_VOLUME_GPIO	SPRD_VOLUMEUP_GPIO
#endif
//ZOVERLAY_TAG_HMD_ONEIMAGE

#ifdef CONFIG_CUSTOMER_PHONE
static char keypad_mode[2][3] = {
	{CMD_RECOVERY_MODE, KEY_VOLUMEUP, 0},
	{CMD_FASTBOOT_MODE, KEY_VOLUMEDOWN, 0},
};
#else
static char keypad_mode[2][3] = {
	{CMD_USB_MUX_MODE, KEY_VOLUMEUP, KEY_VOLUMEDOWN},
	{CMD_FASTBOOT_MODE, KEY_VOLUMEUP, 0},
};
#endif

void board_keypad_init(void);
void keypad_volume_type(int key, char *key_type);
void keypad_eic_get(int *key_code, int key, unsigned pin, int key_volume);
void keypad_gpio_get(int *key_code, int key, int key_volume);
int board_key_scan(void);
int power_button_pressed(void);
unsigned int check_key_boot(unsigned char key);
int boot_pwr_check(void);

#endif
