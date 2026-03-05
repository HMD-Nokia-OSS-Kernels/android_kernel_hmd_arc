/*
 * Copyright (C) 2019 unisoc.com
 */

#include <trusty/crypto.h>
#include <trusty/trusty_dev.h>
#include <trusty/trusty_ipc.h>
#include <trusty/util.h>
#include <stdio.h>

#define LOCAL_LOG 1

static struct trusty_ipc_dev* hwkey_ipc_dev;
static struct trusty_dev hwkey_tdev; /* There should only be one trusty device */

int hwkey_test(void)
{
    trusty_info("Enter hwkey_test!!!\n");
    uint32_t command = HWKEY_DERIVE;
    int rc = -1;

    uint8_t send_buf[] = {
            0xbc, 0x10, 0x6c, 0x9e, 0xc1, 0xa4, 0x71, 0x04,
            0x83, 0xab, 0x03, 0x4b, 0x75, 0x8a, 0xb3, 0x5e,
            0xfb, 0xe5, 0x43, 0x6c, 0xe6, 0x74, 0xb7, 0xfc,
            0xee, 0x20, 0xad, 0xae, 0xfb, 0x34, 0xab, 0xd3,
    };
    uint32_t request_size = sizeof(send_buf);
    uint32_t response_size = sizeof(send_buf);
    uint8_t recv_buf[response_size];

    rc = crypto_hwkey_derive(send_buf, request_size, recv_buf, response_size);
    if (rc < 0){
	trusty_error("get huk derived key failed with %d\n", rc);
    }

    trusty_info("Invoking TA return response_size is %d \n", response_size);
    for (int i = 0; i < response_size; i++){
        printf(" 0x%2x",recv_buf[i]);
    }
    printf("\n");

    return 0;
}
