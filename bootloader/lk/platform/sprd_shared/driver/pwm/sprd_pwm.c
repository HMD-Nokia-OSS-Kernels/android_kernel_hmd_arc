#include <asm/arch/pwm/sprd_pwm.h>
#include <stdio.h>
#include <lk/debug.h>

#ifdef SPRD_PWM_VERSION_R3P1
#define PWM_REGS_SHIFT	14
#else
#define PWM_REGS_SHIFT 5
#endif

#define PWM_PRESCALE	(0x0000)
#define PWM_MOD		(0x0004)
#define PWM_DUTY	(0x0008)
#define PWM_DIV		(0x000c)
#define PWM_PAT_LOW	(0x0010)
#define PWM_PAT_HIGH	(0x0014)
#define PWM_ENABLE	(0x0018)
#define PWM_VERSION	(0x001c)

#define PWM_REG_MSK	0xffff

static inline uint32_t pwm_read(int index, uint32_t reg)
{
	return readl(sprd_pwm[index].pwm_base + (index << PWM_REGS_SHIFT) + reg);
}

static void pwm_write(int index, uint32_t value, uint32_t reg)
{
	writel(value, sprd_pwm[index].pwm_base + (index << PWM_REGS_SHIFT) + reg);
}

int pwm_config(int pwm_id,int duty,int period)
{
	int index = pwm_id;

	writel(EXT_26M, sprd_pwm[index].pwm_clk_base);//ext_26m select
	writel(readl(sprd_pwm[index].apb_base_eb)|(sprd_pwm[index].pwm_eb), sprd_pwm[index].apb_base_eb);//eb

	if (0 == duty) {
		pwm_write(index, 0, PWM_ENABLE);
		//dev_info(priv->udev, "pwm sprd backlight power off. pwm_index=%d  duty=%d\n", index, duty);
	} else {
		pwm_write(index, period, PWM_MOD);
		pwm_write(index, duty, PWM_DUTY);
		pwm_write(index, PWM_REG_MSK, PWM_PAT_LOW);
		pwm_write(index, PWM_REG_MSK, PWM_PAT_HIGH);
		pwm_write(index, 1, PWM_ENABLE);
		//dev_info(priv->udev, "pwm sprd backlight power on. pwm_index=%d  brightness=%d\n", index, duty);
	}

	return 0;
}

