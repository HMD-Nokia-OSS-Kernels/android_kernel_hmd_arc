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

#include <asm/arch/common.h>
#include <part.h>
#include <sprd_ufs.h>
#include "protocol_util.h"

void get_cmnd(uint32_t opcode, lbaint_t lba, lbaint_t size, uint8_t *cmd)
{
	//debugf("Logical blk addr 0x%lx, size 0x%lx \n",lba, size);
	memset(cmd,0x0,16);

	switch (opcode)
	{
		case UFS_OP_READ_10 :
			cmd[0] = UFS_OP_READ_10;
			cmd[1] = 0;
			cmd[2] = (uint8_t)((lba & 0xff000000) >> 24); /* MSB Byte */
			cmd[3] = (uint8_t)((lba & 0x00ff0000) >> 16);
			cmd[4] = (uint8_t)((lba & 0x0000ff00) >> 8);
			cmd[5] = (uint8_t)(lba & 0x000000ff); /* LSB byte */
			cmd[6] = 0;
			cmd[7] = (uint8_t)((size >> 8) & 0xff);
			cmd[8] = (uint8_t)((size) & 0xff);
			cmd[9] = 0;
			break;

		case UFS_OP_WRITE_10 :
			cmd[0] = UFS_OP_WRITE_10;
			cmd[1] = 0;
			cmd[2] = (uint8_t)((lba & 0xff000000) >> 24); /* MSB Byte */
			cmd[3] = (uint8_t)((lba & 0x00ff0000) >> 16);
			cmd[4] = (uint8_t)((lba & 0x0000ff00) >> 8);
			cmd[5] = (uint8_t)(lba & 0x000000ff); /* LSB byte */
			cmd[6] = 0;
			cmd[7] = (uint8_t)((size >> 8) & 0xff);
			cmd[8] = (uint8_t)((size) & 0xff);
			cmd[9] = 0;
			break;

		case UFS_OP_TEST_UNIT_READY :
			cmd[0] = UFS_OP_TEST_UNIT_READY;
			cmd[1] = 0;
			cmd[2] = 0;
			cmd[3] = 0;
			cmd[4] = 0;
			cmd[5] = 0;
			break;

		case UFS_OP_SECURITY_PROTOCOL_IN :
			cmd[0] = UFS_OP_SECURITY_PROTOCOL_IN;
			cmd[1] = SECURITY_PROTOCOL;  /* Manju updated from 0x00 */
			cmd[2] = 0x00;
			cmd[3] = 0x01;
			cmd[4] = 0x00;
			cmd[5] = 0x00;
			cmd[6] = (uint8_t)(size >> 24);
			cmd[7] = (uint8_t)((size >> 16) & 0xff);
			cmd[8] = (uint8_t)((size >> 8) & 0xff);
			cmd[9] = (uint8_t)(size & 0xff);
			cmd[10] = 0x00;
			cmd[11] = 0x00;
			break;

		case UFS_OP_SECURITY_PROTOCOL_OUT:
			cmd[0] = UFS_OP_SECURITY_PROTOCOL_OUT;
			cmd[1] = SECURITY_PROTOCOL;
			cmd[2] = 0x00;
			cmd[3] = 0x01;
			cmd[4] = 0x00;
			cmd[6] = (uint8_t)((size >> 24));
			cmd[7] = (uint8_t)((size >> 16) & 0xff);
			cmd[8] = (uint8_t)((size >> 8) & 0xff);
			cmd[9] = (uint8_t)(size & 0xff);
			cmd[10] = 0x00;
			cmd[11] = 0x00;
			break;

		case UFS_OP_START_STOP_UNIT:
			cmd[0] = UFS_OP_START_STOP_UNIT;
			cmd[1] = 0;
			cmd[2] = 0;
			cmd[3] = 0;
			cmd[4] = UFS_POWERDOWN_PWR_MODE << 4;
			cmd[5] = 0;
			cmd[6] = 0;
			cmd[7] = 0;
			cmd[8] = 0;
			cmd[9] = 0;
			break;

		case UFS_OP_UNMAP:
			cmd[0] = UFS_OP_UNMAP;
			cmd[1] = 0;
			cmd[2] = 0;
			cmd[3] = 0;
			cmd[4] = 0;
			cmd[5] = 0;
			cmd[6] = 0;
			/*use size as parameter list length*/
			cmd[7] = (uint8_t)((size >> 8) & 0xff);
			cmd[8] = (uint8_t)((size) & 0xff);
			cmd[9] = 0;
			break;

		case UFS_OP_SYNCHRONIZE_CACHE_10:
			cmd[0] = UFS_OP_SYNCHRONIZE_CACHE_10;
			cmd[1] = 0;
			cmd[2] = (uint8_t)((lba & 0xff000000) >> 24); /* MSB Byte */
			cmd[3] = (uint8_t)((lba & 0x00ff0000) >> 16);
			cmd[4] = (uint8_t)((lba & 0x0000ff00) >> 8);
			cmd[5] = (uint8_t)(lba & 0x000000ff); /* LSB byte */
			cmd[6] = 0;
			cmd[7] = (uint8_t)((size >> 8) & 0xff);
			cmd[8] = (uint8_t)((size) & 0xff);
			cmd[9] = 0;
			break;

		default:
			break;
	}
}

