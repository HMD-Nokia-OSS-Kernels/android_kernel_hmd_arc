#include "autosave.h"

#define CACHE_SIZE			(32)

unsigned int g_src_offset = 0;
unsigned int g_decode_size = 0;

/* cache */
unsigned char g_cache[CACHE_SIZE] = {0};
int g_cache_offset = 0;
int g_cache_bit_offset = 0;

int g_encode_pos = 0x200;
int g_flush_pos = 0x200;

int g_etb_is_full = 0;
int g_etb_max_size = (32*1024);		//etb size

char g_encode_head[CACHE_SIZE];

HEADER_INFO_T s_header_list[] = {
	{PART_DEBUG_BUS_1, "DEBUG_BUS_1"},
	{PART_DEBUG_BUS_2, "DEBUG_BUS_2"},
	{PART_SAVE_REGISTER, "SAVE_REGS"},
	{PART_CORES_STATUS, "CORES_STATUS"},
	{PART_PC_SAMPLE, "PC_SAMPLE"},
	{PART_AON_APB, "AON_APB"},
	{PART_AON_PMU, "AON_PMU"},

	{PART_ANALOG_G8, "ANALOG_G8"},
	{PART_ANALOG_G9, "ANALOG_G9"},
	{PART_ANALOG_G10, "ANALOG_G10"},
	{PART_TOP_DVFS, "TOP_DVFS"},
	{PART_APCPU_DVFS, "APCPU_DVFS"},

	{PART_DDR_CTRL, "DDR_CTRL"},
	{PART_AON_CLK, "AON_CLK"},
	{PART_IO_PIN, "IO_PIN"},
	{PART_ADI_REGS, "ADI_REGS"}
};

void rametb_open(void)
{
	//lock access reg
	CHIP_REG_SET(SOC_ETB_BASE+CS_LOCK_OFFSET,CS_LOCK_KEY);

	//disable etb
	CHIP_REG_SET(SOC_ETB_BASE+ETB_ENABLE,0);
}

void rametb_close(void)
{
}

void rametb_seek(unsigned int pos)
{
	//set RWP
	CHIP_REG_SET(SOC_ETB_BASE+ETB_RWP,pos);
}

void rametb_write(unsigned int data)
{
	//set RWD
	CHIP_REG_SET(SOC_ETB_BASE+ETB_RWD,data);
}

void socdump_flush_cache(void)
{
	unsigned int temp = 0;
	int n = 0;

	if ((g_flush_pos+CACHE_SIZE)>=g_etb_max_size)
	{
		g_etb_is_full = 1;
		return;
	}

	rametb_seek(g_flush_pos);
	for( n = 0; n < CACHE_SIZE; n = n+4)
	{
		temp = g_cache[n] | (g_cache[n+1] << 8) | (g_cache[n+2] << 16) | (g_cache[n+3] << 24);
		rametb_write(temp);
	}
	g_flush_pos += CACHE_SIZE;

	memset(g_cache, 0x0, CACHE_SIZE);
	g_cache_offset = 0;
}

/*****************************************************************************/
//  Description:    The function is used to encode byte
//  Global resource dependence:
//  Author: Bin.Ji
//  Note:
/*****************************************************************************/
void as_encode_byte(unsigned char ch)
{
	unsigned char ch_low;
	unsigned char ch_high;
	int bit_num = 0;

	g_src_offset++;
	bit_num = (7-g_cache_bit_offset);
	if (ch != 0x0)
	{
		ch_low = ch & ((1 << bit_num)-1);
		ch_high = ch >> bit_num;
		g_cache[g_cache_offset] |= (((ch_low<<1)|1)<<g_cache_bit_offset);
		g_cache_offset++;
		g_encode_pos++;
		if (g_cache_offset == CACHE_SIZE)
			socdump_flush_cache();
		if(g_cache_offset >= CACHE_SIZE)
			return;
		g_cache[g_cache_offset] |= ch_high;
		if (g_cache_bit_offset==7)
		{
			g_cache_offset++;
			g_encode_pos++;
			g_cache_bit_offset = 0;
			if (g_cache_offset == CACHE_SIZE)
				socdump_flush_cache();
		}
		else
		{
			g_cache_bit_offset++;
		}

	}
	else
	{
		if (g_cache_bit_offset==7)
		{
			if (g_cache_offset == (CACHE_SIZE-1))
				socdump_flush_cache();
			else
			{
				g_cache_offset++;
				g_encode_pos++;
			}
			g_cache_bit_offset = 0;
		}
		else
		{
			g_cache_bit_offset++;
		}
	}
}

/*****************************************************************************/
//  Description:    The function is used to encode word
//  Global resource dependence:
//  Author: Bin.Ji
//  Note:
/*****************************************************************************/
void as_encode_word(unsigned int word)
{
	unsigned char ch;
	int i;

//	if (g_ch_index == 0x270)
//		g_ch_index = g_ch_index;
	for (i = 0; i < 4; i++)
	{
		ch = (word >> (8*i)) & 0xFF;
		as_encode_byte(ch);
	}
}

/*****************************************************************************/
//  Description:    The function is used to add encode head
//  Global resource dependence:
//  Author: Bin.Ji
//  Note:
/*****************************************************************************/
void as_encode_head(void)
{
	int i;

	memset(g_encode_head, 0x0, CACHE_SIZE);
	strcpy(g_encode_head, "ENCODE_START:");

	for (i = 0; i < CACHE_SIZE; i++)
	{
		as_encode_byte(g_encode_head[i]);
	}
}


void autosave_add_log_pos(unsigned int part_idx)
{
	char text[12]={0};
	unsigned length;
	unsigned int n;
	unsigned int temp;
	unsigned int i;

	for (i = 0; i < sizeof(s_header_list)/sizeof(s_header_list[0]); i++)
	{
		if (part_idx == s_header_list[i].part_idx)
			break;
	}
	if (i == sizeof(s_header_list)/sizeof(s_header_list[0])) //not find
		return;

	length = strlen(s_header_list[i].part_name);
	length = (length > 12)?12:length;
	memcpy(text, s_header_list[i].part_name, length);

	rametb_seek(LOG_POS_START+i*0x10);

	//12byte log + 4byte offset
	for( n = 0; n < 12; n = n+4)
	{
		temp = text[n] | (text[n+1] << 8) | (text[n+2] << 16) | (text[n+3] << 24);
		rametb_write(temp);
	}

	rametb_write(g_src_offset);
}

void autosave_add_encode_info(void)
{
	char  encode_info[16] = {0};
	int length = sizeof(encode_info);
	int n;
	unsigned int temp = 0;

	memset(encode_info, 0x0, 16);

	rametb_open();
	rametb_seek(ENCODE_INFO_START);

	encode_info[0] = (g_src_offset>>0)&0xFF;
	encode_info[1] = (g_src_offset>>8)&0xFF;
	encode_info[2] = (g_src_offset>>16)&0xFF;
	encode_info[3] = (g_src_offset>>24)&0xFF;

	encode_info[4] = (g_encode_pos>>0)&0xFF;		//or g_flush_pos
	encode_info[5] = (g_encode_pos>>8)&0xFF;
	encode_info[6] = (g_encode_pos>>16)&0xFF;
	encode_info[7] = (g_encode_pos>>24)&0xFF;

	for( n = 0; n < length; n = n+4)
	{
		temp = encode_info[n] | (encode_info[n+1] << 8) | (encode_info[n+2] << 16) | (encode_info[n+3] << 24);
		rametb_write(temp);
	}
}
