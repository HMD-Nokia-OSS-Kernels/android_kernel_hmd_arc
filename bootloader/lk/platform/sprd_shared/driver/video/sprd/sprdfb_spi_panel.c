/*
 *  <sprd_spi_panel.c> - <sprd spi panel>
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

#include "sprdfb.h"
#include "sprdfb_spi_panel.h"

extern struct panel_if_ctrl sprdfb_spi_ctrl;

extern struct panel_spec lcd_gc9305_spi_spec;
//extern struct panel_spec lcd_dummy_spi_spec;

void sprdfb_panel_remove(struct sprdfb_device *dev);

struct panel_spec *spi_panel;

static struct spi_panel_cfg spi_panel_cfg[] = {
#ifdef CONFIG_LCD_GC9305_SPI_QVGA
{
	.lcd_id = 0x9305,
	.panel = &lcd_gc9305_spi_spec,
},
#endif
/*
{
	.lcd_id = 0xFFFF,
	.panel = &lcd_dummy_mipi_spec,
},
*/
};

const char *spi_panel_get_name(void)
{
	if (spi_panel && spi_panel->spi)
		return spi_panel->spi->spi_panel_name;
	else
		return NULL;
}

static void panel_reset(struct sprdfb_device *dev)
{
	//clk/data lane enter LP
	if(NULL != dev->if_ctrl->panel_if_before_panel_reset){
		dev->if_ctrl->panel_if_before_panel_reset(dev);
		mdelay(5);
	}

	//reset panel
	if(NULL != dev->panel->ops->panel_reset){
		dev->panel->ops->panel_reset(dev->panel);
	}
}

static int panel_mount(struct sprdfb_device *dev, struct panel_spec *panel)
{
	uint16_t rval = 1;

	printf("sprdfb: type = %d\n", panel->type);

	dev->if_ctrl = &sprdfb_spi_ctrl;

	if(NULL == dev->if_ctrl){
		return -1;
	}

	if(dev->if_ctrl->panel_if_check){
		rval = dev->if_ctrl->panel_if_check(panel);
	}

	if(0 == rval){
		printf("sprdfb: check panel fail!\n");
		dev->if_ctrl = NULL;
		return -1;
	}

	dev->panel = panel;

/*
	if(NULL == dev->panel->ops->panel_reset){
		dev->panel->ops->panel_reset = panel_reset_dispc;
	}
*/

	dev->if_ctrl->panel_if_mount(dev);

	return 0;
}


int panel_init(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		errorf("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	printf("sprdfb: type = %d\n", dev->panel->type);

	if(NULL != dev->if_ctrl->panel_if_init){
		dev->if_ctrl->panel_if_init(dev);
	}
	return 0;
}

int panel_ready(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		errorf("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	printf("sprdfb:type = %d\n", dev->panel->type);

	if(NULL != dev->if_ctrl->panel_if_ready){
		dev->if_ctrl->panel_if_ready(dev);
	}

	return 0;
}

static struct panel_spec *adapt_panel_from_readid(struct sprdfb_device *dev)
{
	int id, i, ret, b_panel_reset=0;
	uint32_t id_adc;

	for(i = 0; i < (sizeof(spi_panel_cfg)) / (sizeof(spi_panel_cfg[0]));i++) {
		ret = panel_mount(dev, spi_panel_cfg[i].panel);
		if(ret < 0){
			printf("sprdfb: panel_mount failed!\n");
			continue;
		}
		if(dev->ctrl->update_clk)
			dev->ctrl->update_clk(dev);

		panel_init(dev);

		if ((b_panel_reset==0) || (1 == dev->panel->is_need_reset))
		{
			panel_reset(dev);
			b_panel_reset = 1;
		}

		id = dev->panel->ops->panel_readid(dev->panel);
		if(id == spi_panel_cfg[i].lcd_id) {
			printf("sprdfb: LCD Panel 0x%x is attached!\n", spi_panel_cfg[i].lcd_id);

			if(NULL != dev->panel->ops->panel_init){
				dev->panel->ops->panel_init(dev->panel);
			}

			panel_ready(dev);
			return spi_panel_cfg[i].panel;
		} else {
			printf("sprdfb: LCD Panel 0x%x attached fail!go next\n", spi_panel_cfg[i].lcd_id);
			sprdfb_panel_remove(dev);
		}
	}

	printf("sprdfb: final failed to attach LCD Panel!\n");
	return NULL;
}

uint16_t sprdfb_panel_probe(struct sprdfb_device *dev)
{
	//struct panel_spec *spi_panel;

	if(NULL == dev)
		return -1;

	/* can not be here in normal; we should get correct device id from uboot */
	spi_panel = adapt_panel_from_readid(dev);

	if (spi_panel)
		return 0;

	return -1;
}

void sprdfb_panel_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	printf("sprdfb: sprdfb_panel_invalidate_rect\n, (%d, %d, %d,%d)", left, top, right, bottom);

	if(NULL != self->ops->panel_invalidate_rect){
		self->ops->panel_invalidate_rect(self, left, top, right, bottom);
	}
}

void sprdfb_panel_invalidate(struct panel_spec *self)
{
	printf("sprdfb: sprdfb_panel_invalidate\n");

	if(NULL != self->ops->panel_invalidate){
		self->ops->panel_invalidate(self);
	}
}

void sprdfb_panel_before_refresh(struct sprdfb_device *dev)
{
	printf("sprdfb: sprdfb_panel_before_refresh\n");

	if(NULL != dev->if_ctrl->panel_if_before_refresh)
		dev->if_ctrl->panel_if_before_refresh(dev);
}

void sprdfb_panel_after_refresh(struct sprdfb_device *dev)
{
	printf("sprdfb: sprdfb_panel_after_refresh\n");

	if(NULL != dev->if_ctrl->panel_if_after_refresh)
		dev->if_ctrl->panel_if_after_refresh(dev);
}

void sprdfb_panel_remove(struct sprdfb_device *dev)
{
	printf("sprdfb: sprdfb_panel_remove\n");

	if((NULL != dev->if_ctrl) && (NULL != dev->if_ctrl->panel_if_uninit)){
		dev->if_ctrl->panel_if_uninit(dev);
	}
	dev->panel = NULL;
}

