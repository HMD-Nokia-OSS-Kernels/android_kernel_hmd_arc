/*
 *  <sprdfb_spi_api.c> - <sprd spi api>
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
#include "sprdfb_spi.h"
#include "sprdfb_spi_panel.h"

extern int sprdfb_panel_probe(struct sprdfb_device *dev);
extern void sprdfb_panel_remove(struct sprdfb_device *dev);

extern struct display_ctrl sprdfb_swdispc_ctrl;

static struct sprdfb_device s_sprdfb_dev = {0};

bool is_lcd_enable = false;

static int spi_real_refresh(struct sprdfb_device *dev)
{
	int32_t ret;

	if(NULL == dev->panel){
		printf("sprdfb: fail (no panel!)\n");
		return -1;
	}

	ret = dev->ctrl->refresh(dev);
	if (ret) {
		printf("sprdfb: failed to refresh!\n");
		return -1;
	}

	return 0;
}

int sprdfb_spi_probe(void)
{
	struct sprdfb_device *dev = &s_sprdfb_dev;

	dev->ctrl = &sprdfb_swdispc_ctrl;

	if(dev->ctrl->early_init)
		dev->ctrl->early_init(dev);

	if (0 != sprdfb_panel_probe(dev)) {
		sprdfb_panel_remove(dev);
		if(dev->ctrl->uninit)
			dev->ctrl->uninit(dev);
		printf("sprdfb: failed to probe\n");
		return -EFAULT;
	}

	dev->display_width = dev->panel->width;
	dev->display_height = dev->panel->height;

	if(dev->ctrl->init)
		dev->ctrl->init(dev);

	return 0;
}

void spi_lcd_disable(void)
{
	printf("sprdfb: spi_lcd_disable\n");
	sprdfb_panel_remove(&s_sprdfb_dev);
	if(s_sprdfb_dev.ctrl->uninit)
		s_sprdfb_dev.ctrl->uninit(&s_sprdfb_dev);
}

void spi_lcd_refresh(void *base)
{
	if(!is_lcd_enable)
		return ;
	sprdfb_spi_flip(base);
}

void spi_lcd_enable(void)
{
	spi_real_refresh(&s_sprdfb_dev);
	is_lcd_enable = true;
}

extern void *fb_base;

int32_t sprdfb_spi_refresh(struct sprdfb_device *dev)
{
	if(!dev->panel)
	{
		printf("sprdfb:sprdfb_refresh dev->panel is null\n");
		return -1;
	}
	dev->panel->ops->panel_invalidate(dev->panel);
	dev->panel->ops->panel_refresh(dev->panel, fb_base);
	return 0;
}

int32_t sprdfb_spi_flip(void *base)
{
        struct sprdfb_device * dev = &s_sprdfb_dev;
        if(!dev->panel)
        {
                printf("sprdfb:sprdfb_refresh dev->panel is null\n");
                return -1;
        }
        dev->panel->ops->panel_invalidate(dev->panel);
        dev->panel->ops->panel_refresh(dev->panel,base);
        return 0;
}

