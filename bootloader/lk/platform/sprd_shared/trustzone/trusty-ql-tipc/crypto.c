/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <trusty/crypto.h>
#include <linux/types.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <trusty/util.h>


#define LOCAL_LOG 1

static struct trusty_ipc_chan hwkey_chan;

static int crypto_tipc_init(struct trusty_ipc_dev* hwkey_ipc_dev, const char *port) {
    int ret = -1;
    trusty_ipc_chan_init(&hwkey_chan, hwkey_ipc_dev);
    trusty_debug("Connecting to Crypto service\n");

    /* connect to crypto service and wait for connect to complete */
    ret = trusty_ipc_connect(&hwkey_chan, port, true);
    if (ret < 0) {
        trusty_error("failed (%d) to connect to '%s'\n", ret, port);
    }

    return HWKEY_NO_ERROR;
}


static int crypto_send_request(uint32_t cmd, const void* req, size_t req_len) {
    struct crypto_msg *send_msg;
    size_t send_msg_size;
    struct trusty_ipc_iovec req_iov;
    send_msg_size = req_len + sizeof(struct crypto_msg);
    send_msg = malloc(send_msg_size);
    if (send_msg == NULL){
        trusty_error("%s Failed to request memory\n", __func__);
        return -HWKEY_ERR_NOT_FOUND;
    }
    send_msg->cmd = cmd;

    memcpy(send_msg->payload, req, req_len);
    trusty_info("%s send_msg_size:%zu\n", __func__, send_msg_size);

    req_iov.base = send_msg;
    req_iov.len = send_msg_size;

    int ret = trusty_ipc_send(&hwkey_chan, &req_iov, 1, true);
    free(send_msg);
    if (ret != 0) {
        trusty_error("%s send CA message failed (%d)\n", __func__, ret);
        return -HWKEY_ERR_NOT_FOUND;
    }
    return ret;
}

/* Checks that the command opcode in |header| matches |ex-ected_cmd|. Checks
 * that |tipc_result| is a valid response size. Returns negative on error.
 */
static int crypto_check_response_error(uint32_t expected_cmd,
                                struct crypto_msg header,
                                int32_t tipc_result) {
    if (tipc_result < 0) {
        trusty_error("failed (%d) to recv response\n", tipc_result);
        return tipc_result;
    }
    if ((size_t)tipc_result < sizeof(struct crypto_msg)) {
        trusty_error("invalid response size (%d)\n", tipc_result);
        return -HWKEY_ERR_GENERIC;
    }
    if ((header.cmd ) != (expected_cmd | HWKEY_RESP_BIT)) {
        trusty_error("malformed response\n");
        return -HWKEY_ERR_GENERIC;
    }
    return tipc_result;
}

static int crypto_read_response(uint32_t cmd, void* resp, size_t resp_len) {
    struct crypto_msg *rev_msg;
    size_t rev_msg_size;
    struct trusty_ipc_iovec res_iov;
    rev_msg_size = resp_len + sizeof(struct crypto_msg);
    rev_msg = malloc(rev_msg_size);
    if (rev_msg == NULL){
        trusty_error("%s Failed to request memory\n", __func__);
        return -HWKEY_ERR_NOT_FOUND;
    }
    rev_msg->cmd = cmd;

    res_iov.base = rev_msg;
    res_iov.len = rev_msg_size;
    int ret = trusty_ipc_recv(&hwkey_chan, &res_iov, 1, true);

    ret = crypto_check_response_error(cmd, *rev_msg, ret);
    if (ret < 0) {
        goto read_exit;
    }

    memcpy(resp, rev_msg->payload, 32);
    resp_len = ((size_t) ret) - sizeof(struct crypto_msg);
    if (NULL == resp  || resp_len == 0) {
        trusty_error("%s nothing to recv HWKEY\n", __func__);
        ret = -HWKEY_ERR_NOT_FOUND;
    }
read_exit:
    free(rev_msg);
    return ret;
}

static void crypto_tipc_shutdown(struct trusty_ipc_dev* hwkey_ipc_dev) {
    if (!hwkey_chan.dev)
        return;
    /*close channel*/
    trusty_info("shutdown Trusty Crypto client\n");
    trusty_ipc_close(&hwkey_chan);
}


int crypto_hwkey_derive(uint8_t* req, uint32_t req_size, uint8_t* resp, uint32_t resp_size) {
    static struct trusty_ipc_dev* hwkey_ipc_dev;
    static struct trusty_dev hwkey_tdev; /* There should only be one trusty device */
    uint32_t command = HWKEY_DERIVE;
    int rc = -1;

    /* init Trusty device */
    rc = trusty_dev_init(&hwkey_tdev, NULL);
    if (rc != 0) {
        trusty_error("%s Initializing Ca Trusty device failed (%d)\n", __func__, rc);
        return rc;
    }

    /* create Trusty IPC device */
    rc = trusty_ipc_dev_create(&hwkey_ipc_dev, &hwkey_tdev, PAGE_SIZE);
    if (rc != 0) {
        trusty_error("%s Initializing Ca Trusty IPC device failed (%d)\n", __func__, rc);
        return rc;
    }

    /*initi Trusty crypto-hwkey client*/
    rc =  crypto_tipc_init(hwkey_ipc_dev, HWKEY_PORT);
    if (rc < 0) {
        trusty_error("%s Error tipc init!\n", __func__, rc);
        return rc;
    }

    rc = crypto_send_request(command, req, req_size);
    if (rc < 0) {
        trusty_error("failed (%d) to send request!\n", rc);
        return rc;
    }

    rc = crypto_read_response(command, resp, resp_size);
    if (rc < 0) {
        trusty_error("%s: failed (%d) to read response\n", __func__, rc);
        return rc;
    }

    trusty_info("shutdown Trusty Crypto client\n");
    crypto_tipc_shutdown(hwkey_ipc_dev);

    trusty_info("shutdown Trusty IPC device\n");
    trusty_ipc_dev_shutdown(hwkey_ipc_dev);

    trusty_info("shutdown Trusty device\n");
    trusty_dev_shutdown(&hwkey_tdev);

    return rc;
}
