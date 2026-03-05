/*
 * Copyright (C) 2021
 *
 */

#ifndef TRUSTY_QL_CADEMO_H_
#define TRUSTY_QL_CADEMO_H_

#include <trusty/sysdeps.h>
#include <trusty/trusty_ipc.h>

#define TADEMO_PORT "com.android.trusty.tademo"
#define SEND_BUF_SIZE    512
#define RECV_BUF_SIZE    512


/* Maximum buffer length for CA/TA communication can be 128KB*/
#define TADEMO_MAX_BUFFER_LENGTH 1024

enum tademo_command {
    TA_REQ_SHIFT = 1,
    TA_RESP_BIT  = 1,

    TA_INCREASE       = (0 << TA_REQ_SHIFT),
};

typedef enum {
    ERROR_NONE = 0,
    ERROR_FIRST = 1,
    ERROR_UNKNOWN = 2,
} tademo_error_t;


/**
 * tademo_message - Serial header for communicating with ta server
 * @cmd: the command, one of xx, xx. Payload must be a serialized
 *       buffer of the corresponding request object.
 * @payload: start of the serialized command specific payload
 */
struct tademo_message {
    uint32_t cmd;
    uint8_t payload[0];
};

int trusty_ql_cademo(void);

#endif /* TRUSTY_QL_CADEMO_H_ */
