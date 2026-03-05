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

#include "umb9230s.h"

/* dsi rx registers */
#define REG_DSI_RX_BASE                     0x8000
#define REG_DSI_RX_PHY_PD_N                 (REG_DSI_RX_BASE + 0x04)
#define REG_DSI_RX_RST_DPHY_N               (REG_DSI_RX_BASE + 0x08)
#define REG_DSI_RX_PHY_STATE                (REG_DSI_RX_BASE + 0x0C)
#define REG_DSI_RX_PHY_RX_TRIGGERS          (REG_DSI_RX_BASE + 0x34)

/* dsi tx registers */
#define REG_DSI_TX_BASE                         0x9400
#define REG_DSI_TX_PHY_CLK_LANE_LP_CTRL         (REG_DSI_TX_BASE + 0x74)
#define REG_DSI_TX_PHY_INTERFACE_CTRL           (REG_DSI_TX_BASE + 0x78)
#define REG_DSI_TX_PHY_TX_TRIGGERS              (REG_DSI_TX_BASE + 0x7C)
#define REG_DSI_TX_PHY_STATUS                   (REG_DSI_TX_BASE + 0x9C)
#define REG_DSI_TX_PHY_MIN_STOP_TIME            (REG_DSI_TX_BASE + 0xA0)
#define REG_DSI_TX_PHY_LANE_NUM_CONFIG          (REG_DSI_TX_BASE + 0xA4)
#define REG_DSI_TX_PHY_CLKLANE_TIME_CONFIG      (REG_DSI_TX_BASE + 0xA8)
#define REG_DSI_TX_PHY_DATALANE_TIME_CONFIG     (REG_DSI_TX_BASE + 0xAC)
#define REG_DSI_TX_INT_PLL_STS                  (REG_DSI_TX_BASE + 0x200)
#define REG_DSI_TX_INT_PLL_MSK                  (REG_DSI_TX_BASE + 0x204)
#define REG_DSI_TX_INT_PLL_CLR                  (REG_DSI_TX_BASE + 0x208)

/* dphy tx/rx registers */
#define REG_PHY_TEST_CTRL_BASE      0xB000
#define REG_PHY_TX_BASE             (REG_PHY_TEST_CTRL_BASE + 0x400)
#define REG_PHY_RX_BASE             REG_PHY_TEST_CTRL_BASE
#define REG_PHY_TX_TEST_CTRL        (REG_PHY_TEST_CTRL_BASE + 0xC00)
#define REG_PHY_RX_TEST_CTRL        (REG_PHY_TEST_CTRL_BASE + 0x800)

#define REG_PHY_TX_A2D_LANE0_MAP    (REG_PHY_TX_BASE + (0x4D * 4))
#define REG_PHY_TX_A2D_LANE2_MAP    (REG_PHY_TX_BASE + (0x6D * 4))
#define REG_PHY_TX_D2A_LANE_SWAP    (REG_PHY_TX_BASE + (0xF1 * 4))

#define REG_PHY_RX_A2D_LANE0_MAP    (REG_PHY_RX_BASE + (0x4D * 4))
#define REG_PHY_RX_A2D_LANE2_MAP    (REG_PHY_RX_BASE + (0x6D * 4))
#define REG_PHY_RX_D2A_LANE_SWAP    (REG_PHY_RX_BASE + (0xF1 * 4))

int umb9230s_phy_rx_configure(struct umb9230s_device *umb9230s)
{
    u32 buf[2];

    /* rstz */
    buf[0] = REG_DSI_RX_RST_DPHY_N;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* shutdownz */
    buf[0] = REG_DSI_RX_PHY_PD_N;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* test_clr */
    buf[0] = REG_PHY_RX_TEST_CTRL;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* test_clr, clear by hardware automatically */
    buf[0] = REG_PHY_RX_TEST_CTRL;
    buf[1] = 1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* shutdownz */
    buf[0] = REG_DSI_RX_PHY_PD_N;
    buf[1] = 1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* rstz */
    buf[0] = REG_DSI_RX_RST_DPHY_N;
    buf[1] = 1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    udelay(1);

    /* swap datalane0 and datalane2 */
    buf[0] = REG_PHY_RX_D2A_LANE_SWAP;
    buf[1] = 0xc6;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_PHY_RX_A2D_LANE0_MAP;
    buf[1] = 0x40;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_PHY_RX_A2D_LANE2_MAP;
    buf[1] = 0x00;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    return 0;
}

int umb9230s_phy_rx_wait_datalane_stop_state(struct umb9230s_device *umb9230s, u8 mask)
{
    unsigned i = 0;
    union _dsi_rx_0x0C phy_state;
    u8 state = 0;

    for (i = 0; i < 500; i++) {
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_RX_PHY_STATE, &phy_state.val, 1);
        if (phy_state.bits.phy_stopstatedata0)
            state |= BIT(0);
        if (phy_state.bits.phy_stopstatedata1)
            state |= BIT(1);
        if (phy_state.bits.phy_stopstatedata2)
            state |= BIT(2);
        if (phy_state.bits.phy_stopstatedata3)
            state |= BIT(3);

        if (state == mask)
            return 0;

        udelay(10);
    }

    pr_err("umb9230s rx phy datalane stop state wait time out\n");
    return -1;
}


int umb9230s_phy_rx_wait_clklane_stop_state(struct umb9230s_device *umb9230s)
{
    u32 i = 0;
    union _dsi_rx_0x0C phy_state;

    for (i = 0; i < 5000; i++) {
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_RX_PHY_STATE, &phy_state.val, 1);
        if (phy_state.bits.phy_stopstateclk)
            return 0;

        udelay(10);
    }

    pr_err("umb9230s rx phy clklane stop state wait time out\n");
    return -1;
}

int umb9230s_phy_rx_close(struct umb9230s_device *umb9230s)
{
    u32 buf[2];

    pr_info("%s()\n", __func__);

    if (!umb9230s)
        return -1;

    /* rstz */
    buf[0] = REG_DSI_RX_RST_DPHY_N;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* shutdownz */
    buf[0] = REG_DSI_RX_PHY_PD_N;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* rstz */
    buf[0] = REG_DSI_RX_RST_DPHY_N;
    buf[1] = 1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    return 0;
}

static int umb9230s_phy_tx_wait_pll_locked(struct umb9230s_device *umb9230s)
{
    unsigned i = 0;
    union _0x9C phy_status;

    for (i = 0; i < 50000; i++) {
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_TX_PHY_STATUS, &phy_status.val, 1);
        if (phy_status.bits.phy_lock)
            return 0;
        udelay(3);
    }

    pr_err("error: umb9230s dphy pll can not be locked\n");
    return -1;
}

static int umb9230s_phy_tx_is_pll_locked(struct umb9230s_device *umb9230s)
{
    union _0x9C phy_status;

     iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                    REG_DSI_TX_PHY_STATUS, &phy_status.val, 1);
    return phy_status.bits.phy_lock;
}

int umb9230s_phy_tx_wait_datalane_stop_state(struct umb9230s_device *umb9230s, u8 mask)
{
    unsigned i = 0;
    union _0x9C phy_status;
    u8 state = 0;

    for (i = 0; i < 500; i++) {
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_TX_PHY_STATUS, &phy_status.val, 1);
        if (phy_status.bits.phy_stopstate0lane)
            state |= BIT(0);
        if (phy_status.bits.phy_stopstate1lane)
            state |= BIT(1);
        if (phy_status.bits.phy_stopstate2lane)
            state |= BIT(2);
        if (phy_status.bits.phy_stopstate3lane)
            state |= BIT(3);

        if (state == mask)
            return 0;

        udelay(10);
    }

    pr_err("umb9230s datalane ulps exit wait time out\n");
    return -1;
}

static int umb9230s_phy_tx_wait_clklane_stop_state(struct umb9230s_device *umb9230s)
{
    unsigned i = 0;
    union _0x9C phy_status;

    for (i = 0; i < 5000; i++) {
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_TX_PHY_STATUS, &phy_status.val, 1);
        if (phy_status.bits.phy_stopstateclklane)
            return 0;
        udelay(10);
    }

    pr_err("umb9230s clklane ulps exit wait time out\n");
    return -1;
}

int umb9230s_dphy_tx_data_ulps_en(struct umb9230s_device *umb9230s, int enable)
{
    u8 mask = 0;
    u16 lanes = umb9230s->phy_ctx.lanes;
    union _0x78 phy_interface_ctrl;
    u32 buf[2];

    if (enable) {
        /* datalane_ulps_rqst */
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_TX_PHY_INTERFACE_CTRL, &phy_interface_ctrl.val, 1);

        phy_interface_ctrl.bits.rf_phy_data_txrequlps = 1;
        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
    } else {
        /* is_pll_locked */
        if (!umb9230s_phy_tx_is_pll_locked(umb9230s)) {
            pr_err("umb9230s dphy pll is not locked\n");
            return -1;
        }

        /* datalane_ulps_exit */
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_TX_PHY_INTERFACE_CTRL, &phy_interface_ctrl.val, 1);
        phy_interface_ctrl.bits.rf_phy_data_txexitulps = 1;

        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

        switch (lanes) {
        /* Fall through */
        case 4:
            mask |= BIT(3);
        /* Fall through */
        case 3:
            mask |= BIT(2);
        /* Fall through */
        case 2:
            mask |= BIT(1);
        /* Fall through */
        case 1:
            mask |= BIT(0);
            break;
        default:
            break;
        }

        /*
         * verify that the DPHY has left ULPM
         * wait_datalane_stop_state
         */
        umb9230s_phy_tx_wait_datalane_stop_state(umb9230s, mask);

        /* datalane_ulps_rqst */
        phy_interface_ctrl.bits.rf_phy_data_txrequlps = 0;
        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

        /* datalane_ulps_exit */
        phy_interface_ctrl.bits.rf_phy_data_txexitulps = 0;
        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
    }

    return 0;
}

int umb9230s_dphy_tx_clk_ulps_en(struct umb9230s_device *umb9230s, int enable)
{
    union _0x78 phy_interface_ctrl;
    u32 buf[2];

    if (enable) {
        /* clklane_ulps_rqst */
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_TX_PHY_INTERFACE_CTRL, &phy_interface_ctrl.val, 1);
        phy_interface_ctrl.bits.rf_phy_clk_txrequlps = 1;

        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
    } else {
        /* is_pll_locked */
        if (!umb9230s_phy_tx_is_pll_locked(umb9230s)) {
            pr_err("umb9230s dphy pll is not locked\n");
            return -1;
        }

        /* clklane_ulps_exit */
        iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                        REG_DSI_TX_PHY_INTERFACE_CTRL, &phy_interface_ctrl.val, 1);

        phy_interface_ctrl.bits.rf_phy_clk_txexitulps = 1;
        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

        /*
         * verify that the DPHY has left ULPM
         * wait_clklane_stop_state
         */
        umb9230s_phy_tx_wait_clklane_stop_state(umb9230s);

        /* clklane_ulps_rqst */
        phy_interface_ctrl.bits.rf_phy_clk_txrequlps = 0;
        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

        /* clklane_ulps_exit */
        phy_interface_ctrl.bits.rf_phy_clk_txexitulps = 0;
        buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
        buf[1] = phy_interface_ctrl.val;
        iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);
    }

    return 0;
}

int umb9230s_phy_tx_configure(struct umb9230s_device *umb9230s)
{
    struct phy_tx_context *ctx = &umb9230s->phy_ctx;
    struct dphy_tx_pll_ops *pll = umb9230s->pll;
    struct dsi_reg reg;
    u32 buf[2];

    pr_info("lanes : %d\n", ctx->lanes);
    pr_info("freq : %d\n", ctx->freq);

    /* rstz */
    iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                    REG_DSI_TX_PHY_INTERFACE_CTRL, &reg.PHY_INTERFACE_CTRL.val, 1);
    reg.PHY_INTERFACE_CTRL.bits.rf_phy_reset_n = 0;

    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = reg.PHY_INTERFACE_CTRL.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* shutdownz */
    reg.PHY_INTERFACE_CTRL.bits.rf_phy_shutdown = 0;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = reg.PHY_INTERFACE_CTRL.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* clklane_en */
    reg.PHY_INTERFACE_CTRL.bits.rf_phy_clk_en = 0;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = reg.PHY_INTERFACE_CTRL.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* test_clr */
    buf[0] = REG_PHY_TX_TEST_CTRL;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* test_clr, clear by hardware automatically */
    buf[0] = REG_PHY_TX_TEST_CTRL;
    buf[1] = 1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    if (pll && pll->pll_config)
        pll->pll_config(&umb9230s->phy_ctx);
    if (pll && pll->timing_config)
        pll->timing_config(&umb9230s->phy_ctx);

    /* stop_wait_time */
    buf[0] = REG_DSI_TX_PHY_MIN_STOP_TIME;
    buf[1] = 0x1C;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* datalane_en */
    buf[0] = REG_DSI_TX_PHY_LANE_NUM_CONFIG;
    buf[1] = umb9230s->phy_ctx.lanes - 1;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* clklane_en */
    reg.PHY_INTERFACE_CTRL.bits.rf_phy_clk_en = 1;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = reg.PHY_INTERFACE_CTRL.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* shutdownz */
    reg.PHY_INTERFACE_CTRL.bits.rf_phy_shutdown = 1;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = reg.PHY_INTERFACE_CTRL.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* rstz */
    reg.PHY_INTERFACE_CTRL.bits.rf_phy_reset_n = 1;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = reg.PHY_INTERFACE_CTRL.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* wait_pll_locked */
    if (umb9230s_phy_tx_wait_pll_locked(umb9230s)) {
        return -1;
    }

    /* swap datalane0 and datalane2 */
    buf[0] = REG_PHY_TX_D2A_LANE_SWAP;
    buf[1] = 0xc6;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_PHY_TX_A2D_LANE0_MAP;
    buf[1] = 0x40;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    buf[0] = REG_PHY_TX_A2D_LANE2_MAP;
    buf[1] = 0;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    return 0;
}

int umb9230s_phy_tx_close(struct umb9230s_device *umb9230s)
{
    union _0x78 phy_interface_ctrl;
    u32 buf[2];

    pr_info("close phy tx\n");

    if (!umb9230s)
        return -1;

    /* rstz */
    iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                    REG_DSI_TX_PHY_INTERFACE_CTRL, &phy_interface_ctrl.val, 1);
    phy_interface_ctrl.bits.rf_phy_reset_n = 0;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = phy_interface_ctrl.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* shutdownz */
    phy_interface_ctrl.bits.rf_phy_shutdown = 0;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = phy_interface_ctrl.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* rstz */
    phy_interface_ctrl.bits.rf_phy_reset_n = 1;
    buf[0] = REG_DSI_TX_PHY_INTERFACE_CTRL;
    buf[1] = phy_interface_ctrl.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    return 0;
}

void umb9230s_phy_tx_hs_clk_en(struct umb9230s_device *umb9230s, int enable)
{
    union _0x74 phy_clk_lane_lp_ctrl;
    u32 buf[2];

    if (hr_type != HR_TYPE_UMB9230s)
        return;

    /* clk_hs_rqst */
    iic2cmd_read(umb9230s->i2c_bus, umb9230s->i2c_addr,
                    REG_DSI_TX_PHY_CLK_LANE_LP_CTRL, &phy_clk_lane_lp_ctrl.val, 1);
    phy_clk_lane_lp_ctrl.bits.auto_clklane_ctrl_en = 0;
    phy_clk_lane_lp_ctrl.bits.phy_clklane_tx_req_hs = enable;

    buf[0] = REG_DSI_TX_PHY_CLK_LANE_LP_CTRL;
    buf[1] = phy_clk_lane_lp_ctrl.val;
    iic2cmd_write(umb9230s->i2c_bus, umb9230s->i2c_addr, buf, 2);

    /* wait_pll_locked */
    umb9230s_phy_tx_wait_pll_locked(umb9230s);
}

