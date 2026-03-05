#ifndef ___SPRD_GPIO_H_
#define ___SPRD_GPIO_H_

void sprd_gpio_init(void);
int sprd_gpio_direction_output(unsigned offset, int value);
int sprd_gpio_direction_input(unsigned offset);
int sprd_gpio_get(unsigned offset);
void sprd_gpio_set(unsigned offset, int value);
int sprd_gpio_request(unsigned offset);
void sprd_gpio_free(unsigned offset);

#ifdef CONFIG_SPRD_GPIO_IRQ
void GPIO_Enable (unsigned int gpio_id);
void GPIO_SetDirection (unsigned int  gpio_id, int directions);
void GPIO_SetInterruptsSense (unsigned int gpio_id, int sensetype);
typedef void (* GPIO_CALLBACK) (u32 gpio_id, u32 gpio_state);
PUBLIC u32 GPIO_AddCallbackToIntTable(u32 gpio_id, GPIO_CALLBACK gpio_callback_fun);
#endif

#endif


