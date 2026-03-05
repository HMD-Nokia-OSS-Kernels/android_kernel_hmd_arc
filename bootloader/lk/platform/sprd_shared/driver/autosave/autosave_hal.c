#include "autosave.h"
#include <asm/arch/sprd_reg.h>

BOOLEAN AS_IsCorePowerDown(unsigned int core_index);
extern int soc_dump_mem_init(void);

#ifdef PLATFORM_QOGIRL6
extern unsigned int g_as_core_offset[9];
extern void autosave_add_subheader(char *subheader, int length);
extern void soc_dump_debugbus(void);
#endif

#ifdef PLATFORM_QOGIRN6PRO
extern void soc_dump_debugbus(unsigned int part_idx);
extern void soc_dump_aon_apb(void);
extern void soc_dump_aon_pmu(void);
extern void soc_dump_analog_g8(void);
extern void soc_dump_analog_g9(void);
extern void soc_dump_analog_g10(void);
extern void soc_dump_top_dvfs(void);
extern void soc_dump_apcpu_dvfs(void);
extern void soc_dump_ddr_ctrl(void);
extern void soc_dump_aon_clk(void);
extern void soc_dump_io_pin(void);
extern void soc_dump_adi_base(void);
#endif

static volatile unsigned int s_wdg_autosave_step = 0;

static void autosave_add_header(void)
{
	char  header[16] = {"SOC_DUMP_LK"};
	int length = sizeof(header);
	unsigned int temp = 0;
	int n;

	rametb_open();
	rametb_seek(ETB_SOC_DUMP_HEADER_START);

	for( n = 0; n < length; n = n+4)
	{
		temp = header[n] | (header[n+1] << 8) | (header[n+2] << 16) | (header[n+3] << 24);
		rametb_write(temp);
	}
}

#if 0
static void autosave_add_subheader(char *subheader, int length)
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
#endif

static void autosave_add_end(void)
{
	char  header[16] = {"SOC_DUMP_END"};
	int length = sizeof(header);
	unsigned int temp = 0;
	int n;

	rametb_seek(ETB_SOC_DUMP_END_START);

	for( n = 0; n < length; n = n+4)
	{
		temp = header[n] | (header[n+1] << 8) | (header[n+2] << 16) | (header[n+3] << 24);
		rametb_write(temp);
	}
}

/*****************************************************************************/
//  Description:This function save auto save reg info when ap send watchdog int
//  Author:
//  Note:add magic 0xACDEADAE,it mean Ap Core DEAD Auto savE
//       we add magic and point becauce maybe hung on read registor
/*****************************************************************************/
static void autosave_save_registor(void)
{
	unsigned int core_index = 0;
	unsigned int tmp_data = 0;
	unsigned int i = 0;
#ifdef PLATFORM_QOGIRL6
	unsigned int *reg_core_offset = g_as_core_offset;
	char  header[32] = {"PART:REG_AUTOSAVE"};
#endif

	//Enable Automatic Register Saving
	CHIP_REG_SET(REG_DBG_APB_CTRL, AUTOSAVE_ENABLE_BIT);
	//AUTOSAVE_DELAY(5); //wait to fix
	//soft trigger
	CHIP_REG_SET(REG_DBG_APB_CTRL, AUTOSAVE_SOFT_TRIG_BIT);

	//AUTOSAVE_DELAY(50);//wait to fix

	//disable Automatic Registor saving
	CHIP_REG_AND(REG_DBG_APB_CTRL, AUTOSAVE_DISABLE_AUTOSAVE_BIT);

#ifdef PLATFORM_QOGIRL6
	rametb_seek(ETB_AUTOSAVE_HEADER_START);
	autosave_add_subheader(header, 32);

	for(core_index = 0;core_index < ARM_EDPCSR_CORE_NUM;core_index++)
	{
		if (AS_IsCorePowerDown(core_index))
		{
			rametb_seek(ETB_AUTOSAVE_START+reg_core_offset[core_index]);
			for (i = 0; i < (reg_core_offset[core_index+1]-reg_core_offset[core_index])/8; i++)
			{
				rametb_write(0);

			}
		}
		else
		{
			//lock access reg
			CHIP_REG_SET(ARM_AUTOSAVE_LOCK,ARM_AUTOSAVE_LOCK_KEY);
			//disable etb
			CHIP_REG_SET(ARM_AUTOSAVE_ENABLE,0);
			//set read addr
			CHIP_REG_SET(ARM_AUTOSAVE_SET_READ, reg_core_offset[core_index]);
			rametb_seek(ETB_AUTOSAVE_START+reg_core_offset[core_index]);

			for (i = 0; i < (reg_core_offset[core_index+1]-reg_core_offset[core_index])/8; i++)
			{
				tmp_data = CHIP_REG_GET(ARM_AUTOSAVE_READ_DATA);
				rametb_write(tmp_data);
			}
		}
	}
#else
	autosave_add_log_pos(PART_SAVE_REGISTER);
	//lock access reg
	CHIP_REG_SET(ARM_AUTOSAVE_LOCK,ARM_AUTOSAVE_LOCK_KEY);
	//disable etb
	CHIP_REG_SET(ARM_AUTOSAVE_ENABLE,0);
	//set read addr
	CHIP_REG_SET(ARM_AUTOSAVE_SET_READ, 0);
	for (i = 0; i < ETB_AUTOSAVE_SIZE; i+=4)
	{
		tmp_data = CHIP_REG_GET(ARM_AUTOSAVE_READ_DATA);
		as_encode_word(tmp_data);
	}

#endif


}

#if 0
/*****************************************************************************/
//  Description:This function unlock arm_os_lock
//  Author:
//  Note:
/*****************************************************************************/
static void arm_os_unlock(unsigned int core_index)
{
	CHIP_REG_SET(ARM_OS_LOCK + core_index * ARM_EDPCSR_OFFSET, 0);
}

/*****************************************************************************/
//  Description:This function save some debug info when ap send watchdog int
//  Author:
//  Note:add magic 0xACDEAD00,it mean Ap Core DEAD
//       we add magic and point becauce maybe hung on read registor
/*****************************************************************************/
static void autosave_save_pc_sample(void)
{
	unsigned int core_index = 0;
	unsigned int val1 = 0;
	unsigned int val2 = 0;
	unsigned int i = 0;
#ifdef PLATFORM_QOGIRL6
	char  header[32] = {"PART:PC_SAMPLE"};

	rametb_seek(ETB_PC_SAMPLE_START);
	autosave_add_subheader(header, 32);
#else
	autosave_add_log_pos(PART_PC_SAMPLE);
#endif

	//store pc sample
	for (i = 0; i < ARM_EDPCSR_SAVE_NUM; i++)
	{
		for(core_index = 0; core_index < ARM_EDPCSR_CORE_NUM; core_index++)
		{
			if (AS_IsCorePowerDown(core_index))
			{
			#ifdef PLATFORM_QOGIRL6
				rametb_write(0xACDEAD07);
				rametb_write(0xACDEAD07);
			#else
				as_encode_word(0xACDEAD07);
				as_encode_word(0xACDEAD07);
			#endif
			}
			else
			{
				arm_os_unlock(core_index);
				val1 = CHIP_REG_GET(ARM_EDPCSR_BASE + core_index * ARM_EDPCSR_OFFSET);
				val2 = CHIP_REG_GET(ARM_EDPCSR_BASE + core_index * ARM_EDPCSR_OFFSET + ARM_EDPCSR_LOHI_OFFSET);
#ifdef PLATFORM_QOGIRL6
				rametb_write(val1);
				rametb_write(val2);
#else
				as_encode_word(val1);
				as_encode_word(val2);
#endif
			}

		}
	}
}

/*****************************************************************************/
//  Description:This function save some debug info when ap send watchdog int
//  Author:
//  Note:
/*****************************************************************************/
static unsigned int  arm_edscr_val(unsigned int i)
{
	unsigned int val = 0;

	val = CHIP_REG_GET(ARM_DEBUG +0x88 + i * ARM_EDPCSR_OFFSET);

	return val;
}

/*****************************************************************************/
//  Description:This function save some debug info when ap send watchdog int
//  Author:
//  Note:add magic 0xACDEAD00,it mean Ap Core DEAD
//       we add magic and point becauce maybe hung on read registor
/*****************************************************************************/
static void autosave_save_core_status(void)
{
	unsigned int i = 0;
	unsigned int edpsr = 0;
	unsigned int edscr = 0;
	unsigned int status= 0;	//0: OFF 1:STOP 2:RUN 3:DEBUG
#ifdef PLATFORM_QOGIRL6
	char  header[32] = {"PART:CORE_STATUS"};

	rametb_seek(ETB_CORE_STATUS_START);
	autosave_add_subheader(header, 32);
#else
	autosave_add_log_pos(PART_CORES_STATUS);
#endif

	for(i = 0; i < ARM_EDPCSR_CORE_NUM; i++)
	{
		status = 0;	//OFF
		if (AS_IsCorePowerDown(i))
		{
#ifdef PLATFORM_QOGIRL6
			rametb_write(0xACDEAD07);
#else
			as_encode_word(0xACDEAD07);
#endif
		}
		else
		{
			edpsr = CHIP_REG_GET(ARM_EDPRSR + i * ARM_EDPCSR_OFFSET);
			if ((edpsr & (1<<0)))
			{
				if ((edpsr & (1<<5)))
					arm_os_unlock(i);
				status = 1;	//STOP
				edscr = arm_edscr_val(i);
				if ((edscr & 0x3F)==0x2)	//non debug
				{
					if (edscr & (1<<25))	//check pipeadv
					{
						CHIP_REG_SET(ARM_DEBUG + 0x90 + i * ARM_EDPCSR_OFFSET, (1<<3));
						//wait 1.s
						edscr = arm_edscr_val(i);
						if (edscr & (1<<25))	//check pipeadv
							status = 2;	//RUN
					}
				}
				else
				{
					status = 3;	//DEBUG
				}
			}
#ifdef PLATFORM_QOGIRL6
			rametb_write(status);
#else
			as_encode_word(status);
#endif
		}
	}
}


#endif

 void wdt_save_dbg_info(void)
{
	unsigned int i = 0;

	if (soc_dump_mem_init())
	{
		//fail
		return;
	}

	s_wdg_autosave_step = 200;    //use for debug,recording code execution location

	//clear etb
	rametb_open();

	rametb_seek(ETB_CLEAR_START);

	for(i = 0; i < ETB_CLEAR_SIZE; i+=4)
	{
		rametb_write(0x0);
	}

	autosave_add_header();
	s_wdg_autosave_step = 201;

#ifdef PLATFORM_QOGIRL6
	soc_dump_debugbus();  //dump debugbus second,4k
#endif
	s_wdg_autosave_step = 204;

	autosave_save_registor();	//dump autosave later, 8K
	s_wdg_autosave_step = 206;

//	autosave_save_core_status();
//	s_wdg_autosave_step = 207;

//	autosave_save_pc_sample();
//	s_wdg_autosave_step = 208;

#ifdef PLATFORM_QOGIRN6PRO
	as_encode_head();
	s_wdg_autosave_step = 202;

	soc_dump_debugbus(PART_DEBUG_BUS_1);  //dump debugbus first,4k
	s_wdg_autosave_step = 203;

	soc_dump_debugbus(PART_DEBUG_BUS_2);  //dump debugbus second,4k
	s_wdg_autosave_step = 204;

	soc_dump_aon_apb();
	s_wdg_autosave_step = 209;

	soc_dump_aon_pmu();
	s_wdg_autosave_step = 210;

	soc_dump_analog_g8();
	s_wdg_autosave_step = 211;

	soc_dump_analog_g9();
	s_wdg_autosave_step = 212;

	soc_dump_analog_g10();
	s_wdg_autosave_step = 213;

	soc_dump_top_dvfs();
	s_wdg_autosave_step = 214;

	soc_dump_apcpu_dvfs();
	s_wdg_autosave_step = 215;

	soc_dump_ddr_ctrl();
	s_wdg_autosave_step = 216;

	soc_dump_aon_clk();
	s_wdg_autosave_step = 217;

	soc_dump_io_pin();
	s_wdg_autosave_step = 218;

	soc_dump_adi_base();
	s_wdg_autosave_step = 219;

	//end encode
	socdump_flush_cache();
	s_wdg_autosave_step = 220;

	autosave_add_encode_info();
	s_wdg_autosave_step = 221;
#endif
	autosave_add_end();
	s_wdg_autosave_step = 999;
}
