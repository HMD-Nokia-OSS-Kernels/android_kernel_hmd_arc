/*
 * An ETB device/driver
 * Copyright (C) 2016 Spreadtrum Communication Inc
 *
 */

#include <sprd_sizes.h>
#include <asm/types.h>
#include <asm/arch/common.h>
#include <sprd_common.h>
#include <lcd.h>
#include <asm/arch/sprd_reg.h>

#ifdef SPRD_CORESIGHT_ETB
#define ETB_BASE			SPRD_CORESIGHT_ETB
#else
#error "SPRD_CORESIGHT_ETB undefined!"
#endif

#define ETB_RAM_DEPTH_LK_REG		0x004
#define ETB_STATUS_LK_REG		0x00c
#define ETB_RAM_READ_DATA_LK_REG	0x010
#define ETB_RAM_READ_POINTER_LK		0x014
#define ETB_RAM_WRITE_POINTER_LK	0x018
#define ETB_CTL_REG_LK			0x020
#define ETB_FFCR_LK			0x304

#define ETB_STATUS_RAM_FULL_LK		BIT_0
#define ETB_FFCR_FON_MAN_LK		BIT_6
#define ETB_FFCR_STOP_FI_LK		BIT_12
#define ETB_FRAME_SIZE_WORDS		4

#define CORESIGHT_UNLOCK		0xc5acce55
#define CORESIGHT_LAR			0xfb0

unsigned char etb_dump_mem[SZ_32K] = {0};
u32 etb_buf_size;

static inline void CS_LOCK(u32 addr)
{
	do {
		/* Wait for things to settle */
		//mb();
		writel(0x0, addr + CORESIGHT_LAR);
	} while (0);
}
static inline void CS_UNLOCK(u32 addr)
{
	do {
		writel(CORESIGHT_UNLOCK, addr + CORESIGHT_LAR);
		/* Make sure everyone has seen this */
		//mb();
	} while (0);
}

void sprd_etb_hw_dis(void)
{
	u32 ffcr;
	CS_UNLOCK(ETB_BASE);

	ffcr = readl(ETB_BASE + ETB_FFCR_LK);
	/* stop formatter when a stop has completed */
	ffcr |= ETB_FFCR_STOP_FI_LK;
	writel(ffcr, ETB_BASE + ETB_FFCR_LK);
	/* manually generate a flush of the system */
	ffcr |= ETB_FFCR_FON_MAN_LK;
	writel(ffcr, ETB_BASE + ETB_FFCR_LK);

	writel(0x0, ETB_BASE + ETB_CTL_REG_LK);
	CS_LOCK(ETB_BASE);
}

void sprd_etb_dump (void)
{
	int i;
	unsigned char *buf_ptr = NULL;
	u32 read_data;
	u32 read_ptr, write_ptr;
	u32 frame_off, frame_endoff;

	buf_ptr = (unsigned char *)etb_dump_mem;

	CS_UNLOCK(ETB_BASE);

	read_ptr = readl(ETB_BASE + ETB_RAM_READ_POINTER_LK);
	write_ptr = readl(ETB_BASE + ETB_RAM_WRITE_POINTER_LK);
	etb_buf_size = readl(ETB_BASE + ETB_RAM_DEPTH_LK_REG);

	frame_off = write_ptr % ETB_FRAME_SIZE_WORDS;
	frame_endoff = ETB_FRAME_SIZE_WORDS - frame_off;
	if (frame_off)
		write_ptr += frame_endoff;

	if ((readl(ETB_BASE + ETB_STATUS_LK_REG) & ETB_STATUS_RAM_FULL_LK) == 0)
		writel(0x0, ETB_BASE + ETB_RAM_READ_POINTER_LK);
	else
		writel(write_ptr, ETB_BASE + ETB_RAM_READ_POINTER_LK);

	for (i = 0; i < etb_buf_size; i++) {
		read_data = readl(ETB_BASE + ETB_RAM_READ_DATA_LK_REG);
		*buf_ptr++ = read_data >> 0;
		*buf_ptr++ = read_data >> 8;
		*buf_ptr++ = read_data >> 16;
		*buf_ptr++ = read_data >> 24;
	}

	if (frame_off) {
		buf_ptr -= (frame_endoff * 4);
		for (i = 0; i < frame_endoff; i++) {
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
		}
	}

	//writel(read_ptr, ETB_BASE + ETB_RAM_READ_POINTER_LK);
	/* Read_pointer has been set as 0x7cb0 in sprd_log_point.c
         * Read_pointer should be set as 0x0 to dump etb.bin for soc_dump function
        */
	writel(0x0, ETB_BASE + ETB_RAM_READ_POINTER_LK);
	CS_LOCK(ETB_BASE);
}

