/*
 * Copyright (C) 2020 spreadtrum.com
 */

#include <linux/types.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <trusty/trusty_dev.h>
#include <trusty/trusty_ipc.h>
#include <lk/debug.h>

#include "trusty_ql_cademo.h"


static struct trusty_ipc_dev* cademo_ipc_dev;
static struct trusty_dev cademo_tdev; /* There should only be one trusty device */
static struct trusty_ipc_chan cademo_chan;


int trusty_cademo_call(uint32_t cmd, void *in, uint32_t in_size, uint8_t *out, uint32_t *out_size)
{
    struct trusty_ipc_iovec req_iov, res_iov;
    struct tademo_message *msg;
    size_t msg_size;
    int ret = -1;


    msg_size = in_size + sizeof(struct tademo_message);
    msg = malloc(msg_size);
    msg->cmd = cmd;

    memcpy(msg->payload, in, in_size);
    dprintf(INFO,"%s msg_size:%zu\n", __func__, msg_size);

    req_iov.base = msg;
    req_iov.len = msg_size;


    /* init Trusty device */
    ret = trusty_dev_init(&cademo_tdev, NULL);
    if (ret != 0) {
        dprintf(INFO,"%s Initializing cademo Trusty device failed (%d)\n", __func__, ret);
        return ret;
    }

    /* create Trusty IPC device */
    ret = trusty_ipc_dev_create(&cademo_ipc_dev, &cademo_tdev, PAGE_SIZE);
    if (ret != 0) {
        dprintf(INFO,"%s Initializing cademo Trusty IPC device failed (%d)\n", __func__, ret);
        goto err_dev;
    }

    trusty_ipc_chan_init(&cademo_chan, cademo_ipc_dev);


    /* connect to cademo service and wait for connect to complete */
    ret = trusty_ipc_connect(&cademo_chan, TADEMO_PORT, true);
    if (ret < 0) {
        dprintf(INFO,"%s failed (%d) to connect to '%s'\n", __func__, ret, TADEMO_PORT);
        goto err_ipc_dev;
    }

    ret = trusty_ipc_send(&cademo_chan, &req_iov, 1, true);
    if (ret != 0) {
        dprintf(INFO,"%s send cademo message failed (%d)\n", __func__, ret);
        goto err_chan;
    }

    if (NULL == out  || *out_size < 0) {
        dprintf(INFO,"%s nothing to recv cademo's TA\n", __func__);
        goto err_chan;
    }
    res_iov.base = out;
    res_iov.len = *out_size;

    ret = trusty_ipc_recv(&cademo_chan, &res_iov, 1, true);
    if (ret < 0) {
        dprintf(INFO,"%s failed (%d) to recv cademo's TA response\n", __func__, ret);
        goto err_chan;
    }

    if (ret < sizeof(struct tademo_message)) {
        dprintf(INFO,"%s invalid cademo response size (%d)\n", __func__, ret);
        ret = -1;
        goto err_chan;
    }

    msg = (struct tademo_message *) out;

    if (msg->cmd != (cmd | TA_RESP_BIT)) {
        dprintf(INFO,"%s invalid cademo response cmd 0x%x\n", __func__, msg->cmd);
        ret = -1;
        goto err_chan;
    }

    (void)trusty_ipc_close(&cademo_chan);

    (void)trusty_ipc_dev_shutdown(cademo_ipc_dev);

    (void)trusty_dev_shutdown(&cademo_tdev);

    *out_size = ((size_t) ret) - sizeof(struct tademo_message);

    dprintf(INFO,"%s: trusty_cademo_call(cmd=0x%x) successful \n", __func__, cmd);

    return 0;

err_chan:
     (void)trusty_ipc_close(&cademo_chan);
err_ipc_dev:
     (void)trusty_ipc_dev_shutdown(cademo_ipc_dev);
err_dev:
    (void)trusty_dev_shutdown(&cademo_tdev);
    return ret;
}


int trusty_ql_cademo(void)
{
    uint32_t command = TA_INCREASE;
    uint8_t recv_buf[RECV_BUF_SIZE];
    uint32_t response_size = RECV_BUF_SIZE;
    uint8_t send_buf[SEND_BUF_SIZE];
    uint32_t request_size = SEND_BUF_SIZE;
    struct tademo_message *msg;
    uint8_t *payload;
    int rc;


    send_buf[0] = 99;
    dprintf(INFO,"%s Before  counter: %d\n", __func__, send_buf[0]);
    rc = trusty_cademo_call(command, send_buf, request_size, recv_buf, &response_size);
    if (rc < 0) {
        dprintf(INFO,"%s error (%d) calling  TA\n", __func__, rc);
        return ERROR_UNKNOWN;
    }

    msg = (struct tademo_message *)(recv_buf);
    payload = msg->payload;

    dprintf(INFO,"%s Invoking TA to increment  counter: %d\n", __func__, payload[0]);
    return 0;
}
