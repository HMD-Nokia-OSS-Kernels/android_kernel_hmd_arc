#ifndef __DRIVERS_EIC_H__
#define __DRIVERS_EIC_H__

void sprd_eic_init(void);
void sprd_eic_free(unsigned offset);
int sprd_eic_request(unsigned offset);
int sprd_eic_get(unsigned offset);

#ifdef CONFIG_SPRD_EIC_IRQ
void EIC_Enable(u32 eic_num);
void EIC_SetIntSense(u32 eic_num,int sense_type);
typedef void (* EIC_CALLBACK) (u32 eic_num, u32 eic_state);
u32 EICA_AddCallbackToIntTable(u32 eic_num,EIC_CALLBACK eic_callback_fun);
#endif

#endif
