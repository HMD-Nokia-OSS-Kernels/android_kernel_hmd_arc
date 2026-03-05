/*
 *  <panel_cfg.h> - <panel configure>
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

#include "../sprd_panel.h"

extern struct panel_driver ft8006p_easyquick_driver;
extern struct panel_driver ft8006p_hlt_driver;
extern struct panel_driver ft8006p_huaxian_driver;
extern struct panel_driver dummy_mipi_driver;
extern struct panel_driver icnl9911_hlt_driver;
extern struct panel_driver icnl9911_txd_driver;
extern struct panel_driver ili9881c_truly_driver;
extern struct panel_driver ili7807s_tianma_driver;
extern struct panel_driver ili9881c_3lane_driver;
extern struct panel_driver ili9881c_skyworth_driver;
extern struct panel_driver ssd2092_truly_driver;
extern struct panel_driver st7701_boe_driver;
extern struct panel_driver st7701_coe_driver;
extern struct panel_driver nt35532_truly_driver;
extern struct panel_driver nt35695_truly_driver;
extern struct panel_driver nt35596_boe_driver;
extern struct panel_driver nt35597_boe_driver;
extern struct panel_driver nt35597_fpga_driver;
extern struct panel_driver jd9161_xxx_driver;
extern struct panel_driver jd9365z_king_driver;
extern struct panel_driver jd9366d_truly_driver;
extern struct panel_driver rm67191_edo_driver;
extern struct panel_driver r61350_truly_driver;
extern struct panel_driver r61350_truly_v2_driver;
extern struct panel_driver td4310_truly_driver;
extern struct panel_driver g40396_truly_driver;
extern struct panel_driver nt36525b_txd_driver;
extern struct panel_driver nt36525b_tm_driver;
extern struct panel_driver nt36525b_hx_driver;
extern struct panel_driver jd9365t_cpt_driver;
extern struct panel_driver nl9911c_truly_driver;
extern struct panel_driver gc7202h_genrpro_driver;
extern struct panel_driver jd9365t_hlt_driver;
extern struct panel_driver nl9911c_truly_6mask_driver;
extern struct panel_driver hx83102e_inx_driver;
extern struct panel_driver hx83102e_xy_hsd_driver;
extern struct panel_driver icnl9911c_truly_driver;
extern struct panel_driver nt36672e_truly_nodsc_driver;
extern struct panel_driver nt36672e_truly_bp_driver;
extern struct panel_driver nt36672e_truly_driver;
extern struct panel_driver nt36672c_truly_driver;
extern struct panel_driver nt36672e_truly_umb9230s_driver;
extern struct panel_driver td4160_boe_driver;
extern struct panel_driver td4320_truly_driver;
//AUTO_GEN_TAG_LCD_EXTERN
extern struct panel_driver g3655fp103ff_visionox_driver;
extern struct panel_driver g2667fp108ff_visionox_driver;
extern struct panel_driver nt36528_boe_driver;
extern struct panel_driver nt36528_kti_driver;

//add by jinqiang for udc 
#ifdef CONFIG_UDC

extern struct panel_driver lcd_panel_udc_lcd;
static struct panel_cfg lcd_panel[] = {
	[0]={
	.lcd_id = UDC_LCM_ID,
	.drv = &lcd_panel_udc_lcd,
	}

};


udc_lcd* g_sc9850_udc_lcd;
#else
static struct panel_cfg supported_panel[] = {
#if 1//AUTO_GEN_TAG_LCD_CFG
#ifdef CONFIG_LCD_NT36528_KTI_MIPI_HDP
	{
		.lcd_id = 0x36528,
		.drv = &nt36528_kti_driver,
	},
#endif
#ifdef CONFIG_AMOLED_G2667FP108FF_VISIONOX_MIPI_FHD
	{
		.lcd_id = 0x3655,
		.drv = &g2667fp108ff_visionox_driver,
	},
#endif
#ifdef CONFIG_AMOLED_G3655FP103FF_VISIONOX_MIPI_FHD
	{
		.lcd_id = 0x3655,
		.drv = &g3655fp103ff_visionox_driver,
	},
#endif
#ifdef CONFIG_LCD_FT8006P_EASYQUICK_MIPI_HDP
	{
		.lcd_id = 0xf0,
		.drv = &ft8006p_easyquick_driver,
	},
#endif
#ifdef CONFIG_LCD_FT8006P_HLT_MIPI_HDP
	{
		.lcd_id = 0xf0,
		.drv = &ft8006p_hlt_driver,
	},
#endif
#ifdef CONFIG_LCD_FT8006P_HUAXIAN_MIPI_HDP
	{
		.lcd_id = 0xe0,
		.drv = &ft8006p_huaxian_driver,
	},
#endif
#ifdef CONFIG_LCD_ICNL9911_HLT_MIPI_HDP
        {
                .lcd_id = 0x9911,
                .drv = &icnl9911_hlt_driver,
        },
#endif
#ifdef CONFIG_LCD_ICNL9911_TXD_MIPI_HDP
        {
                .lcd_id = 0x9911,
                .drv = &icnl9911_txd_driver,
        },
#endif
#ifdef CONFIG_LCD_ILI9881C_TRULY_MIPI_HD
	{
		.lcd_id = 0x9881,
		.drv = &ili9881c_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_ILI7807S_TIANMA_MIPI_HD
	{
		.lcd_id = 0x20,
		.drv = &ili7807s_tianma_driver,
	},
#endif
#ifdef CONFIG_LCD_ILI9881C_3LANE_MIPI_HD
	{
		.lcd_id = 0x9881,
		.drv = &ili9881c_3lane_driver,
	},
#endif
#ifdef CONFIG_LCD_ILI9881C_SKYWORTH_HD
	{
		.lcd_id = 0x98814,
		.drv = &ili9881c_skyworth_driver,
	},
#endif
#ifdef CONFIG_LCD_SSD2092_TRULY_MIPI_FHD
	{
		.lcd_id = 0x2092,
		.drv = &ssd2092_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_ST7701_BOE_MIPI_WVGA
	{
		.lcd_id = 0x7701,
		.drv = &st7701_boe_driver,
	},
#endif
#ifdef CONFIG_LCD_ST7701_COE_MIPI_WVGA
	{
		.lcd_id = 0x77011,
		.drv = &st7701_coe_driver,
	},
#endif
#ifdef CONFIG_LCD_NT35532_TRULY_MIPI_FHD
	{
		.lcd_id = 0x32,
		.drv = &nt35532_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_NT35596_BOE_MIPI_FHD
	{
		.lcd_id = 0x96,
		.drv = &nt35596_boe_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36528_BOE_MIPI_HDP
	{
		.lcd_id = 0x36528,
		.drv = &nt36528_boe_driver,
	},
#endif
#ifdef CONFIG_LCD_NT35597_BOE_MIPI_HD
	{
		.lcd_id = 0x97,
		.drv = &nt35597_boe_driver,
	},
#endif
#ifdef CONFIG_LCD_NT35597_FPGA_MIPI_2K
	{
		.lcd_id = 0x97,
		.drv = &nt35597_fpga_driver,
	},
#endif
#ifdef CONFIG_LCD_NT35695_TRULY_MIPI_FHD
	{
		.lcd_id = 0x35695,
		.drv = &nt35695_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_JD9161_XXX_MIPI_WVGA
	{
		.lcd_id = 0x91612,
		.drv = &jd9161_xxx_driver,
	},
#endif
#ifdef CONFIG_LCD_JD9365Z_KING_MIPI_HDP
	{
		.lcd_id = 0x9365,
		.drv = &jd9365z_king_driver,
	},
#endif
#ifdef CONFIG_LCD_JD9366D_TRULY_MIPI_HDP
	{
		.lcd_id = 0x9366,
		.drv = &jd9366d_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_RM67191_EDO_MIPI_FHD
	{
		.lcd_id = 0x67191,
		.drv = &rm67191_edo_driver,
	},
#endif
#ifdef CONFIG_LCD_R61350_TRULY_MIPI_HD
	{
		.lcd_id = 0x61350,
		.drv = &r61350_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_R61350_TRULY_MIPI_HD_V2
	{
		.lcd_id = 0x61350,
		.drv = &r61350_truly_v2_driver,
	},
#endif
#ifdef CONFIG_LCD_TD4310_TRULY_MIPI_FHD
	{
		.lcd_id = 0x4310,
		.drv = &td4310_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_G40396_TRULY_MIPI_FHD
	{
		.lcd_id = 0x40396,
		.drv = &g40396_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_ICNL9911C_TRULY_MIPI_HDP
	{
		.lcd_id = 0x99,
		.drv = &icnl9911c_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36525B_TXD_MIPI_HDP
	{
		.lcd_id = 0x55,
		.drv = &nt36525b_txd_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36525B_TM_MIPI_HDP
	{
		.lcd_id = 0x3b,
		.drv = &nt36525b_tm_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36525B_HX_MIPI_HDP
	{
		.lcd_id = 0x3b,
		.drv = &nt36525b_hx_driver,
	},
#endif
#ifdef CONFIG_LCD_JD9365T_CPT_MIPI_HDP
	{
		.lcd_id = 0x23,
		.drv = &jd9365t_cpt_driver,
	},
#endif
#ifdef CONFIG_LCD_NL9911C_TRULY_MIPI_HDP
	{
		.lcd_id = 0x42,
		.drv = &nl9911c_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_GC7202H_GENRPRO_MIPI_HDP
	{
		.lcd_id = 0x70,
		.drv = &gc7202h_genrpro_driver,
	},
#endif
#ifdef CONFIG_LCD_JD9365T_HLT_MIPI_HDP
	{
		.lcd_id = 0x28,
		.drv = &jd9365t_hlt_driver,
	},
#endif
#ifdef CONFIG_LCD_NL9911C_TRULY_6MASK_MIPI_HDP
	{
		.lcd_id = 0x44,
		.drv = &nl9911c_truly_6mask_driver,
	},
#endif
#ifdef CONFIG_LCD_HX83102E_INX_MIPI_HDP
	{
		.lcd_id = 0x83,
		.drv = &hx83102e_inx_driver,
	},
#endif
#ifdef CONFIG_LCD_HX83102E_XY_HSD_MIPI_HDP
	{
		.lcd_id = 0x20,
		.drv = &hx83102e_xy_hsd_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36672E_TRULY_MIPI_FHD_NODSC
	{
		.lcd_id = 0x36672,
		.drv = &nt36672e_truly_nodsc_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36672E_TRULY_MIPI_FHD_BP
	{
		.lcd_id = 0x36672,
		.drv = &nt36672e_truly_bp_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36672E_TRULY_MIPI_FHD
        {
                .lcd_id = 0x36672,
                .drv = &nt36672e_truly_driver,
        },
#endif
#ifdef CONFIG_LCD_NT36672C_TRULY_MIPI_FHD
	{
		.lcd_id = 0x36672,
		.drv = &nt36672c_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_TD4160_BOE_MIPI_HDP
	{
		.lcd_id = 0x4160,
		.drv = &td4160_boe_driver,
	},
#endif
#ifdef CONFIG_LCD_TD4320_TRULY_MIPI_FHD
	{
		.lcd_id = 0x4320,
		.drv = &td4320_truly_driver,
	},
#endif
#ifdef CONFIG_LCD_NT36672E_TRULY_MIPI_FHD_UMB9230S
        {
                .lcd_id = 0x4180,
                .drv = &nt36672e_truly_umb9230s_driver,
        },
#endif
#endif

/* warning: the dummy lcd must be the last item in this array */
	{
		.lcd_id = 0xFFFF,
		.drv = &dummy_mipi_driver,
	},
};
#endif
