/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <linux/types.h>
#include <malloc.h>
#include <lk_sec_drv.h>
#include <mmc.h>
#include <string.h>
#include <rpmb.h>
#include <trusty/trusty_dev.h>
#include <trusty/trusty_ipc.h>
#include <sec_rpmb.h>


#define BL_RPMB_PORT    "com.android.trusty.rpmb.bl"
#define BL_DATA_MAX_SIZE 256
#define BL_RPMB_DATA_SIZE 256
#define BL_RPMB_PAC_SIZE 512
#define BL_SESSION_MAGIC 0x53535343

struct bl_rpmb_msg {
    uint32_t cmd;
    uint32_t op_id;
    uint32_t size;
    int32_t  result;
    uint16_t blk_ind;
    uint32_t wr_count;
    uint8_t  data_wr[BL_RPMB_DATA_SIZE];
    uint8_t  pac[BL_RPMB_PAC_SIZE];
};

enum bl_rpmb_cmd {
    BL_REQ_SHIFT = 1,
    BL_RESP_BIT  = 1,

    BL_RESP_MSG_ERR   = BL_RESP_BIT,

    BL_DATA_RD           = 1 << BL_REQ_SHIFT,
    BL_DATA_WR	        = 2 << BL_REQ_SHIFT,
    BL_DATA_BLK_IND	 = 3 << BL_REQ_SHIFT,
    BL_SWP_CONFIG_RD     = 4 << BL_REQ_SHIFT,
    BL_SWP_CONFIG_WR	  = 5 << BL_REQ_SHIFT,
};

enum bl_rpmb_err {
    BL_NO_ERROR          = 0,
    BL_ERR_GENERIC       = 1,
    BL_ERR_NOT_VALID     = 2,
    BL_ERR_UNIMPLEMENTED = 3,
    BL_ERR_ACCESS        = 4,
    BL_ERR_NOT_FOUND     = 5,
    BL_ERR_EXIST         = 6,
};


static struct trusty_ipc_dev* rpmb_ipc_dev;
static struct trusty_dev rpmb_tdev; /* There should only be one trusty device */
static struct trusty_ipc_chan rpmb_chan;


/*
*@buf  The data read from RPMB
*@len   The buffer length must be 256
*@blk_ind rpmb reserve block number [1 - 13]
*Return value: zero is ok
*/
int sec_rpmb_read(void *buf, int len, int blk_ind)
{
    uint8_t data_rd[BL_DATA_MAX_SIZE];
    uint16_t block_ind, block_count;
    int ret = 0, i = 0;
    struct bl_rpmb_msg msg;
    struct trusty_ipc_iovec req_iov = {
            .base = &msg,
            .len = sizeof(msg)
    };

    if (NULL == buf) {
        errorf("%s: buffer is NULL\n", __func__);
        return -1;
    }

    if (BL_DATA_MAX_SIZE != len) {
        errorf("%s: invalid buffer length %d\n", __func__, len);
        return -1;
    }



    /* init Trusty device */
    ret = trusty_dev_init(&rpmb_tdev, NULL);
    if (ret != 0) {
        errorf("Initializing rpmb Trusty device failed (%d)\n", ret);
        return ret;
    }

    /* create Trusty IPC device */
    ret = trusty_ipc_dev_create(&rpmb_ipc_dev, &rpmb_tdev, PAGE_SIZE);
    if (ret != 0) {
        errorf("Initializing rpmb Trusty IPC device failed (%d)\n", ret);
        goto error_dev;
    }

    trusty_ipc_chan_init(&rpmb_chan, rpmb_ipc_dev);

    /* connect to km service and wait for connect to complete */
    ret = trusty_ipc_connect(&rpmb_chan, BL_RPMB_PORT, true);
    if (ret < 0) {
        errorf("failed (%d) to connect to '%s'\n", ret, BL_RPMB_PORT);
        goto error_ipc_dev;
    }

    msg.cmd = BL_DATA_BLK_IND;
    msg.blk_ind = blk_ind;

    ret = trusty_ipc_send(&rpmb_chan, &req_iov, 1, true);
    if (ret != 0) {
        errorf("send rpmb message failed (%d)\n", ret);
        goto error_chan;
    }

    ret = trusty_ipc_recv(&rpmb_chan, &req_iov, 1, true);
    if (ret < 0) {
        errorf("failed (%d) to recv rpmb response\n", ret);
        goto error_chan;
    }
    if (ret < sizeof(struct bl_rpmb_msg)) {
        errorf("invalid rpmb response size (%d)\n", ret);
        ret = -1;
        goto error_chan;
    }
    if (msg.cmd != (BL_DATA_BLK_IND | BL_RESP_BIT)) {
        errorf("invalid lk rpmb response cmd 0x%x\n", msg.cmd);
        ret = -1;
        goto error_chan;
    }
    if (msg.result != BL_NO_ERROR) {
        errorf("invalid rpmb response result 0x%x\n", msg.result);
        ret = -1;
        goto error_chan;
    }

    (void)trusty_ipc_close(&rpmb_chan);

    (void)trusty_ipc_dev_shutdown(rpmb_ipc_dev);

    (void)trusty_dev_shutdown(&rpmb_tdev);


    block_ind = msg.blk_ind;
    memset(data_rd, 0x0, sizeof(data_rd));
    block_count = sizeof(data_rd) / BL_RPMB_DATA_SIZE;
    for (i = 0; i < block_count; i++) {
        ret = rpmb_blk_read(data_rd + (i * BL_RPMB_DATA_SIZE), block_ind + i, 1);
        if(ret < 0) {
            dprintf(INFO,"%s: rpmb data read %d fail! ret %d \n", __func__, block_ind + i, ret);
            return ret;
        }
    }

    dprintf(INFO,"%s: rpmb block %d read successful \n", __func__, block_ind);

    memcpy((void *)buf, data_rd, len);

    return 0;

error_chan:
     (void)trusty_ipc_close(&rpmb_chan);
error_ipc_dev:
     (void)trusty_ipc_dev_shutdown(rpmb_ipc_dev);
error_dev:
    (void)trusty_dev_shutdown(&rpmb_tdev);
    return ret;
}

/*
*@buf  The data to write to RPMB
*@len   The buffer length must be 256
*@blk_ind rpmb reserve block number [1 - 13]
*Return value: zero is ok
*
*/
int sec_rpmb_write(void *buf, int len, int blk_ind)
{
    uint16_t block_ind, block_count;
    struct bl_rpmb_msg msg;
    int ret = -1, j = 0;
    struct trusty_ipc_iovec req_iov = {
            .base = &msg,
            .len = sizeof(msg)
    };

    if (NULL == buf) {
        errorf("%s: buffer is NULL\n", __func__);
        return -1;
    }

    if (BL_DATA_MAX_SIZE != len) {
        errorf("%s: invalid buffer length %d\n", __func__, len);
        return -1;
    }

    block_ind = blk_ind;

    block_count = BL_DATA_MAX_SIZE / BL_RPMB_DATA_SIZE;

    /* init Trusty device */
    ret = trusty_dev_init(&rpmb_tdev, NULL);
    if (ret != 0) {
        errorf("Initializing rpmb Trusty device failed (%d)\n", ret);
        return ret;
    }

    /* create Trusty IPC device */
    ret = trusty_ipc_dev_create(&rpmb_ipc_dev, &rpmb_tdev, PAGE_SIZE);
    if (ret != 0) {
        errorf("Initializing rpmb Trusty IPC device failed (%d)\n", ret);
        goto err_dev;
    }

    trusty_ipc_chan_init(&rpmb_chan, rpmb_ipc_dev);

    /* connect to km service and wait for connect to complete */
    ret = trusty_ipc_connect(&rpmb_chan, BL_RPMB_PORT, true);
    if (ret < 0) {
        errorf("failed (%d) to connect to '%s'\n", ret, BL_RPMB_PORT);
        goto err_ipc_dev;
    }

    for (j = 0; j < block_count; j++) {

        memcpy((void *)msg.data_wr, buf + (j * BL_RPMB_DATA_SIZE), BL_RPMB_DATA_SIZE);
        msg.cmd = BL_DATA_WR;
        msg.blk_ind = block_ind + j;
        msg.wr_count = rpmb_read_writecount();
        memset(msg.pac, 0x0, sizeof(msg.pac));

        ret = trusty_ipc_send(&rpmb_chan, &req_iov, 1, true);
        if (ret != 0) {
            errorf("send rpmb message failed (%d)\n", ret);
            goto err_chan;
        }

        ret = trusty_ipc_recv(&rpmb_chan, &req_iov, 1, true);
        if (ret < 0) {
            errorf("failed (%d) to recv lk rpmb response\n", ret);
            goto err_chan;
        }
        if (ret < sizeof(struct bl_rpmb_msg)) {
            errorf("invalid rpmb response size (%d)\n", ret);
            ret = -1;
            goto err_chan;
        }
        if (msg.cmd != (BL_DATA_WR | BL_RESP_BIT)) {
            errorf("invalid rpmb response cmd 0x%x\n", msg.cmd);
            ret = -1;
            goto err_chan;
        }
        if (msg.result != BL_NO_ERROR) {
            errorf("invalid rpmb response result 0x%x\n", msg.result);
            ret = -1;
            goto err_chan;
        }

        ret = rpmb_write_pac(msg.pac, block_ind + j);
        if(ret < 0) {
            errorf("%s: rpmb data write %d fail! ret %d \n", __func__, block_ind + j, ret);
            goto err_chan;
        }

        dprintf(INFO,"%s:rpmb block %d write  successful \n", __func__, block_ind +j);
    }

    (void)trusty_ipc_close(&rpmb_chan);

    (void)trusty_ipc_dev_shutdown(rpmb_ipc_dev);

    (void)trusty_dev_shutdown(&rpmb_tdev);

    return 0;

err_chan:
     (void)trusty_ipc_close(&rpmb_chan);
err_ipc_dev:
     (void)trusty_ipc_dev_shutdown(rpmb_ipc_dev);
err_dev:
    (void)trusty_dev_shutdown(&rpmb_tdev);
    return ret;
}


/*
*@buf  The Secure Write Protect Configuration Block  write to RPMB
*@len   The buffer length must be 256
*Return value: zero is ok
*
*/
int sec_rpmb_swp_config_write(void *buf, int len)
{
    struct bl_rpmb_msg msg;
    int ret = -1;
    struct trusty_ipc_iovec req_iov = {
            .base = &msg,
            .len = sizeof(msg)
    };

    if (NULL == buf) {
        errorf("%s: buffer is NULL\n", __func__);
        return -1;
    }

    if (BL_RPMB_DATA_SIZE != len) {
        errorf("%s: invalid buffer length %d\n", __func__, len);
        return -1;
    }

    /* init Trusty device */
    ret = trusty_dev_init(&rpmb_tdev, NULL);
    if (ret != 0) {
        errorf("Initializing rpmb Trusty device failed (%d)\n", ret);
        return ret;
    }

    /* create Trusty IPC device */
    ret = trusty_ipc_dev_create(&rpmb_ipc_dev, &rpmb_tdev, PAGE_SIZE);
    if (ret != 0) {
        errorf("Initializing rpmb Trusty IPC device failed (%d)\n", ret);
        goto err_dev;
    }

    trusty_ipc_chan_init(&rpmb_chan, rpmb_ipc_dev);

    /* connect to km service and wait for connect to complete */
    ret = trusty_ipc_connect(&rpmb_chan, BL_RPMB_PORT, true);
    if (ret < 0) {
        errorf("failed (%d) to connect to '%s'\n", ret, BL_RPMB_PORT);
        goto err_ipc_dev;
    }


    memcpy((void *)msg.data_wr, buf, BL_RPMB_DATA_SIZE);
    msg.cmd = BL_SWP_CONFIG_WR;
    msg.blk_ind = 0;
    msg.wr_count = rpmb_read_writecount();
    memset(msg.pac, 0x0, sizeof(msg.pac));

    ret = trusty_ipc_send(&rpmb_chan, &req_iov, 1, true);
    if (ret != 0) {
        errorf("send rpmb message failed (%d)\n", ret);
        goto err_chan;
    }

    ret = trusty_ipc_recv(&rpmb_chan, &req_iov, 1, true);
    if (ret < 0) {
        errorf("failed (%d) to recv lk rpmb response\n", ret);
        goto err_chan;
    }
    if (ret < sizeof(struct bl_rpmb_msg)) {
        errorf("invalid rpmb response size (%d)\n", ret);
        ret = -1;
        goto err_chan;
    }
    if (msg.cmd != (BL_SWP_CONFIG_WR | BL_RESP_BIT)) {
        errorf("invalid rpmb response cmd 0x%x\n", msg.cmd);
        ret = -1;
        goto err_chan;
    }
    if (msg.result != BL_NO_ERROR) {
        errorf("invalid rpmb response result 0x%x\n", msg.result);
        ret = -1;
        goto err_chan;
    }

    ret = rpmb_write_pac(msg.pac, 0);
    if(ret < 0) {
        errorf("%s: rpmb config write  fail! ret %d \n", __func__, ret);
        goto err_chan;
    }

    dprintf(INFO,"%s:Secure Write Protect Configuration Block write successful \n", __func__);

    (void)trusty_ipc_close(&rpmb_chan);

    (void)trusty_ipc_dev_shutdown(rpmb_ipc_dev);

    (void)trusty_dev_shutdown(&rpmb_tdev);

    return 0;

err_chan:
     (void)trusty_ipc_close(&rpmb_chan);
err_ipc_dev:
     (void)trusty_ipc_dev_shutdown(rpmb_ipc_dev);
err_dev:
    (void)trusty_dev_shutdown(&rpmb_tdev);
    return ret;
}



/*
*@buf  The Secure Write Protect Block Configuration read from RPMB
*@len   The buffer length must be 256
*Return value: zero is ok
*/
int sec_rpmb_swp_config_read(void *buf, int len)
{
    if (NULL == buf) {
        errorf("%s: buffer is NULL\n", __func__);
        return -1;
    }

    if (BL_RPMB_DATA_SIZE != len) {
        errorf("%s: invalid buffer length %d\n", __func__, len);
        return -1;
    }

    return rpmb_swp_config_read(buf);
}


