#include <stdbool.h>
#include "autosave.h"
#include <asm/arch/sprd_reg.h>

unsigned int g_as_core_offset[9] = {0, 0x200, 0x400, 0x600, 0x800, 0xA00, 0xC00, 0x1200, 0x1800};

/*****************************************************************************/
//  Description: is core power down
//  Author:
//  Note:
/*****************************************************************************/
BOOLEAN AS_IsCorePowerDown(unsigned int core_index)
{
	unsigned int apcpu_pwr_state = 0;
	unsigned int core_pwr_status = 0;

	if(core_index < 2)
	{
		apcpu_pwr_state = CHIP_REG_GET(APCU_PWR_STATE0);
		core_pwr_status = (apcpu_pwr_state >> ((core_index+2) * 8)) & 0xff;
	}
	else if(core_index < 6)
	{
		apcpu_pwr_state = CHIP_REG_GET(APCU_PWR_STATE1);
		core_pwr_status = (apcpu_pwr_state >> ((core_index-2) * 8)) & 0xff;
	}
	else
	{
		apcpu_pwr_state = CHIP_REG_GET(APCU_PWR_STATE2);
		core_pwr_status = (apcpu_pwr_state >> ((core_index-6) * 8)) & 0xff;
	}
	if (0 != core_pwr_status)
		return TRUE;
	return FALSE;
}

static void write_debug_bus(unsigned int index, unsigned int modsel_start, unsigned int modsel_end, unsigned int addr , unsigned int offset)
{
	unsigned int i = 0;
	unsigned int db_data = 0;

	*(uint32_t *)(0x7C00A018) = index;

	for(i = modsel_start; i <= modsel_end; i++)
	{
		*(uint32_t *)(addr) = (i << offset);
		db_data = *(uint32_t *)(0x7C00A050);
		rametb_write(db_data);
	}
}

static void write_debug_bus_b(unsigned int index, unsigned int modsel_start, unsigned int modsel_end, unsigned int addr , unsigned int add_offset)
{
	unsigned int i = 0;
	unsigned int db_data = 0;

	*(uint32_t *)(0x7C00A018) = index;

	for(i = modsel_start; i <= modsel_end; i++)
	{
		*(uint32_t *)(addr) = (add_offset << 8) | i;
		db_data = *(uint32_t *)(0x7C00A050);
		rametb_write(db_data);
	}
}

static void write_align(unsigned int count )
{
	unsigned int i;

	for(i = 0; i <= count; i++)
	{
		rametb_write(0);
	}
}

void autosave_add_subheader(char *subheader, int length)
{
//	char  header[32] = {"cm4_dump_autosave_V1.0"};
	char  *header = subheader;
	unsigned int temp = 0;
	int n;

	for( n = 0; n < length; n = n+4)
	{
		temp = header[n] | (header[n+1] << 8) | (header[n+2] << 16) | (header[n+3] << 24);
		rametb_write(temp);
	}
}

/*****************************************************************************/
//  Description: init mem
//  Author: Bin.Ji
/*****************************************************************************/
int soc_dump_mem_init(void)
{
	return 0;
}

/*****************************************************************************/
//  Description: soc_dump_debugbus
//  Author: Bin.Ji
/*****************************************************************************/
void soc_dump_debugbus(void)
{
	char  header[32] = {"PART:DEBUG_BUS"};

	rametb_seek(ETB_DB_START);
	autosave_add_subheader(header, 32);

	/* read aon debugbus */
	write_debug_bus(0xA, 0, 8, 0X7C00A020, 16);
	write_debug_bus(0xA, 16, 24, 0X7C00A020, 16);
	write_debug_bus(0xA, 32, 40, 0X7C00A020, 16);

	/* read ap debugbus */
	write_debug_bus(0x0, 1, 56, 0X7C00A01C, 8);

	/* read apcpu debugbus */
	write_debug_bus(0x9, 0, 34, 0X7C00A028, 16);

	/* read audcp debugbus */
	write_debug_bus(0x3, 0, 43, 0X7C00A024, 0);

	/* read gpu debugbus */
	write_debug_bus(0x7, 1, 6, 0X7C00A01C, 24);
	write_debug_bus(0x7, 8, 11, 0X7C00A01C, 24);
	write_debug_bus(0x7, 15, 15, 0X7C00A01C, 24);

	/* read aon_lp debugbus */
	*(uint32_t *)(0X7C00A028) = 1 << 24;
	write_debug_bus(0x6, 0, 76, 0X7C00A020, 8);

	*(uint32_t *)(0X7C00A028) = 2 << 24;
	write_debug_bus(0x6, 0, 132, 0X7C00A020, 8);

	*(uint32_t *)(0X7C00A028) = 0 << 24;
	write_debug_bus(0x6, 0, 147, 0X7C00A020, 8);

	*(uint32_t *)(0X7C00A028) = 3 << 24;
	write_debug_bus(0x6, 0, 157, 0X7C00A020, 8);

	 /* read mm debugbus */
	write_debug_bus(0x5, 1, 50, 0X7C00A01C, 16);

	 /* read pub debugbus */
	write_debug_bus(0x4, 1, 96, 0X7C00A020, 0);
	write_debug_bus(0x4, 128, 190, 0X7C00A020, 0);
	write_debug_bus(0x4, 237, 248, 0X7C00A020, 0);
	write_debug_bus(0x4, 64, 70, 0X7C00A020, 0);
	write_debug_bus(0x4, 80, 86, 0X7C00A020, 0);

	/* read pubcp debugbus */
	write_debug_bus(0x1, 0, 65, 0X7C00A024, 8);

	 /* read wcn debugbus */
	write_debug_bus_b(0xB, 1, 148, 0X7C00A074, 0);
	write_debug_bus_b(0xB, 0, 0, 0X7C00A074, 4);
	write_debug_bus_b(0xB, 1, 34, 0X7C00A074, 1);
	write_debug_bus_b(0xB, 0, 21, 0X7C00A074, 3);
	write_debug_bus_b(0xB, 1, 41, 0X7C00A074, 2);

	/* read wtlcp debugbus */
	write_debug_bus(0x2, 0, 143, 0X7C00A028, 8);
	write_debug_bus(0x2, 193, 203, 0X7C00A028, 8);
	write_debug_bus(0x2, 240, 243, 0X7C00A028, 8);

	//for 8 words alignment
	write_align(4);
}
