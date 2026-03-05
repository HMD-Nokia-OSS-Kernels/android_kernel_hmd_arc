/*
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2023-2023 Unisoc (Shanghai) Technologies Co., Ltd
 *
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License. You may obtain a copy of the License at
 *
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 *
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <i2c.h>

#include "../sprd_dsi.h"

#include "lk_gpio_fun.h"
#include "umb9230s.h"

/* iic2apb register */
#define REG_IIC2APB_BASE            0x0
#define REG_IIC2APB_INT_EN          (REG_IIC2APB_BASE + 0x08)
#define REG_IIC2APB_INT_RAW         (REG_IIC2APB_BASE + 0x0C)
#define REG_IIC2APB_INT_CLR         (REG_IIC2APB_BASE + 0x10)

/* clk register */
#define REG_CLK_CORE_BASE           0x4400
#define REG_CLOCK_SWITCH            (REG_CLK_CORE_BASE + 0x28)
#define REG_DSC_DIV                 (REG_CLK_CORE_BASE + 0x3C)

#define BIT_CLK_XBUF                0
#define BIT_CLK_RPLL                BIT(0)

/* top register */
#define REG_TOP_BASE                0x6000
#define REG_POWER_SWITCH            REG_TOP_BASE
#define REG_INTR_MASKED_STATUS      (REG_TOP_BASE + 0x50)
#define REG_INTR_RAW_STATUS         (REG_TOP_BASE + 0x54)
#define REG_INTR_MASK               (REG_TOP_BASE + 0x58)
#define REG_TE_DELAY_EN             (REG_TOP_BASE + 0x60)
#define REG_TE_DELAY                (REG_TOP_BASE + 0x64)
#define REG_CLK_CTRL                (REG_TOP_BASE + 0x70)

#define BIT_DEEP_SLEEP              BIT(0)
#define BIT_DPI_PAD_IN_SEL          BIT(7)

/* dsc registers */
#define REG_DSC_BASE                    0x9000
#define REG_DSC_CTRL                    (REG_DSC_BASE + 0x00)
#define REG_DSC_PIC_SIZE                (REG_DSC_BASE + 0x04)
#define REG_DSC_GRP_SIZE                (REG_DSC_BASE + 0x08)
#define REG_DSC_SLICE_SIZE              (REG_DSC_BASE + 0x0c)
#define REG_DSC_H_TIMING                (REG_DSC_BASE + 0x10)
#define REG_DSC_V_TIMING                (REG_DSC_BASE + 0x14)
#define REG_DSC_CFG0                    (REG_DSC_BASE + 0x18)
#define REG_DSC_CFG1                    (REG_DSC_BASE + 0x1c)
#define REG_DSC_CFG2                    (REG_DSC_BASE + 0x20)
#define REG_DSC_CFG3                    (REG_DSC_BASE + 0x24)
#define REG_DSC_CFG4                    (REG_DSC_BASE + 0x28)
#define REG_DSC_CFG5                    (REG_DSC_BASE + 0x2c)
#define REG_DSC_CFG6                    (REG_DSC_BASE + 0x30)
#define REG_DSC_CFG7                    (REG_DSC_BASE + 0x34)
#define REG_DSC_CFG8                    (REG_DSC_BASE + 0x38)
#define REG_DSC_CFG9                    (REG_DSC_BASE + 0x3c)
#define REG_DSC_CFG10                   (REG_DSC_BASE + 0x40)
#define REG_DSC_CFG11                   (REG_DSC_BASE + 0x44)
#define REG_DSC_CFG12                   (REG_DSC_BASE + 0x48)
#define REG_DSC_CFG13                   (REG_DSC_BASE + 0x4c)
#define REG_DSC_CFG14                   (REG_DSC_BASE + 0x50)
#define REG_DSC_CFG15                   (REG_DSC_BASE + 0x54)
#define REG_DSC_CFG16                   (REG_DSC_BASE + 0x58)
#define REG_DSC_STS0                    (REG_DSC_BASE + 0x5c)
#define REG_DSC_STS1                    (REG_DSC_BASE + 0x60)
#define REG_DSC_VERSION                 (REG_DSC_BASE + 0x64)

/* video2cmd registers */
#define REG_VIDEO2CMD_BASE          0xA200
#define REG_VIDEO2CMD_MODE          REG_VIDEO2CMD_BASE
#define REG_VIDEO2CMD_SIZE_X        (REG_VIDEO2CMD_BASE + 0x4)
#define REG_VIDEO2CMD_SIZE_Y        (REG_VIDEO2CMD_BASE + 0x8)
#define REG_VIDEO2CMD_IDLE          (REG_VIDEO2CMD_BASE + 0xC)

/* color pattern registers */
#define REG_COLOR_PATTERN_BASE                  0xA400
#define REG_COLOR_PATTERN_EB                    REG_COLOR_PATTERN_BASE
#define REG_COLOR_PATTERN_VIDEO_CMD             (REG_COLOR_PATTERN_BASE + 0x04)
#define REG_COLOR_PATTERN_MODE                  (REG_COLOR_PATTERN_BASE + 0x08)
#define REG_COLOR_PATTERN_VS                    (REG_COLOR_PATTERN_BASE + 0x0C)
#define REG_COLOR_PATTERN_VBP                   (REG_COLOR_PATTERN_BASE + 0x10)
#define REG_COLOR_PATTERN_VFP                   (REG_COLOR_PATTERN_BASE + 0x14)
#define REG_COLOR_PATTERN_HS                    (REG_COLOR_PATTERN_BASE + 0x18)
#define REG_COLOR_PATTERN_HBP                   (REG_COLOR_PATTERN_BASE + 0x1C)
#define REG_COLOR_PATTERN_HFP                   (REG_COLOR_PATTERN_BASE + 0x20)
#define REG_COLOR_PATTERN_SIZE_X                (REG_COLOR_PATTERN_BASE + 0x24)
#define REG_COLOR_PATTERN_SIZE_Y                (REG_COLOR_PATTERN_BASE + 0x28)
#define REG_COLOR_PATTERN_COCLOR_DATA0          (REG_COLOR_PATTERN_BASE + 0x2C)
#define REG_COLOR_PATTERN_COCLOR_DATA1          (REG_COLOR_PATTERN_BASE + 0x30)
#define REG_COLOR_PATTERN_COCLOR_DATA2          (REG_COLOR_PATTERN_BASE + 0x34)
#define REG_COLOR_PATTERN_COCLOR_DATA3          (REG_COLOR_PATTERN_BASE + 0x38)
#define REG_COLOR_PATTERN_COCLOR_DATA4          (REG_COLOR_PATTERN_BASE + 0x3C)
#define REG_COLOR_PATTERN_IDLE                  (REG_COLOR_PATTERN_BASE + 0x40)
#define REG_COLOR_PATTERN_IS_COLOR_PATTERN_T    (REG_COLOR_PATTERN_BASE + 0x44)

/*  deep sleep enable registers */
#define REG_PG0_PIN_RF_BASE                      0x5000
#define REG_PG1_PIN_RF_BASE                      0x5400
#define REG_PMU_IO_DEEP_SLEEP_FORCE_EN           REG_TOP_BASE    /* top register 0x6000*/
#define REG_CLK_AUX                              (REG_PG1_PIN_RF_BASE + 0xC8)
#define REG_DSI_TE_O                             (REG_PG0_PIN_RF_BASE + 0xA0)
#define REG_MTCK_ARM                             (REG_PG0_PIN_RF_BASE + 0x9C)
#define REG_GPIO0                                (REG_PG1_PIN_RF_BASE + 0xAC)
#define REG_GPIO3                                (REG_PG1_PIN_RF_BASE + 0xB0)
#define REG_GPIO1                                (REG_PG1_PIN_RF_BASE + 0xB4)
#define REG_GPIO2                                (REG_PG1_PIN_RF_BASE + 0xC0)
#define REG_DSI_TE_I                             (REG_PG1_PIN_RF_BASE + 0xC4)

struct umb9230s_device umb9230s_dev;
int32_t hr_type;

extern int sprd_get_tpic_version(void);

static int umb9230s_power_enable(struct umb9230s_device *umb9230s, int enable)
{
    uint32_t buf[2];
    int ret;

    if (enable) {
        ret = umb9230s_gpio_init();
        if (ret) {
            pr_err("umb9230s gpio init failed\n");
            return ret;
        }

        mdelay(1);

        /* iic clock switch: 0x4428 bit0 set 1 */
        buf[0] = REG_CLOCK_SWITCH;
        buf[1] = 0x1;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

        /* enable power switch: 0x6000 bit0 set 0 */
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_POWER_SWITCH, &buf[1], 1);
        buf[0] = REG_POWER_SWITCH;
        buf[1] &= (~(1 << 0));
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

        mdelay(5);

        pr_info("umb9230s power up completed!\n");
    } else {
        /* close power switch: 0x6000 bit0 set 1 */
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_POWER_SWITCH, &buf[1], 1);
        buf[0] = REG_POWER_SWITCH;
        buf[1] |= 0x1;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

        mdelay(5);

        /* EXT_RST_B_AP pull down, RFCTL16 GPIO8 */
        umb9230s_gpio_reset(0);

        /* xbuf_pd pull high，U2TXD GPIO171 */
        umb9230s_set_gpio_power(1);

        pr_info("umb9230s power down completed!\n");
    }

    return 0;
}

void umb9230s_dslp_mode_enable(struct umb9230s_device *umb9230s)
{
	uint32_t buf[2] = {0};

	/* IO_DEEP_SLEEP_MODE: 0x6000 bit10 set 1 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                                        REG_PMU_IO_DEEP_SLEEP_FORCE_EN, &buf[1], 1);
	buf[0] = REG_PMU_IO_DEEP_SLEEP_FORCE_EN;
	buf[1] |= (1 << 10);
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* CLK_AUX: 0x54c8 bit2 set 1 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_CLK_AUX, &buf[1], 1);
	buf[0] = REG_CLK_AUX;
	buf[1] |= (1 << 2);
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* DSI_TE_O: 0x50a0 bit13~bit18 set 0 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_DSI_TE_O, &buf[1], 1);
	buf[0] = REG_DSI_TE_O;
	buf[1] &= (~(0x3f << 13));
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* MTCK_ARM: 0x509c bit13~bit18 set 0 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_MTCK_ARM, &buf[1], 1);
	buf[0] = REG_MTCK_ARM;
	buf[1] &= (~(0x3f << 13));
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* GPIO0: 0x54ac bit13~bit18 set 0 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_GPIO0, &buf[1], 1);
	buf[0] = REG_GPIO0;
	buf[1] &= (~(0x3f << 13));
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* GPIO3: 0x54b0 bit13~bit18 set 0 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_GPIO3, &buf[1], 1);
	buf[0] = REG_GPIO3;
	buf[1] &= (~(0x3f << 13));
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* GPIO1: 0x54b4 bit13~bit18 set 0 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_GPIO1, &buf[1], 1);
	buf[0] = REG_GPIO1;
	buf[1] &= (~(0x3f << 13));
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* GPIO2: 0x54c0 bit13~bit18 set 0 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_GPIO2, &buf[1], 1);
	buf[0] = REG_GPIO2;
	buf[1] &= (~(0x3f << 13));
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

	/* DSI_TE_I: 0x54c4 bit13~bit18 set 0 */
	iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr, REG_DSI_TE_I, &buf[1], 1);
	buf[0] = REG_DSI_TE_I;
	buf[1] &= (~(0x3f << 13));
	iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
}

static void umb9230s_pattern_mode(struct umb9230s_device *umb9230s)
{
    struct panel_info *panel = umb9230s->panel;
    struct rgb_timing *timing = &panel->rgb_timing;
    uint32_t buf[2] = {0};

    pr_info("color pattern start\n");

    /* 1: gen dpi timing; 0: stop gen timing */
    buf[0] = REG_COLOR_PATTERN_EB;
    buf[1] = 0x0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* 0:video2cmd putput; 1:color pattern output */
    buf[0] = REG_COLOR_PATTERN_IS_COLOR_PATTERN_T;
    buf[1] = 0x1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* 0: video mode; 1: cmd mode */
    buf[0] = REG_COLOR_PATTERN_VIDEO_CMD;
    buf[1] = 0x0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_VS;
    buf[1] = timing->vsync & 0xff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_VBP;
    buf[1] = timing->vbp & 0xfff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_VFP;
    buf[1] = timing->vfp & 0xfff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_HS;
    buf[1] = timing->hsync & 0xff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_HBP;
    buf[1] = timing->hbp & 0xfff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_HFP;
    buf[1] = timing->hfp & 0xfff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_SIZE_X;
    buf[1] = panel->width & 0xfff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_SIZE_Y;
    buf[1] = panel->height & 0xfff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* 0: horizontal stripe; 1: vertical stripe; other: checkboard */
    buf[0] = REG_COLOR_PATTERN_MODE;
    buf[1] = 0x1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_EB;
    buf[1] = 0x1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* set pattern color */
    buf[0] = REG_COLOR_PATTERN_COCLOR_DATA0;
    buf[1] = 0x0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_COCLOR_DATA1;
    buf[1] = 0xffffff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_COCLOR_DATA2;
    buf[1] = 0xff0000;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_COCLOR_DATA3;
    buf[1] = 0x00ff00;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_COLOR_PATTERN_COCLOR_DATA4;
    buf[1] = 0x0000ff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
}

static void umb9230s_video2cmd_mode(struct umb9230s_device *umb9230s, bool enable)
{
    struct panel_info *panel = umb9230s->panel;
    u32 buf[4];

    pr_info("enable:%d\n", enable);

    buf[0] = REG_VIDEO2CMD_MODE;
    buf[1] = enable;

    if ((panel->width % 3) && panel->umb9230s_dsc_en)
        buf[2] = panel->width / 3 + 1;
    else
        buf[2] = panel->width / 3;

    buf[3] = panel->height;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 4);
}

static void umb9230s_te_delay(struct umb9230s_device *umb9230s, uint32_t time)
{
    u32 buf[3] = {};

    pr_info("time:%d\n", time);

    buf[0] = REG_TE_DELAY_EN;
    if (time > 0) {
        buf[1] = 1;
        buf[2] = time;
    } else
        buf[1] = 0;

    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 3);
}

static void umb9230s_dsc_config(struct umb9230s_device *umb9230s)
{
    struct dsc_reg *reg = &umb9230s->dsc_cfg.reg;
    struct panel_info *panel = umb9230s->panel;
    struct rgb_timing *rgb = &panel->rgb_timing;
    u32 buf[24];

    calc_dsc_r4p0_params(&umb9230s->dsc_init, &umb9230s->dsc_cfg, panel);

    /*
     * if 1 slice per line, clk_dsc = clk_dpi;
     * if 2 slice per line, clk_dsc = clk_dpi / 2;
     * if 4 slice per line, clk_dsc = clk_dpi / 4;
     */
    if (panel->umb9230s_slice_width == panel->width / 2)
        buf[1] = 1;
    else
        buf[1] = 0;
    buf[0] = REG_DSC_DIV;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    reg->dsc_pic_size = (panel->height << 16) | (panel->width << 0);
    reg->dsc_h_timing = (rgb->hsync << 0) | (rgb->hbp << 8) | (rgb->hfp << 20);
    reg->dsc_v_timing = (rgb->vsync << 0) | (rgb->vbp << 8) | (rgb->vfp << 20);

    if (panel->umb9230s_work_mode == SPRD_MIPI_MODE_CMD)
        reg->dsc_ctrl = 0x2000050b;
    else
        reg->dsc_ctrl = 0x2000040b;

    buf[0] = REG_DSC_CTRL;
    memcpy(&buf[1], &reg->dsc_ctrl, 23 * 4);
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 24);

    pr_info("umb9230s dsc config OK\n");
}

static int umb9230s_rx_module_enable(struct umb9230s_device *umb9230s)
{
    struct panel_info *panel;

    if (hr_type != HR_TYPE_UMB9230s)
        return 0;

    panel = umb9230s->panel;

    pr_info("dsc_en:%d work_mode:%d\n", panel->umb9230s_dsc_en,
                                panel->umb9230s_work_mode);

    umb9230s_phy_rx_configure(umb9230s);

    umb9230s_dsi_rx_init(umb9230s);

    if (panel->umb9230s_dsc_en)
        umb9230s_dsc_config(umb9230s);

    if (panel->umb9230s_work_mode == SPRD_MIPI_MODE_CMD)
        umb9230s_video2cmd_mode(umb9230s, true);

    return 0;
}

static int umb9230s_tx_module_enable(struct umb9230s_device *umb9230s)
{
    int ret;

    pr_info("i2c_bus:%d i2c_addr:0x%x\n", umb9230s->i2c_bus, umb9230s->i2c_addr);

    ret = umb9230s_power_enable(umb9230s, 1);
    if (ret) {
        pr_err("umb9230s power enable failed\n");
        return ret;
    }

    umb9230s_dslp_mode_enable(umb9230s);  //UMB9230s DEEP SLEEP MODE ENALBE

    umb9230s_dsi_tx_enable(umb9230s);

    umb9230s_phy_tx_configure(umb9230s);

    return 0;
}

static int umb9230s_context_init(struct umb9230s_device *umb9230s)
{
    struct panel_info *panel = umb9230s->panel;
    struct dsi_tx_context *dsi_ctx = &umb9230s->dsi_ctx;
    struct phy_tx_context *phy_ctx = &umb9230s->phy_ctx;

    dsi_ctx->max_rd_time = 0x8000;
    dsi_ctx->int0_mask = 0xffffffff;
    dsi_ctx->int1_mask = 0xffffffff;

    dsi_ctx->freq = panel->umb9230s_phy_freq;
    dsi_ctx->lanes = panel->lane_num;

    phy_ctx->freq = panel->umb9230s_phy_freq;
    phy_ctx->lanes = panel->lane_num;

    return 0;
}

int umb9230s_wait_idle_state(struct umb9230s_device *umb9230s)
{
    u8 lane_mask;

    if (hr_type != HR_TYPE_UMB9230s)
        return 0;

    lane_mask = (1 << umb9230s->phy_ctx.lanes) - 1;

    umb9230s_phy_tx_wait_datalane_stop_state(umb9230s, lane_mask);

    umb9230s_phy_rx_wait_datalane_stop_state(umb9230s, lane_mask);

    return 0;
}

static void umb9230s_enable_irq(struct umb9230s_device *umb9230s)
{
    u32 buf[2];

    buf[0] = REG_IIC2APB_INT_EN;
    buf[1] = 0xff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_IIC2APB_INT_CLR;
    buf[1] = 0xff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_INTR_MASK;
    buf[1] = 0xffffffff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
}

static void umb9230s_disable_irq(struct umb9230s_device *umb9230s)
{
    u32 buf[2];

    buf[0] = REG_IIC2APB_INT_EN;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_IIC2APB_INT_CLR;
    buf[1] = 0xff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_INTR_MASK;
    buf[1] = 0xffffffff;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
}

int umb9230s_tx_module_suspend(struct umb9230s_device *umb9230s)
{
    if (hr_type != HR_TYPE_UMB9230s)
        return 0;

    pr_info("umb9230s suspend\n");

    umb9230s_disable_irq(umb9230s);

    /* dphy_data_ulps_en */
    umb9230s_dphy_tx_data_ulps_en(umb9230s, true);

    /* dphy_clk_ulps_en */
    umb9230s_dphy_tx_clk_ulps_en(umb9230s, true);

    /* dphy_close */
    umb9230s_phy_tx_close(umb9230s);

    /* dsi_uninit */
    umb9230s_dsi_tx_uninit(umb9230s);

    umb9230s_power_enable(umb9230s, 0);

    return 0;
}

int umb9230s_check_lcd_compatibility(int index, int size)
{
    struct panel_info *panel = panel_info_attach();

    if (index == (size - 1)) {
        /* set umb9230 to video mode for dummy lcd */
        if (!strncmp(panel->lcd_name, "lcd_dummy_mipi_hd", strlen("lcd_dummy_mipi_hd"))) {
            panel->umb9230s_work_mode = SPRD_MIPI_MODE_VIDEO;
            panel->umb9230s_phy_freq = 500000;
        }
        return 0;
    }

    hr_type = sprd_get_tpic_version();

    if ((hr_type != HR_TYPE_UMB9230s && panel->umb9230s_en) ||
        (hr_type == HR_TYPE_UMB9230s && !panel->umb9230s_en))
        return 1;

    return 0;
}

int umb9230s_module_probe(void)
{
    struct umb9230s_device *umb9230s;

    if (hr_type != HR_TYPE_UMB9230s) {
        pr_info("no umb9230s(%d)\n", hr_type);
        return 0;
    }

    umb9230s = &umb9230s_dev;

#if defined (CONFIG_UMB9230S_I2C_BUS) && defined (CONFIG_UMB9230S_I2C_ADDR)
    umb9230s->i2c_bus = CONFIG_UMB9230S_I2C_BUS;
    umb9230s->i2c_addr = CONFIG_UMB9230S_I2C_ADDR;

    umb9230s->phy_ctx.i2c_bus = CONFIG_UMB9230S_I2C_BUS;
    umb9230s->phy_ctx.i2c_addr = CONFIG_UMB9230S_I2C_ADDR;
#else
    pr_err("no i2c addr! skip umb9230s probe\n");
    hr_type = HR_TYPE_Non_HR;
    return 0;
#endif

    pr_info("%s()\n", __func__);

    umb9230s->panel = panel_info_attach();
    umb9230s->pll = umb9230s_dphy_tx_pll_ops_attach();

    umb9230s_context_init(umb9230s);
    umb9230s_tx_module_enable(umb9230s);
    umb9230s_rx_module_enable(umb9230s);
    umb9230s_enable_irq(umb9230s);

    return 0;
}
