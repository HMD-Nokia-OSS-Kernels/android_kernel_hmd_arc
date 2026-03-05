#ifndef _LOGO_BIN_H_
#define _LOGO_BIN_H_

#include <linux/types.h>

#define LOGO_NORMAL_POWER 		3
#define LOGO_LOW_VOL 			0
#define LOGO_LOW_BATTERY_CHARGING	1
#define LOGO_POWEROFF_CHARGE 		4

#define LOGO_TYPE_BMP 			0
#define LOGO_TYPE_BIN 			1
#define LOGO_MAX_BPP 			4

#define LOW_VOL_DISPLAY_DELAY_TIME 	3000

#define LCD_DISPLAY_ENABLE		1

#define LOGO_PART "logo"

#define FONT_RED 0xff0000
#define FONT_GREEN 0x00ff00
#define FONT_BLUE 0x0000ff

extern int logo_type;
extern int logo_index;
extern void *bmp_base;
extern int panel_enabled;

struct header_info {
    char signature[2];
    uint16_t file_number;
    uint32_t reserved_num[5];
    uint32_t gz_size[0];
};

/*
*save font infomation
* @size fonts size (24 or 32)
* @color fonts color
* @st_x strings position of x [0-100)
* @st_y strings position of y [0-45)
*/
struct sprd_font_info {
	u16 size;
	u16 reserved;
	u32 color;
	int st_x;
	int st_y;
};

void logo_display(int index, int backlight_value, int lcd_on);
int bmp_get_bpix(ulong addr);
int get_bmp_base_from_dt(void **bmp_base);
int get_gzip_base_from_dt(void **gzip_base);
int logo_mem_init(void);
int get_logo_bin_info(u8 *bmp, char *logo_part_name);

#endif