#include <stdbool.h>
#include "autosave.h"
#include <asm/arch/sprd_reg.h>

/*****************************************************************************/
//  Description: is core power down
//  Author:
//  Note: return true power_down; false power_up
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

/*****************************************************************************/
//  Description: init mem
//  Author: porter.xu
/*****************************************************************************/
 int soc_dump_mem_init(void)
{
	return 0;
}

/* refer to qogirn6pro_debugbus_code.c from xiaopeng.bai */
static void write_debug_bus_sigsel(uint32_t sysselreg_val, unsigned int modreg_val, unsigned int sigsel_start, unsigned int sigsel_end, unsigned int sigreg_addr , unsigned int offset)
{
	 unsigned int i = 0;
	unsigned int db_data = 0;
	unsigned int j = 0;

	*(uint32_t *)(0x7800A100) = sysselreg_val;  //syssel_reg:0x7800A100
	*(uint32_t *)(0x7800A10C) = modreg_val;  //mod_reg:0x7800A10C

	if(sigsel_start <= sigsel_end ) {
		for(i = sigsel_start; i <= sigsel_end; i++)
		{
			*(uint32_t *)(sigreg_addr) = (i << offset);

			//delay and read twice to keep value stable
			for (j = 0; j < 20; j++);
			db_data = *(uint32_t *)(0x7800A308); //data_reg:0x7800A308
			db_data = *(uint32_t *)(0x7800A308); //data_reg:0x7800A308
			as_encode_word(db_data);
		}
	}
	else {
		for(i = sigsel_start; i >= sigsel_end; i--)
		{
			*(uint32_t *)(sigreg_addr) = (i << offset);

			//delay and read twice to keep value stable
			for (j = 0; j < 20; j++);
			db_data = *(uint32_t *)(0x7800A308); //data_reg:0x7800A308
			db_data = *(uint32_t *)(0x7800A308); //data_reg:0x7800A308
			as_encode_word(db_data);
		}
	}
}

static void write_debug_bus_modsel(unsigned int sysselreg_val, unsigned int sigreg_val, unsigned int modsel_start, unsigned int modsel_end, unsigned int modreg_addr , unsigned int offset)
{
	 unsigned int i = 0;
	unsigned int db_data = 0;
	unsigned int j = 0;

	*(uint32_t *)(0x7800A100) = sysselreg_val;  //syssel_reg:0x7800A100
	*(uint32_t *)(0x7800A110) = sigreg_val;  //sig_reg:0x7800A110

	for(i = modsel_start; i <= modsel_end; i++)
	{
		*(uint32_t *)(modreg_addr) = (i << offset);

		//delay and read twice to keep value stable
		for (j = 0; j < 20; j++);
		db_data = *(uint32_t *)(0x7800A308); //data_reg:0x7800A308
		db_data = *(uint32_t *)(0x7800A308); //data_reg:0x7800A308
		as_encode_word(db_data);
	}
}

static void write_align(unsigned int count )
{
	unsigned int i;

	for(i = 0; i <= count; i++)
	{
		as_encode_word(0);
	}
}

/*****************************************************************************/
//  Description: soc_dump_debugbus
//  Author: porter.xu
/*****************************************************************************/
 void soc_dump_debugbus(unsigned int part_idx)
{
	autosave_add_log_pos(part_idx);

	/* read ai debugbus */
	write_debug_bus_sigsel(0xA, 0x0, 0x0, 0x18, 0x7800A110, 0);

	/* read aon debugbus */
	write_debug_bus_sigsel(0x6, 0x1, 0x1, 0x3A, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x2, 0x1, 0x9, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x3, 0x1, 0xD, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x4, 0x0, 0x1F, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x5, 0x0, 0x8, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x6, 0x0, 0x8, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x7, 0x0, 0x8, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x8, 0x0, 0xD8, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0x9, 0x0, 0xD7, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0xA, 0x1, 0x3E, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0xB, 0x1, 0x3A, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0xC, 0x1, 0x79, 0x7800A110, 0);
	write_debug_bus_sigsel(0x6, 0xD, 0x1, 0xA, 0x7800A110, 0);
	write_debug_bus_modsel(0x6, 0x0, 0xE, 0xF, 0x7800A10C, 0);

	/* read ap debugbus */
	write_debug_bus_sigsel(0x0, 0x0, 0x1, 0x2D, 0x7800A110, 0);

	/* read apcpu debugbus */
	write_debug_bus_modsel(0x7, 0x0, 0x0, 0x29, 0x7800A10C, 0); //apcpu_dbg_bus40
	write_debug_bus_modsel(0x7, 0x10, 0x29, 0x29, 0x7800A10C, 0); //apcpu_dbg_bus40
	write_debug_bus_modsel(0x7, 0x0, 0x2A, 0x2E, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x80, 0x0, 0xD7, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x84, 0x0, 0xD7, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x88, 0x0, 0xD7, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x8C, 0x0, 0xD7, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x90, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x91, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x92, 0x0, 0x9F, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x94, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x95, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x96, 0x0, 0x9F, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x98, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x99, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x9A, 0x0, 0x9F, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x9C, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x9D, 0x0, 0xFF, 0x7800A10C, 0);
	write_debug_bus_modsel(0x7, 0x9E, 0x0, 0x9F, 0x7800A10C, 0);

	/* read audio debugbus */
	write_debug_bus_sigsel(0x3, 0x0, 0x0, 0x47, 0x7800A110, 0);

	/* read camera debugbus */
	write_debug_bus_modsel(0xB, 0x0, 0x1, 0x5B, 0x7800A10C, 0);

	/* read ch debugbus  */
	write_debug_bus_sigsel(0xE, 0x0, 0x1, 0x6, 0x7800A110, 0);

	/* read dpu debugbus */
	write_debug_bus_sigsel(0xC, 0x0, 0x1, 0x26, 0x7800A110, 0);

	/* read gpu debugbus */
	write_debug_bus_modsel(0x5, 0x0, 0x1, 0x13, 0x7800A10C, 0);

	/* read ipa debugbus */
	write_debug_bus_sigsel(0x8, 0x0, 0x1, 0x3F, 0x7800A110, 0);

	/* read pcie debugbus */
	write_debug_bus_modsel(0x9, 0x0, 0x1, 0x1B, 0x7800A10C, 0);

	/* read phycp debugbus */
	write_debug_bus_sigsel(0x2, 0x0, 0x1, 0xF4, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x1, 0x1, 0x22, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x2, 0x1, 0x35, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x3, 0x1, 0x89, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x4, 0x1, 0x5F, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x5, 0x1, 0x52, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x6, 0x1, 0x2D, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x7, 0x0, 0x4, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x8, 0x0, 0x0, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x9, 0x1, 0x8A, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0x9, 0xE0, 0xE0, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x0, 0x0, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0xD, 0x1, 0x7800A110, 0);  //sigsel--
	write_debug_bus_sigsel(0x2, 0xA, 0x12, 0x1C, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x20, 0x33, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x3C, 0x3C, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x3E, 0x3E, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x3D, 0x3D, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x38, 0x38, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x3B, 0x3B, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x3A, 0x3A, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x39, 0x39, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x34, 0x34, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x37, 0x35, 0x7800A110, 0);  //sigsel--
	write_debug_bus_sigsel(0x2, 0xA, 0x48, 0x41, 0x7800A110, 0);  //sigsel--
	write_debug_bus_sigsel(0x2, 0xA, 0x50, 0x54, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x60, 0x60, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0x73, 0x61, 0x7800A110, 0);  //sigsel--
	write_debug_bus_sigsel(0x2, 0xA, 0x80, 0x80, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xA, 0xA0, 0xA0, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xB, 0x0, 0xD, 0x7800A110, 0);
	write_debug_bus_sigsel(0x2, 0xC, 0x0, 0x0, 0x7800A110, 0);

	/* read pub debugbus */
	write_debug_bus_sigsel(0x4, 0x1, 0x1, 0x7F, 0x7800A110, 0);
	write_debug_bus_sigsel(0x4, 0x2, 0x1, 0x3E, 0x7800A110, 0);
	write_debug_bus_sigsel(0x4, 0x2, 0x6D, 0x76, 0x7800A110, 0);
	write_debug_bus_sigsel(0x4, 0x3, 0x1, 0x6, 0x7800A110, 0);
	write_debug_bus_sigsel(0x4, 0x4, 0x1, 0x6, 0x7800A110, 0);

	/* read sp debugbus */
	write_debug_bus_sigsel(0xD, 0x0, 0x1, 0x6, 0x7800A110, 0);

	/* read pscp debugbus */
	write_debug_bus_modsel(0x1, 0x0, 0x1, 0x46, 0x7800A10C, 0);
	write_debug_bus_sigsel(0x1, 0x47, 0x1, 0x30, 0x7800A110, 0);
	write_debug_bus_modsel(0x1, 0x0, 0x48, 0x7E, 0x7800A10C, 0);

	//for 8 words alignment
	write_align(4);
}

/*****************************************************************************/
//  Description: soc_dump_aon_apb
//  Author: porter.xu
/*****************************************************************************/
 void soc_dump_aon_apb(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_AON_APB);

	for(i = 0; i < ETB_AONAPB_SIZE; i+=4)
	{
		data = *(uint32_t *)(0x64900000+i);   //0x64900000
		//rametb_write(data);
		as_encode_word(data);
	}
}

/*****************************************************************************/
//  Description: soc_dump_aon_pmu
//  Author: porter.xu
/*****************************************************************************/
 void soc_dump_aon_pmu(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_AON_PMU);

	if ((*(uint32_t *)(0x64900000+0x8)) & (0x1<<8))	//check PMU_TOP_EB in REG_AON_APB_APB_EB2 from aon_apb.h
	{
		for(i = 0; i < ETB_AONPMU_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x64910000+i);   //0x64910000
			//rametb_write(data);
			as_encode_word(data);
		}
	}
}

//add for xunrui
 void soc_dump_analog_g8(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_ANALOG_G8);

	if ((*(uint32_t *)(0x64900000+0x4)) & (0x1<<12))	//ANA EB
	{
		for(i = 0; i < ETB_ANALOG_G8_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x6432c000+i);   //0x6432c000
			as_encode_word(data);
		}
	}
}

 void soc_dump_analog_g9(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_ANALOG_G9);

	if ((*(uint32_t *)(0x64900000+0x4)) & (0x1<<12))	//ANA EB
	{
		for(i = 0; i < ETB_ANALOG_G9_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x64330000+i);   //0x64330000
			as_encode_word(data);
		}
	}
}

 void soc_dump_analog_g10(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_ANALOG_G10);

	if ((*(uint32_t *)(0x64900000+0x4)) & (0x1<<12))	//ANA EB
	{
		for(i = 0; i < ETB_ANALOG_G10_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x64334000+i);   //0x64334000
			as_encode_word(data);
		}
	}
}

 void soc_dump_top_dvfs(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_TOP_DVFS);

	if ((*(uint32_t *)(0x64900000+0x4)) & (0x1<<7))	//top dvfs EB
	{
		for(i = 0; i < ETB_TOP_DVFS_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x64940000+i);   //0x64940000
			as_encode_word(data);
		}
	}
}

 void soc_dump_apcpu_dvfs(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_APCPU_DVFS);

	if ((*(uint32_t *)(0x64900000+0x4)) & (0x1<<28))	//apcpu dvfs EB
	{
		for(i = 0; i < ETB_APCPU_DVFS_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x64950000+i);   //0x64950000
			as_encode_word(data);
		}
	}
}


/*****************************************************************************/
//  Description: soc_dump_ddr_ctrl
//  Author: porter.xu
/*****************************************************************************/

 void soc_dump_ddr_ctrl(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_DDR_CTRL);

	//base
	for(i = 0; i < ETB_DDR_BASE_SIZE; i+=4)
	{
		data = *(uint32_t *)(0x60000000+i); //0x60000000
		as_encode_word(data);
	}
	//phy0
	for(i = 0; i < ETB_DDR_PHY0_SIZE; i+=4)
	{
		data = *(uint32_t *)(0x60001000+i); //0x60001000
		as_encode_word(data);
	}
	//phy1
	for(i = 0; i < ETB_DDR_PHY1_SIZE; i+=4)
	{
		data = *(uint32_t *)(0x60002000+i);  //0x60002000
		as_encode_word(data);
	}

}
/*****************************************************************************/
//  Description: soc_dump_aon_clk
//  Author: porter.xu
/*****************************************************************************/
 void soc_dump_aon_clk(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_AON_CLK);

	if ((*(uint32_t *)(0x64900000+0x4)) & (0x1<<13))	//check AON_CLK_TOP_EB in REG_AON_APB_APB_EB1 from aon_apb.h
	{
		for(i = 0; i < ETB_AONCLK_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x64920000+i);  //0x64920000
			as_encode_word(data);
		}
	}

}

/*****************************************************************************/
//  Description: soc_dump_io_pin
//  Author: porter.xu
/*****************************************************************************/
 void soc_dump_io_pin(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_IO_PIN);

	if ((*(uint32_t *)(0x64900000+0x4)) & (0x1<<11))	//check PIN_REG_EB in REG_AON_APB_APB_EB1 from aon_apb.h
	{
		for(i = 0; i < ETB_IOPIN_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x642e0000+i);   //0x642e0000
			as_encode_word(data);
		}
	}
}

/*****************************************************************************/
//  Description: soc_dump_adi_base
//  Author: porter.xu
/*****************************************************************************/
 void soc_dump_adi_base(void)
{
	unsigned int i = 0;
	unsigned int data = 0;

	autosave_add_log_pos(PART_ADI_REGS);

	if ((*(uint32_t *)(0x64900000+0x8)) & (0x1<<9))	//check ADI_EB in REG_AON_APB_APB_EB2 from aon_apb.h
	{
		for(i = 0; i < ETB_ADI_BASE_SIZE; i+=4)
		{
			data = *(uint32_t *)(0x64400000+i);   //0x64400000
			as_encode_word(data);
		}
	}
}

