#ifndef ___SPRD_GPIO_PLUS_H_
#define ___SPRD_GPIO_PLUS_H_


void sprd_gpio_init(void);
void sprd_gpio_free(u32 offset);
int sprd_gpio_request(u32 offset);
void sprd_gpio_set(u32 offset, int value);
int sprd_gpio_get(u32 offset);
int sprd_gpio_direction_input(u32 offset);
int sprd_gpio_direction_output(u32 offset, int value);



#endif


