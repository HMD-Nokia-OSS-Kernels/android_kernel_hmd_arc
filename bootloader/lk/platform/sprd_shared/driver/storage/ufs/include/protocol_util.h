/*
 * * Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * * you may not use this file except in compliance with the License.
 * * You may obtain a copy of the License at
 * * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * * Software distributed under the License is distributed on an "AS IS" BASIS,
 * * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * * See the Unisoc General Software License, version 1.0 for more details.
 * */

#ifndef SCSI_PROCESSOR_H
#define SCSI_PROCESSOR_H

/* Ref [2], Sec 11.3 */
#define UFS_OP_FORMAT_UNIT           (0x04)
#define UFS_OP_INQUIRY               (0x12)
#define UFS_OP_MODE_SELECT_10        (0x55)
#define UFS_OP_MODE_SENSE_10         (0x5A)
#define UFS_OP_PRE_FETCH_10          (0x34)
#define UFS_OP_PRE_FETCH_16          (0x90)
#define UFS_OP_READ_6                (0x08)
#define UFS_OP_READ_10               (0x28)
#define UFS_OP_READ_16               (0x88)
#define UFS_OP_READ_BUFFER           (0x3C)
#define UFS_OP_READ_CAPACITY_10      (0x25)
#define UFS_OP_READ_CAPACITY_16      (0x9E)
#define UFS_OP_REPORT_LUNS           (0xA0)
#define UFS_OP_REQUEST_SENSE         (0x03)
#define UFS_OP_SECURITY_PROTOCOL_IN  (0xA2)
#define UFS_OP_SECURITY_PROTOCOL_OUT (0xB5)
#define UFS_OP_SEND_DIAGNOSTIC       (0x1D)
#define UFS_OP_START_STOP_UNIT       (0x1B)
#define UFS_OP_SYNCHRONIZE_CACHE_10  (0x35)
#define UFS_OP_SYNCHRONIZE_CACHE_16  (0x91)
#define UFS_OP_TEST_UNIT_READY       (0x00)
#define UFS_OP_UNMAP                 (0x42)
#define UFS_OP_VERIFY_10             (0x2F)
#define UFS_OP_WRITE_6               (0x0A)
#define UFS_OP_WRITE_10              (0x2A)
#define UFS_OP_WRITE_16              (0x8A)
#define UFS_OP_WRITE_BUFFER          (0x3B)
#define SECURITY_PROTOCOL            (0xEC)

/* Function Prototypes */
void get_cmnd(uint32_t opcode, lbaint_t lba, lbaint_t size, uint8_t *cmd);

#endif
