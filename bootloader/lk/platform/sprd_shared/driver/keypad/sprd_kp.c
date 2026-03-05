#include <malloc.h>
#include <asm/arch/mfp.h>
#include <asm/arch/common.h>
#include <eic.h>
#include <gpio.h>
#include <sprd_types.h>
#include <lk/debug.h>
#include <linux/kernel.h>
#include <sprd_common.h>
#include <sprd_keys.h>
#include <asm/arch/sprd_eic.h>
#include <lk/board.h>

extern int enter_sysdump_flag;

void board_keypad_init(void)
{
#ifdef SPRD_VOLUME_GPIO
	sprd_gpio_request(SPRD_VOLUME_GPIO);
	sprd_gpio_direction_input(SPRD_VOLUME_GPIO);
	dprintf(INFO, "[gpio keys] init!\n");
#else
	dprintf(INFO, "[gpio keys] init skip!\n");
#endif
}

void keypad_volume_type(int key, char *key_type)
{
	if (key == KEY_VOLUMEUP) {
		strcpy(key_type, "up");
	}
	else if(key == KEY_VOLUMEDOWN) {
		strcpy(key_type, "down");
	}
	else {
		errorf("[keypads] the key type set error!!!\n");
	}
}

void keypad_eic_get(int *key_code, int key, unsigned pin, int key_volume)
{
	int eic_volume = -1;
	char key_type[8] = {0};
	unsigned eic_pin = EIC_KEY2_7S_RST_EXT_RSTN_ACTIVE;

	if(pin != 0)
		eic_pin = pin;

	*key_code = KEY_RESERVED;
	keypad_volume_type(key, key_type);
	sprd_eic_request(eic_pin);
	udelay(3000);
	eic_volume = sprd_eic_get(eic_pin);
	if(eic_volume < 0) {
		errorf("[eic keys] volume%s : sprd_eic_get return ERROR!\n", key_type);
		*key_code = -1;
	} else if(eic_volume == key_volume) {
		debugf("[eic keys] volume%s pressed!\n", key_type);
		*key_code = key;
	}
}

void keypad_gpio_get(int *key_code, int key, int key_volume)
{
	int gpio_volume = -1;
	char key_type[8] = {0};

	*key_code = KEY_RESERVED;

	keypad_volume_type(key, key_type);
#ifdef SPRD_VOLUME_GPIO
	gpio_volume = sprd_gpio_get(SPRD_VOLUME_GPIO);
#endif
	if(gpio_volume < 0) {
		errorf("[gpio keys] volume%s : sprd_eic_get return ERROR!\n", key_type);
		*key_code = -1;
	} else if (gpio_volume == key_volume) {
		debugf("[gpio keys] volume%s pressed!\n", key_type);
		*key_code = key;
	}
}

int board_keypad_get(int *key1, int *key2)
{
	int ret = 0;

#ifdef SPRD_VOLUMEKEY_EIC
	keypad_eic_get(key1, KEY_VOLUMEUP, EIC_KEY2_7S_RST_EXT_RSTN_ACTIVE, KEY_VOLUMEUP_PRESS); //volumeup use eic SPRD_DDIE_EIC_EXTINT2
	keypad_eic_get(key2, KEY_VOLUMEDOWN, SPRD_DDIE_EIC_EXTINT2, KEY_VOLUMEDOWN_PRESS);	//volumedown use eic EIC_KEY2_7S_RST_EXT_RSTN_ACTIVE
#ifdef ZCFG_LK_KEY_VOLUMEDOWN_CUST
	if(*key2 == KEY_RESERVED)
		keypad_eic_get(key2, KEY_VOLUMEDOWN, SPRD_DDIE_EIC_EXTINT3, KEY_VOLUMEDOWN_PRESS);
#endif
	/*
	if(*key2 == 0) {     //key_down eic inverse
		*key2 = KEY_VOLUMEDOWN;
	}else{
		*key2 = 0;
	}
	*/
#elif SPRD_VOLUMEUP_GPIO
	keypad_eic_get(key1, KEY_VOLUMEDOWN, 0, KEY_VOLUMEDOWN_PRESS);	//volumeup use eic
	keypad_gpio_get(key2, KEY_VOLUMEUP, KEY_VOLUMEUP_PRESS);	//volumedown use gpio
#elif defined(SPRD_VOLUMEDOWN_GPIO)
	keypad_eic_get(key1, KEY_VOLUMEUP, 0, KEY_VOLUMEUP_PRESS);	//volumeup use eic
	keypad_gpio_get(key2, KEY_VOLUMEDOWN, KEY_VOLUMEDOWN_PRESS);	//volumedown use gpio
#elif defined(SPRD_DDIE_EIC_EXTINT2)
	keypad_eic_get(key1, KEY_VOLUMEUP, SPRD_DDIE_EIC_EXTINT2, KEY_VOLUMEUP_PRESS); //volumeup use eic SPRD_DDIE_EIC_EXTINT2
	keypad_eic_get(key2, KEY_VOLUMEDOWN, 0, KEY_VOLUMEDOWN_PRESS);	//volumedown use eic EIC_KEY2_7S_RST_EXT_RSTN_ACTIVE
#else
	errorf("[keypads] parameter configuration missing!!!\n");
#endif

	if(*key1 < 0 || *key2 < 0) {
		ret = -1;
	}

	return ret;
}

int board_key_scan(void)
{
	int key_code = KEY_RESERVED;
	int key1 = -1;
	int key2 = -1;
	int ret = 0;

	ret = board_keypad_get(&key1, &key2);
	if(!ret) {
		key_code = key1 + key2;
		if (KEY_RESERVED == key_code) {
			if (enter_sysdump_flag)
				uprintf("no key pressed!\n");
			else
				debugf("no key pressed!\n");
		}
	} else {
		errorf("the keypad is error!!!!!\n");
	}

	return key_code;
}

int power_button_pressed(void)
{
	int eic_value,ret;
//maybe get button status from eic API is batter
	sprd_eic_request(EIC_PBINT);
	udelay(3000);
	eic_value = sprd_eic_get(EIC_PBINT);
	debugf("power_button_pressed status %x\n", eic_value);

#ifdef CONFIG_POWERKEY_DEFAULT_HIGH
	if (eic_value == 0)
		ret = KEY_PRESSED;
	else
		ret = KEY_NOT_PRESSED;
#else
	if (eic_value == 0)
		ret = KEY_NOT_PRESSED;
	else
		ret = KEY_PRESSED;
#endif

	return ret;
}

/*This function was used in unlock phone to get power button status */
int power_button_status(void)
{
	static int eic_value = KEY_NOT_PRESSED;
	static int last_value = KEY_NOT_PRESSED;
	char status[12];
	int ret;

	eic_value = sprd_eic_get(EIC_PBINT);

#ifdef CONFIG_POWERKEY_DEFAULT_HIGH
	if (eic_value == 0) {
		strcpy(status, "PRESSED");
		ret = KEY_PRESSED;
	} else {
		strcpy(status, "NO_PRESSED");
		ret = KEY_NOT_PRESSED;
	}
#else
	if (eic_value == 0) {
		strcpy(status, "NO_PRESSED");
		ret = KEY_NOT_PRESSED;
	} else {
		strcpy(status, "PRESSED");
		ret = KEY_PRESSED;
	}
#endif

	if(eic_value != last_value)
		debugf("power_button_status last_value %x, current_value %x: %s\n", last_value, eic_value, status);
	else
		dprintf(ALWAYS, "power_button_status %x, %s\n", eic_value, status); //only print log by uart, can't save

	last_value = eic_value;

	return ret;
}

int boot_pwr_check(void)
{
	static int total_cnt = 0;

	if (!power_button_pressed())
		total_cnt ++;

	return total_cnt;
}

unsigned int check_key_boot(unsigned char key)
{
	for(int i=0; i < ARRAY_SIZE(keypad_mode); i++) {
		if (key == (keypad_mode[i][1] + keypad_mode[i][2]))
			return keypad_mode[i][0];
	}

	return CMD_UNDEFINED_MODE;
}
