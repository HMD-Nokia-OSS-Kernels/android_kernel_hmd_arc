/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

#include <lk/debug.h>
#include <lk/err.h>
#include <lk/compiler.h>
#include <lk/console_cmd.h>
#include <platform.h>
#include <platform/debug.h>
#include <kernel/thread.h>
#include <stdio.h>
#if WITH_LIB_CONSOLE
#include <lib/console.h>
#endif
#include <boot_mode.h>
#include <part_efi.h>
#include <lk/reg.h>
#include <sprd_common.h>
#include <adi_hal_internal.h>
#include <asm/arch/sprd_reg.h>
#include <asm/arch/common.h>
#include <chipram_env.h>
#include <adi_hal_internal.h>
#include <sprd_spi.h>

#include <part_efi.h>
#include <sprd_common.h>
#include <sprd_sizes.h>
#include <malloc.h>

#if defined(CONFIG_UFS)
enum command_ret_t {
	CMD_RET_SUCCESS,	/* 0 = Success */
	CMD_RET_FAILURE,	/* 1 = Failure */
	CMD_RET_USAGE = -1,	/* Failure, please report 'usage' error */
};

extern int common_raw_read(const char *part_name, uint64_t size, uint64_t offset, char *buf);
extern int common_raw_write(const char *part_name, uint64_t size,
				uint64_t updsize, uint64_t offset, char *buf);
extern int storage_write_protect_set(const char *ptn_name, uint64_t offset, uint64_t bytes);
extern int storage_write_protect_remove(const char *ptn_name, uint64_t offset, uint64_t bytes);
extern int storage_write_protect_check(const char *ptn_name, uint64_t offset, uint64_t bytes);
#endif

extern void fastboot_mode(void);
extern void recovery_mode(void);

static int cmd_fastboot(int argc, const cmd_args *argv) {
    fastboot_mode();
    return 0;
}

static int cmd_recovery(int argc, const cmd_args *argv) {
    recovery_mode();
    return 0;
}

static int cmd_normal_boot(int argc, const cmd_args *argv) {
    normal_mode();
    return 0;
}

static int cmd_read_register(int argc, const cmd_args *argv) {

    uint32_t addr;
    uint32_t value;

    addr = (uint32_t) simple_strtoul(argv[1].str, NULL, 16);
    value = CHIP_REG_GET(addr);
    dprintf(INFO,"0x%02x\n", value);

    return 0;
}

static int cmd_write_register(int argc, const cmd_args *argv) {

    uint32_t addr;
    uint32_t value;

    value = (uint32_t) simple_strtoul(argv[1].str, NULL, 16);
    addr = (uint32_t) simple_strtoul(argv[2].str, NULL, 16);
    CHIP_REG_SET(addr,value);
    if (CHIP_REG_GET(addr) == value){
        dprintf(INFO,"OK\n");
    }else{
        errorf("FAIL\n");
        return -1;
    }

    return 0;
}

static int cmd_adiread_register(int argc, const cmd_args *argv) {

    uint32_t addr;
    uint16_t value;

    addr = (uint32_t) simple_strtoul(argv[1].str, NULL, 16);
    value = ANA_REG_GET(addr);
    dprintf(INFO,"0x%02x\n", value);

    return 0;
}

static int cmd_adiwrite_register(int argc, const cmd_args *argv) {

    uint32_t addr;
    uint16_t value;

    value = (uint16_t) simple_strtoul(argv[1].str, NULL, 16);
    addr = (uint32_t) simple_strtoul(argv[2].str, NULL, 16);
    ANA_REG_SET(addr,value);
    if (ANA_REG_GET(addr) == value){
        dprintf(INFO,"OK\n");
    }else{
        errorf("FAIL\n");
        return -1;
    }

    return 0;
}

#if defined(CONFIG_RFSPIRW)
static int cmd_rfspi_read_register(int argc, const cmd_args *argv) {

    uint16_t addr;
    uint16_t value1;
    uint16_t value2;

    addr = (uint16_t) simple_strtoul(argv[1].str, NULL, 16);
    value1 = SPI_REG_GET(addr);
    value2 = SPI_REG_GET(addr);
    dprintf(INFO,"0x%02x\n", value2);

    return 0;
}

static int cmd_rfspi_write_register(int argc, const cmd_args *argv) {

    uint16_t addr;
    uint16_t value;
    uint16_t value1;
    uint16_t value2;

    value = (uint16_t) simple_strtoul(argv[1].str, NULL, 16);
    addr = (uint16_t) simple_strtoul(argv[2].str, NULL, 16);
    SPI_REG_SET(addr,value);
    value1 = SPI_REG_GET(addr);
    value2 = SPI_REG_GET(addr);
    if (value2 == value){
        dprintf(INFO,"OK\n");
    }else{
        errorf("FAIL\n");
    }

    return 0;
}
#endif

#if defined(CONFIG_UFS)
cmd_args *argv_global;
int argc_global;

int test_ufs_set_swp(int argc, const cmd_args *argv)
{
	int ret = 0;
	uint64_t bytes = 0;
	uint64_t offset = 0;
	argc_global = argc;
	argv_global = argv;
	errorf("%s:%d\n",__func__,__LINE__);

//	ret = storage_write_protect_set(argv[0].str, offset, bytes);
	ret = storage_write_protect_set("boot_b", 0, 0x1000);
	if (-1 == ret)
		errorf("set swp entry fail!\n");
	errorf("%s:%d\n",__func__,__LINE__);

	return ret;
}

int test_ufs_remove_swp(int argc, const cmd_args *argv)
{
	int ret = 0;
	uint64_t bytes = 0;
	uint64_t offset = 0;
	ret = storage_write_protect_remove("boot_b", 0, 0x1000);
	if (-1 == ret)
		errorf("remove swp entry fail!\n");

	return ret;
}

int test_ufs_check_swp(int argc, const cmd_args *argv)
{
	int ret = 0;
	uint64_t bytes = 0;
	uint64_t offset = 0;
	ret = storage_write_protect_check("boot_b", 0, 0x1000);
	if (0 == ret)
		debugf("This entry is protected!\n");

	if (1 == ret)
		debugf("This entry is not protected!\n");

	if (-1 == ret)
		debugf("check swp entry fail!\n");

	return 0;
}

int test_ufs_read(int argc, const cmd_args *argv)
{
	int ret = 0;
	uint64_t size = 0;
	uint64_t offset = 0;
	int i =0;
	char* buf = malloc(SZ_32K);
	if (NULL == buf) {
		errorf("Malloc for read buf fail\n");
		return CMD_RET_FAILURE;
	}
	memset(buf, 0, SZ_32K);

	ret = common_raw_read("boot_b",  0x100,0, buf);
	if (-1 == ret) {
		errorf("common_raw_read fail!\n");
		goto fail;
	}

	debugf("Read result:");
	for (i = 0; i < 10; i += 4) {
		if (0 == i % 16)
			dprintf(INFO,"\n");
		dprintf(INFO,"%08x ", *(uint32_t *)(buf + i));
	}
	dprintf(INFO,"\n");

	free(buf);
	return 0;
fail:
	free(buf);
	return CMD_RET_FAILURE;
}

int test_ufs_write(int argc, const cmd_args *argv)
{
	int ret = 0;
	uint64_t size = 0;
	uint64_t offset = 0;
	int i =0;
	int value = 0;
	char* buf = malloc(SZ_32K);
	if (NULL == buf) {
		errorf("Malloc for write buf fail\n");
		return CMD_RET_FAILURE;
	}

	memset(buf, 'a', size);

	ret = common_raw_write("boot_b", 0x100, 0, 0, buf);
	if (-1 == ret) {
		errorf("common_raw_write fail!\n");
		goto fail;
	}
	debugf("Write OK\n");

	memset(buf, 0, SZ_32K);
	free(buf);
	return 0;
fail:
	free(buf);
	return CMD_RET_FAILURE;

}
#endif

STATIC_COMMAND_START
STATIC_COMMAND("fastboot", "entry fastboot mode", &cmd_fastboot)
STATIC_COMMAND("recovery", "entry recovery mode", &cmd_recovery)
STATIC_COMMAND("boot", "entry normal boot mode", &cmd_normal_boot)
STATIC_COMMAND("read", "read register", &cmd_read_register)
STATIC_COMMAND("write", "write register", &cmd_write_register)
STATIC_COMMAND("adiread", "adi read register", &cmd_adiread_register)
STATIC_COMMAND("adiwrite", "adi write register", &cmd_adiwrite_register)

#if defined(CONFIG_RFSPIRW)
STATIC_COMMAND("rfspiread", "rfspi read register", &cmd_rfspi_read_register)
STATIC_COMMAND("rfspiwrite", "rfspi write register", &cmd_rfspi_write_register)
#endif

#if defined(CONFIG_UFS)
STATIC_COMMAND("test_ufs_set_swp", "test_ufs_set_swp", &test_ufs_set_swp)
STATIC_COMMAND("test_ufs_remove_swp", "test_ufs_remove_swp", &test_ufs_remove_swp)
STATIC_COMMAND("test_ufs_check_swp", "test_ufs_check_swp", &test_ufs_check_swp)
STATIC_COMMAND("test_ufs_read", "test_ufs_read", &test_ufs_read)
STATIC_COMMAND("test_ufs_write", "test_ufs_write", &test_ufs_write)
/*
please using cmd:
 test_ufs_set_swp boot_b 0 100 1 11 1
 test_ufs_check_swp boot_b 0 100 1 11 1
 test_ufs_write boot_b 0 100 1 11 1
 test_ufs_read boot_b 0 100 1 11 1
 test_ufs_remove_swp boot_b 0 100 1 11 1
 test_ufs_check_swp boot_b 0 100 1 11 1
 **/
#endif

STATIC_COMMAND_END(sprd_shlle_cmd);

