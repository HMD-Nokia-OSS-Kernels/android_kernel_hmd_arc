/*
 *  <sprd_panel.c> - <sprd panel>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2021-08-09> <pony.wu@unisoc.com>
 *
 *      The above copyright notice shall be
 *      included in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "sprd_panel.h"
#ifdef CONFIG_UDC
#include <udc.h>
#include <chipram_env.h>
#endif
#include "sprd_dsi.h"
#include "sprd_dphy.h"
#include "lcd/panel_cfg.h"
#include "string.h"
#include "vibrator.h"
#include "umb9230s/umb9230s.h"

#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))

static uint32_t lcd_id_to_kernel;
static struct panel_driver *panel_drv;

#ifdef CONFIG_BACKLIGHT_DSI
void set_backlight(uint32_t brightness)
{
	if (panel_drv && panel_drv->ops->set_brightness) {
		panel_drv->ops->set_brightness(brightness);
	}
	pr_info("lcd cabc backlight brightness==%d\n",brightness);
	return;
}
#endif

struct panel_info *panel_info_attach(void)
{
	return panel_drv->info;
}

uint32_t lcd_get_pixel_clock(void)
{
	if (panel_drv && panel_drv->info)
		return panel_drv->info->pixel_clk;
	else
		return 0;
}

uint32_t lcd_get_bpix(void)
{
	return panel_info.vl_bpix;
}

const char *lcd_get_name(void)
{
	if (panel_drv && panel_drv->info)
		return panel_drv->info->lcd_name;
	else
		return NULL;
}

uint32_t load_lcd_id_to_kernel(void)
{
	return lcd_id_to_kernel;
}

/* WORKAROUND: to keep the same order with sprdfb_panel.c */
uint32_t load_lcd_width_to_kernel(void)
{
	if (panel_drv && panel_drv->info)
		return panel_drv->info->height;
	else
		return 0;
}

/* WORKAROUND: to keep the same order with sprdfb_panel.c */
uint32_t load_lcd_hight_to_kernel(void)
{
	if (panel_drv && panel_drv->info)
		return panel_drv->info->width;
	else
		return 0;
}

static void check_idle_state(void)
{
#ifdef CONFIG_DISP_UMB9230S
	umb9230s_wait_idle_state(&umb9230s_dev);
#endif
}

static int panel_if_init(void)
{
	int type = panel_drv->info->type;

	switch (type) {
	case SPRD_PANEL_TYPE_MIPI:
#ifdef CONFIG_DISP_UMB9230S
		umb9230s_module_probe();
#endif
		sprd_dsi_probe();
		sprd_dphy_probe();

		return 0;

	case SPRD_PANEL_TYPE_RGB:
		return 0;

	default:
		pr_err("doesn't support current interface type %d\n", type);
		return -1;
	}
}

static int panel_slave_if_init(void)
{
	int type = panel_drv->info->type;

	switch (type) {
	case SPRD_PANEL_TYPE_MIPI:
		sprd_dsi_slave_probe();
		sprd_dphy_slave_probe();
		return 0;

	case SPRD_PANEL_TYPE_RGB:
		return 0;

	default:
		pr_err("panel slave doesn't support current interface type %d\n", type);
		return -1;
	}
}

static int panel_if_uinit(void)
{
	int type = panel_drv->info->type;

	switch (type) {
	case SPRD_PANEL_TYPE_MIPI:
#ifdef CONFIG_DISP_UMB9230S
		umb9230s_tx_module_suspend(&umb9230s_dev);
#endif
		sprd_dphy_suspend(&dphy_device);
		sprd_dsi_suspend(&dsi_device);
		return 0;

	case SPRD_PANEL_TYPE_RGB:
		return 0;

	default:
		pr_err("doesn't support current interface type %d\n", type);
		return -1;
	}
}



#ifdef CONFIG_UDC
void set_panel_sleep_in(void)
{
	struct panel_ops *ops;
	struct panel_info *info;
	int i;
	int ret;

	//for (i = 0; i < ARRAY_SIZE(lcd_panel); i++) {
		panel_drv = lcd_panel[0].drv;
		info = panel_drv->info;
		ops = panel_drv->ops;



		if (ops && ops->read_id) {
			ret = ops->read_id(info);
			if (!ret) {
				pr_info("attach panel 0x%x success\n",
					lcd_panel[0].lcd_id);
				if(ops && ops->sleep_in) {
					ops->sleep_in();
				}
			}
		}
	//}
	return;
}
#else
	
void set_panel_sleep_in(void)
{
	struct panel_ops *ops;
	struct panel_info *info;
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(supported_panel); i++) {
		panel_drv = supported_panel[i].drv;
		info = panel_drv->info;
		ops = panel_drv->ops;

#ifdef CONFIG_DISP_UMB9230S
		ret = umb9230s_check_lcd_compatibility(i, ARRAY_SIZE(supported_panel));
		if (ret) {
			pr_err("skip panel 0x%x, try next.\n", supported_panel[i].lcd_id);
			continue;
		}
#endif

		if (ops && ops->read_id) {
			ret = ops->read_id(info);
			if (!ret) {
				pr_info("attach panel 0x%x success\n",
					supported_panel[i].lcd_id);
				if(ops && ops->sleep_in) {
					ops->sleep_in();
				}
			}
		}
	}
	return;
}


#endif

//add by jinqiang for udc 
#ifdef CONFIG_UDC
int sprd_panel_probe(void)
{
	struct panel_info *info;
	struct panel_ops *ops;
	//int i = 0;
	uint32_t id;	
	uint16_t sec_id = SEC_LCD0;
	uint16_t ret = 0;
	boot_mode_t boot_role;
	chipram_env_t* cr_env = get_chipram_env();
	boot_role = cr_env->mode;
	printf("sprd_panel_probe: boot_role = 0x%x\n", boot_role);
	if(boot_role == BOOTLOADER_MODE_DOWNLOAD)
	{
		printf("sprd_panel_probe: download mode\r\n");
		return NULL;
	}
	g_sc9850_udc_lcd = udc_lcd_create(SEC_LCD0, &lcd_panel[0]); 

	while(1)
	{        
		ret = udc_lcd_config_panel(g_sc9850_udc_lcd, sec_id++);
		
		panel_drv = lcd_panel[0].drv;
		info = panel_drv->info;
		ops = panel_drv->ops;
		
		
////////////////read id start/////////////////////////////////////////////		
		panel_if_init();

		if (ops && ops->power)
		{
			printf("sprd_panel_probe: start ops->power\n");
			ops->power(true);
		}
  		//panel_reset(true);
  		printf("sprd_panel_probe: ret = %d\n", ret);
		if(!ret)//not find lcd then config the last lcd to panle
		{
		  id = 0xffff;
		  break;
		}
		else
		{
		  id = ops->read_id(info);
		}
		printf("sprd_panel_probe: id = 0x%x, lcd_panel[0].lcd_id = 0x%x\n", id, lcd_panel[0].lcd_id);

		if(id == lcd_panel[0].lcd_id) 
		{
		 break;
	 	}
		//panel_reset(false);
		//if (ops && ops->power)
			//ops->power(false);

		//panel_if_uinit();

		pr_err("attach panel 0x%x failed, try next...\n",
			lcd_panel[0].lcd_id);
	}

	if (ops && ops->init)
		ops->init();

	panel_info.vl_row = info->height;
	panel_info.vl_col = info->width;
	lcd_id_to_kernel = lcd_panel[0].lcd_id;///supported_panel[i].lcd_id    add by jinq for script modify
	return 0;
}
#else
int sprd_panel_probe(void)
{
	struct panel_info *info;
	struct panel_ops *ops;
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(supported_panel); i++) {
		panel_drv = supported_panel[i].drv;
		info = panel_drv->info;
		ops = panel_drv->ops;

#ifdef CONFIG_DISP_UMB9230S
		ret = umb9230s_check_lcd_compatibility(i, ARRAY_SIZE(supported_panel));
		if (ret) {
			pr_err("skip panel 0x%x, try next.\n", supported_panel[i].lcd_id);
			continue;
		}
#endif

		panel_if_init();

		if (info->dual_dsi_en)
			panel_slave_if_init();

#ifdef CONFIG_SC27XX_VDDVIB_VSD2
		set_vibrator(1);
		mdelay(50);
#endif

		if (ops && ops->power)
			ops->power(true);
		if (ops && ops->read_id) {
			ret = ops->read_id(info);
			if (!ret) {
				pr_info("attach panel 0x%x success\n",
					supported_panel[i].lcd_id);
				break;
			}
		}

		check_idle_state();

		if (ops && ops->power)
			ops->power(false);

		panel_if_uinit();

		pr_err("attach panel 0x%x failed, try next...\n",
			supported_panel[i].lcd_id);
	}

	if (ops && ops->init)
		ops->init();

	panel_info.vl_row = info->height;
	panel_info.vl_col = info->width;
	lcd_id_to_kernel = supported_panel[i].lcd_id;

	return 0;
}
#endif