/*
 * <sprd_sizes.h>
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <sprd_sizes.h>
 * This program is free software; you can redistribute it and/or modify.
 */

#ifndef _SPRD_SIZES_H_
#define _SPRD_SIZES_H_

#define SZ_1				(1<<0) // 1 bytes
#define SZ_2				(1<<1) // 2 bytes
#define SZ_4				(1<<2) // 4 bytes
#define SZ_8				(1<<3) // 8 bytes
#define SZ_16				(1<<4) // 16 bytes
#define SZ_32				(1<<5) // 32 bytes
#define SZ_64				(1<<6) // 64 bytes
#define SZ_128			(1<<7) // 128 bytes
#define SZ_256			(1<<8) // 256 bytes
#define SZ_512			(1<<9) // 512 bytes

#define SZ_1K				(1<<10) // 1 KB
#define SZ_2K				(1<<11) // 2 KB
#define SZ_4K				(1<<12) // 4 KB
#define SZ_8K				(1<<13) // 8 KB
#define SZ_16K			(1<<14) // 16 KB
#define SZ_32K			(1<<15) // 32 KB
#define SZ_64K			(1<<16) // 64 KB
#define SZ_128K			(1<<17) // 128 KB
#define SZ_256K			(1<<18) // 256 KB
#define SZ_512K			(1<<19) // 512 KB

#define SZ_1M				(1<<20) // 1 MB
#define SZ_2M				(1<<21) // 2 MB
#define SZ_4M				(1<<22) // 4 MB
#define SZ_8M				(1<<23) // 8 MB
#define SZ_16M			(1<<24) // 16 MB
#define SZ_32M			(1<<25) // 32 MB
#define SZ_64M			(1<<26) // 64 MB
#define SZ_128M			(1<<27) // 128 MB
#define SZ_256M			(1<<28) // 256 MB
#define SZ_512M			(1<<29) // 512 MB

#define SZ_1G				(1<<30) // 1 GB
#define SZ_2G				(1<<31) // 2 GB

#ifdef CONFIG_VERIFY_GPT
#define SZ_H                            0x200   //sign header size
#define SZ_E                            0x4000  //entry size
#define SZ_K                            0xe00   //key size
#define SZ_D                            0x1000  //verify data size
#define GPT_DATA_SIZE 			0x5000  //signheader+entry+key
#endif

#endif /* _SPRD_SIZES_H_ */
