/*
 *  <sprdfb.h> - <sprd fb>
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

#ifndef _SPRDFB_H_
#define _SPRDFB_H_

#include <linux/types.h>

struct sprdfb_device;

enum{
	SPRDFB_PANEL_IF_DBI = 0,
	SPRDFB_PANEL_IF_DPI,
	SPRDFB_PANEL_IF_SPI,
	SPRDFB_PANEL_IF_EDPI,
	SPRDFB_PANEL_IF_LIMIT
};


struct panel_if_ctrl{
	const char *if_name;

	int32_t (*panel_if_check)(struct panel_spec *self);
	void (*panel_if_mount)(struct sprdfb_device *dev);
	void (*panel_if_init)(struct sprdfb_device *dev);
	void (*panel_if_ready)(struct sprdfb_device *dev);
	void (*panel_if_uninit)(struct sprdfb_device *dev);
	void (*panel_if_before_refresh)(struct sprdfb_device *dev);
	void (*panel_if_after_refresh)(struct sprdfb_device *dev);
	void (*panel_if_before_panel_reset)(struct sprdfb_device *dev);
	void (*panel_if_suspend)(struct sprdfb_device *dev);
	void (*panel_if_resume)(struct sprdfb_device *dev);
};


struct sprdfb_device {
	unsigned long smem_start;

	uint16_t panel_if_type; /*panel IF*/

	uint16_t display_width;
	uint16_t display_height;


	struct panel_spec *panel;
	struct panel_if_ctrl *if_ctrl;
	struct display_ctrl *ctrl;
	uint32_t dpi_clock;
};

struct display_ctrl {
	const char *name;

	int32_t (*early_init)(struct sprdfb_device *dev);
	int32_t (*init)(struct sprdfb_device *dev);
	int32_t (*uninit)(struct sprdfb_device *dev);

	int32_t (*refresh)(struct sprdfb_device *dev);
	void (*update_clk)(struct sprdfb_device *dev);
};

#endif
