/*
 *  <sprd_bl.h> - <sprd bl>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2023-07-11> <pony.wu@unisoc.com>
 *
 *      The above copyright notice shall be
 *      included in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/types.h>
#include <../../../driver/adi/include/adi_hal_internal.h>
#include <../../../include/power/sprd_pmic/sc2730_reg_base.h>
#include <../../../../../top/include/lk/reg.h>
#include <sprd_compat.h>
#include <sprd_common.h>

#define ANA_BLTC_BASE           0x64220180
#define REG_BLTC_R_PRESCL       (ANA_BLTC_BASE + 0x004)
#define REG_BLTC_R_DUTY         (ANA_BLTC_BASE + 0x008)
#define REG_BLTC_R_CURVE0       (ANA_BLTC_BASE + 0x00C)
#define REG_BLTC_R_CURVE1       (ANA_BLTC_BASE + 0x010)
#define REG_BLTC_G_PRESCL       (ANA_BLTC_BASE + 0x014)
#define REG_BLTC_G_DUTY         (ANA_BLTC_BASE + 0x018)
#define REG_BLTC_G_CURVE0       (ANA_BLTC_BASE + 0x01C)
#define REG_BLTC_G_CURVE1       (ANA_BLTC_BASE + 0x020)
#define REG_BLTC_B_PRESCL       (ANA_BLTC_BASE + 0x024)
#define REG_BLTC_B_DUTY         (ANA_BLTC_BASE + 0x028)
#define REG_BLTC_B_CURVE0       (ANA_BLTC_BASE + 0x02C)
#define REG_BLTC_B_CURVE1       (ANA_BLTC_BASE + 0x030)
#define REG_BLTC_STS            (ANA_BLTC_BASE + 0x034)

#define CTL_BASE_ANA_GLB 		0x64221800

#define ANA_REG_GLB_MODULE_EN0  (CTL_BASE_ANA_GLB + 0x0008)
#define ANA_REG_GLB_RTC_CLK_EN0 (CTL_BASE_ANA_GLB + 0x0010)
#define ANA_REG_GLB_RGB_CTRL0    (CTL_BASE_ANA_GLB + 0x0380)

/* ANA_REG_GLB_RGB_CTRL0 */
#define BIT_SLP_RGB_PD_EN    BIT(2)

/* ANA_BLTC_BASE */
#define SC27XX_LED_RUN		BIT(0)
#define SC27XX_LED_TYPE		BIT(1)
#define SC27XX_LED_OUTPUT		BIT(2)
#define SC2730_RGB_PD_SW  		BIT(12)
#define SC2730_RGB_PD_HW  		BIT(13)

#define BITS_RGB_V(x)           (((x) & 0x1F) << 4)
#define BIT_BLTC_EN             BIT(9)
#define BIT_RTC_BLTC_EN         BIT(7)
#define X_CURRENT				0x1F	/*0 ~ 0x1F*/

#define BIT(x) (1<<(x))

static void bltc_write(u32 value, u32 reg)
{
	__raw_writel(value, reg);
	udelay(100);
}

static void bltc_bits_or(u32 value, u32 reg)
{
	__raw_writel((ANA_REG_GET(reg) | value), reg);
	udelay(100);
}

static void bltc_bits_bic(u32 value, u32 reg)
{
	__raw_writel((ANA_REG_GET(reg) & ~value), reg);
	udelay(100);
}

void set_bltc_backlight(uint32_t brightness)
{
	u32 duty = (brightness << 8) | 0xff;

	bltc_bits_bic(BIT_SLP_RGB_PD_EN, ANA_REG_GLB_RGB_CTRL0);
	bltc_bits_bic(SC2730_RGB_PD_SW, ANA_BLTC_BASE);  //Power on
	//bltc_bits_bic(SC2730_RGB_PD_HW, ANA_BLTC_BASE);  //Power on

	/*SET BLTC prescale coefficient, no prescl(0 -> 1rtc_clk)*/
	bltc_write(0x0, REG_BLTC_R_PRESCL);
	bltc_write(0x0, REG_BLTC_G_PRESCL);
	bltc_write(0x0, REG_BLTC_B_PRESCL);

	/*SET BLTC Output RISE/FALL Time*/
	bltc_write(0x0, REG_BLTC_R_CURVE0);
	bltc_write(0x0, REG_BLTC_G_CURVE0);
	bltc_write(0x0, REG_BLTC_B_CURVE0);

	/*SET BLTC Output HIGH/LOW Time*/
	bltc_write(0x0, REG_BLTC_R_CURVE1);
	bltc_write(0x0, REG_BLTC_G_CURVE1);
	bltc_write(0x0, REG_BLTC_B_CURVE1);

	bltc_write((SC27XX_LED_RUN | SC27XX_LED_TYPE) |
				((SC27XX_LED_RUN | SC27XX_LED_TYPE) << 4) |
				((SC27XX_LED_RUN | SC27XX_LED_TYPE) << 8), ANA_BLTC_BASE);  //Red

	/*SET BLTC DUTY(duty_counter + mod_counter)*/
	bltc_write(duty, REG_BLTC_R_DUTY);
	bltc_write(duty, REG_BLTC_G_DUTY);
	bltc_write(duty, REG_BLTC_B_DUTY);

	/*USE MAX CURRENT 85.71mA= {(1.69+0.84*32)*3}*/
	/*AS THE backlight DEFAULT OUTPUT CURRENT*/
	bltc_write(0xf8, REG_BLTC_STS);  //Set Current
	bltc_bits_or(BIT_BLTC_EN, ANA_REG_GLB_MODULE_EN0);
	bltc_bits_or(BIT_RTC_BLTC_EN, ANA_REG_GLB_RTC_CLK_EN0);

	return;
}
