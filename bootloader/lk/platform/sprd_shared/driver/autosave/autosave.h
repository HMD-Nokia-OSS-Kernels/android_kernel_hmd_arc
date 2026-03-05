#ifndef _AUTOSAVE_H_
#define _AUTOSAVE_H_

#include <asm/arch/sprd_reg.h>

#define TRUE   1   /* Boolean true value. */
#define FALSE  0   /* Boolean false value. */
typedef unsigned char	BOOLEAN;

/**---------------------------------------------------------------------------*
 **                             Compiler Flag                                 *
 **--------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C"
{
#endif

#define ENCODE_INFO_START		0x10
#define LOG_POS_START			0x20

typedef struct {
	unsigned int part_idx;
	char *part_name;
}HEADER_INFO_T;

typedef enum {
	PART_DEBUG_BUS_1 = 0,
	PART_DEBUG_BUS_2,
	PART_SAVE_REGISTER,
	PART_CORES_STATUS,
	PART_PC_SAMPLE,
	PART_AON_APB,
	PART_AON_PMU,

	PART_ANALOG_G8,
	PART_ANALOG_G9,
	PART_ANALOG_G10,
	PART_TOP_DVFS,
	PART_APCPU_DVFS,

	PART_DDR_CTRL,
	PART_AON_CLK,
	PART_AON_CLK_LPW,
	PART_IO_PIN,
	PART_ADI_REGS,
}AS_PART_E;

void rametb_open(void);
void rametb_close(void);
void rametb_seek(unsigned int pos);
void rametb_write(unsigned int data);
void socdump_flush_cache(void);
void autosave_add_encode_info(void);
void autosave_add_log_pos(unsigned int part_idx);
void as_encode_word(unsigned int word);
void as_encode_head(void);
//void wdt_save_dbg_info(void);



/**----------------------------------------------------------------------------*
**                         Compiler Flag                                      **
**----------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
#endif
