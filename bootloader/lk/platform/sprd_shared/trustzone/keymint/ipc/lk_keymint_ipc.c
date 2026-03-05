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

#include <ipc/lk_keymint_ipc.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef NELEMS
#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))
#endif

static struct trusty_ipc_chan _km_chan;
static struct trusty_ipc_dev* _ipc_dev;
static struct trusty_dev _tdev; /* There should only be one trusty device */
static bool _initialized;

static bool buffer_check(void* buffer, size_t buffer_size) {
    if ((buffer == NULL) && (buffer_size == 0))
        return true;
    else if ((buffer != NULL) && (buffer_size > 0))
        return true;
    else
        return false;
}

void km_ipc_init(void) {
    int rc;
    /* init Trusty device */
    trusty_info("Initializing Trusty device\n");
    rc = trusty_dev_init(&_tdev, NULL);
    if (rc != 0) {
        trusty_error("Initializing Trusty device failed (%d)\n", rc);
        return;
    }

    /* create Trusty IPC device */
    trusty_info("Initializing Trusty IPC device\n");
    rc = trusty_ipc_dev_create(&_ipc_dev, &_tdev, PAGE_SIZE);
    if (rc != 0) {
        trusty_error("Initializing Trusty IPC device failed (%d)\n", rc);
        return;
    }
}

int km_ipc_connect(void) {
    int rc = TRUSTY_ERR_GENERIC;

    trusty_info("Initializing LK Keymint IPC service\n");
    trusty_assert(_ipc_dev);
    trusty_ipc_chan_init(&_km_chan, _ipc_dev);
    trusty_debug("Connecting to keymint service\n");

    /* connect to km service and wait for connect to complete */
    rc = trusty_ipc_connect(&_km_chan, KEYMINT_PORT, true);
    if (rc < 0) {
        trusty_error("failed (%d) to connect to '%s'\n", rc, KEYMINT_PORT);
        return rc;
    }

    _initialized = true;

    return TRUSTY_ERR_NONE;
}

void km_ipc_disconnect(void) {
    if (!_initialized)
        return;
    /* close channel */
    trusty_ipc_close(&_km_chan);

    _initialized = false;
}

void km_ipc_shutdown(void) {
    trusty_info("Shutdown LK Keymint IPC service\n");
    (void)trusty_ipc_dev_shutdown(_ipc_dev);

    trusty_debug("Shutdown Trusty device\n");
    (void)trusty_dev_shutdown(&_tdev);
}

int lk_keymint_call(uint32_t cmd,
                    void* in, uint32_t in_size,
                    uint8_t* out, uint32_t* out_size) {
    int rc = 0;
    struct keymaster_message req_header = {.cmd = cmd};
    int num_iovecs = in_size ? 2 : 1;

    struct trusty_ipc_iovec req_iovs[2] = {
            {.base = &req_header, .len = sizeof(req_header)},
            {.base = in, .len = in_size},
    };

    rc = trusty_ipc_send(&_km_chan, req_iovs, num_iovecs, true);
    while ((rc < 0 && rc  == TRUSTY_ERR_NO_MEMORY)) {
        rc = trusty_ipc_send(&_km_chan, req_iovs, num_iovecs, true);
    }

    if (rc < 0) {
        trusty_error("%s: failed (%d) to send km request\n", __func__, rc);
        return rc;
    }
    trusty_info("Succeeded to send cmd (%d) to keymint TA.\n", cmd);

    size_t out_max_size = *out_size;
    *out_size = 0;

    struct trusty_ipc_iovec rsp_iovs[2];
    struct keymaster_message rsp_header;
    rsp_iovs[0].base = &rsp_header;
    rsp_iovs[0].len = sizeof(struct keymaster_message);
    while (true) {
        rsp_iovs[1].base = out + *out_size,
        rsp_iovs[1].len = MIN(KEYMINT_MAX_BUFFER_LENGTH, out_max_size - *out_size);
        rc = trusty_ipc_recv(&_km_chan, rsp_iovs, NELEMS(rsp_iovs), true);
        if (rc < 0) {
            trusty_error("Failed to retrieve response for cmd (%d) to %s: %d\n", cmd, KEYMINT_PORT,
                  rc);
            return rc;
        }

        trusty_debug("Getting response size (%d)\n", (int)rc);

        if ((size_t)rc < sizeof(struct keymaster_message)) {
            trusty_error("Invalid response size (%d)\n", (int)rc);
            return rc;
        }

        if ((cmd | KEYMASTER_RESP_BIT) != (rsp_header.cmd & ~(KEYMASTER_STOP_BIT))) {
            trusty_error("Invalid command (%d)\n", rsp_header.cmd);
            return rc;
        }
        *out_size += ((size_t)rc - sizeof(struct keymaster_message));
        if (rsp_header.cmd & KEYMASTER_STOP_BIT) {
            break;
        }
    }

    return rc;
}

keymaster_error_t lk_keymint_send(struct km_ipc_task* ipc_task,
                                  void* req_buffer, void* rsp_buffer) {
    uint8_t* send_buf = NULL;
    uint8_t* recv_buf = NULL;

    size_t req_size = (size_t)ipc_task->serialized_size(req_buffer);
    if (!buffer_check(req_buffer, req_size)) {
            return KM_ERROR_INVALID_ARGUMENT;
    }
    if (req_size > KEYMINT_SEND_BUF_SIZE) {
        trusty_error("Request too big: %lu Max size: %u\n", req_size, KEYMINT_SEND_BUF_SIZE);
        return KM_ERROR_INVALID_INPUT_LENGTH;
    }

    send_buf = (uint8_t*)trusty_calloc(1, KEYMINT_SEND_BUF_SIZE);
    if (send_buf == NULL) {
        trusty_error("Initialize send buffer failed!");
        goto end;
    }
    ipc_task->serialize(req_buffer, send_buf, send_buf + req_size);

    recv_buf = (uint8_t*)trusty_calloc(1, KEYMINT_RECV_BUF_SIZE);
    if (recv_buf == NULL) {
        trusty_error("Initialize receive buffer failed!");
        goto end;
    }
    uint32_t rsp_size = KEYMINT_RECV_BUF_SIZE;

    int rc = lk_keymint_call(ipc_task->cmd, send_buf, req_size, recv_buf, &rsp_size);
    if (rc < 0) {
        trusty_error("tipc error: %d\n", rc);
        ipc_task->error = KM_ERROR_UNKNOWN_ERROR;
        goto end;
    } else {
        trusty_info("Received %d byte response\n", rsp_size);
    }

    const uint8_t* p = recv_buf;
    if(!km_rsp_error_deserialize(&ipc_task->error, &p, p + sizeof(uint32_t))) {
        trusty_error("Deserializing error of response of failed!\n");
        ipc_task->error = KM_ERROR_UNKNOWN_ERROR;
        goto end;
    }
    rsp_size -= sizeof(uint32_t);
    if(ipc_task->error != KM_ERROR_OK) {
        trusty_error("Response of size %d contained error code %d\n", (int)rsp_size, (int)ipc_task->error);
        goto end;
    } else if (!((bool)ipc_task->deserialize(rsp_buffer, &p, p + rsp_size))) {
        trusty_error("Error deserializing response of size %d\n", (int)rsp_size);
        ipc_task->error = KM_ERROR_UNKNOWN_ERROR;
        goto end;
    }

end:
    trusty_free(send_buf);
    trusty_free(recv_buf);
    return ipc_task->error;
}
