/*
 *  <sprd_main.c> - <sprd main>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2021-08-09> <pony.wu@unisoc.com>
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

#include <sprd_common.h>
#include <malloc.h>
#include <logo_bin.h>
#include <lcd.h>
#include <splash.h>
#include <bmp_layout.h>
#include <ctype.h>
#include <sprd_common_rw.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "sprd_dispc.h"
#include "sprd_panel.h"
#include <decompress_data.h>
#ifdef CONFIG_SPI_SLAVER_PANEL
#include "sprdfb_spi.h"
#endif
#ifdef CONFIG_ROUND_CORNER_SUPPORT
#include "sprd_round_corner.h"
#endif

typedef unsigned long long fdt_addr_t;
typedef unsigned long long fdt_size_t;

extern const char* g_env_bootmode;
extern int gunzip(void *, int, unsigned char *, unsigned long *);/* external/gzip/gunzip.c */

int logo_index = 0;
unsigned long bmp_size;
void *bmp_base;
int logo_type;
int panel_enabled = 0;

void *fb_base;

void *lcd_get_base_addr(void *lcd_base)
{
	return fb_base;
}

static void logo_flip(void)
{
	struct sprd_dispc *dispc = &dispc_device;
	struct sprd_restruct_config *config;
	struct panel_info *panel = panel_info_attach();
	uint8_t logo_bpp = panel_info.vl_bpix > 16 ? 4 : 2;

	config = malloc(sizeof(struct sprd_restruct_config) +
			sizeof(struct sprd_adf_hwlayer));
	if (config == NULL)
		return;

	config->number_hwlayer = 1;
	config->hwlayers[0].hwlayer_id = 0;
	config->hwlayers[0].iova_plane[0] = fb_base;
	config->hwlayers[0].n_planes = 1;
	config->hwlayers[0].alpha = 0xFF;
	config->hwlayers[0].pitch[0] = panel->width * logo_bpp;
	config->hwlayers[0].dst_w = panel->width;
	config->hwlayers[0].dst_h = panel->height;
	config->hwlayers[0].dst_x = 0;
	config->hwlayers[0].dst_y = 0;
	config->hwlayers[0].start_x = 0;
	config->hwlayers[0].start_y = 0;
	config->hwlayers[0].start_w = 0;
	config->hwlayers[0].start_h = 0;
	config->hwlayers[0].blending = HWC_BLENDING_NONE;
	config->hwlayers[0].compression = 0;
	config->hwlayers[0].format = logo_bpp == 2 ?
		SPRD_DRM_FORMAT_RGB565 : SPRD_DRM_FORMAT_ARGB8888;

	sprd_dispc_flip(dispc, config);

	free(config);
}

int get_fb_base_from_dt(void)
{
	unsigned long base, offset;
	struct panel_info *panel = panel_info_attach();

	base = LOGO_RESERVED_ADDR;

	offset = panel->width * panel->height * 4 * 2;
	fb_base = base;

	return 0;
}

static int sprdfb_probe(void)
{
	sprd_panel_probe();
	sprd_dispc_probe();

#ifdef CONFIG_SPI_SLAVER_PANEL
	sprdfb_spi_probe();
#endif

	return 0;
}

void lcd_disable(void)
{
	pr_info("lcd disable\n");
}

void lcd_enable(void)
{
	pr_info("start flip\n");
	logo_flip();

#ifdef CONFIG_SPI_SLAVER_PANEL
	spi_lcd_enable();
#endif
}

void lcd_ctrl_init(void *lcdbase)
{
	int ret;

	sprdfb_probe();

	ret = logo_mem_init();
	if (ret) {
		errorf("logo memory init failed.\n");
		return;
	}

	dprintf(INFO,"logo memory init success.\n");
	lcd_set_flush_dcache(1);
}

int logo_mem_init(void)
{
	u32 bmp_header_size = 8192;
	void *temp_logo_addr;

	get_fb_base_from_dt();
	bmp_size = panel_info.vl_col * panel_info.vl_row * panel_info.vl_bpix / 8 + bmp_header_size;
	temp_logo_addr = (void *)BMP_RESERVED_ADDR;
	bmp_size = LOGO_BUFFER_SIZE;

	if (!temp_logo_addr) {
		pr_err("failed to alloc bmp space\n");
		return -1;
	}
	bmp_base = temp_logo_addr;
	pr_info("splashimage addr is 0x%p, fb size is 0x%x\n", (void *)temp_logo_addr, bmp_size);

	return 0;
}

void logo_display(int index, int backlight_value, int lcd_on)
{
	extern uint32_t lk_start_time;
	extern int enter_sysdump_flag;
	uint32_t lcd_init_time;
	uint32_t backlight_on_time;
	uint32_t lk_consume_time;

	if (!lcd_on) {
		dprintf(INFO,"lcd off, no need to enable display\n");
		return;
	}

	panel_enabled = 1;
	logo_index = index;
	lcd_init_time = SCI_GetTickCount();
	dprintf(CRITICAL,"lcd start init time:%dms\n", lcd_init_time);
	panel_enabled = 1;
	extern void lcd_enable(void);
	drv_lcd_init();
	if (!enter_sysdump_flag)
		lcd_splash(LOGO_PART);
	lcd_enable();

	set_backlight(backlight_value);
	backlight_on_time = SCI_GetTickCount();
	lcd_init_time= backlight_on_time - lcd_init_time;
	lk_consume_time = backlight_on_time - lk_start_time;
	dprintf(CRITICAL,"lk consume time:%dms, lcd init consume:%dms, backlight on time:%dms \n", \
		lk_consume_time, lcd_init_time, backlight_on_time);
}

int get_logo_bin_info(u8 *bmp, char *logo_part_name)
{
	u8 bpix;
	u32 in_size = 0;
	u32 header_size = 128;
	unsigned long inout_size;
	int ret, i, count;
	u8 *s;
	u8 *bmp_addr;
	u8 *gz_addr;
	u32 offset = 0;
	struct header_info *header;
	uint64_t logo_part_size;

	struct bmp_image *temp_bmp = (struct bmp_image*)bmp;
	if ((temp_bmp->header.signature[0] == 'B') && (temp_bmp->header.signature[1] == 'M')) {
		bpix = bmp_get_bpix(bmp);
		logo_type = LOGO_TYPE_BMP;
		dprintf(INFO,"logo type: bmp file .\n");
	}  else if ((temp_bmp->header.signature[0] == 'G') && (temp_bmp->header.signature[1] == 'Z')){
		header = (struct header_info*)bmp;
		count = header->file_number;
		header_size = count * sizeof(uint32_t) + sizeof(struct header_info);
		if (0 != common_raw_read(logo_part_name, (uint64_t)header_size, (uint64_t)0, (u8 *)header)) {
			debug("failed to read logo partition:%s\n", logo_part_name);
			return -1;
		}

#ifdef SPRD_BOOTLOADER_LOGO_TYPE
	struct panel_info *panel = panel_info_attach();

	if ((720 <= panel->width) && (panel->width < 1080)) {
		logo_index = logo_index + 4;
	}
#endif
		in_size = header->gz_size[logo_index];
		gz_addr = malloc(in_size);
		if (!gz_addr) {
			errorf("gz_addr is NULL\n");
			return -1;
		}
		offset += header_size;
		for (i = 0; i < logo_index; i++) {
			offset += header->gz_size[i];
		}

		if (get_img_partition_size(logo_part_name, &logo_part_size)) {
			errorf("failed to get logo partition size\n");
			return -1;
		}

		if ((in_size + offset) > logo_part_size) {
			debug("refuse to read size over max logo partition size\n");
			return -1;
		}

		bmp_addr = (void *)BMP_RESERVED_ADDR;
		if (0 != common_raw_read(logo_part_name, (uint64_t)in_size, (uint64_t)offset, gz_addr)) {
			debug("failed to read logo partition:%s\n", logo_part_name);
			return -1;
		}

		inout_size = in_size;
		ret = gunzip((void *)bmp_addr, bmp_size, (void *)gz_addr, &inout_size);
		free(gz_addr);
		if (ret != 0) {
			debugf("unzip failed %d.\n", ret);
			return -1;
		}

		temp_bmp = (struct bmp_image*)bmp_addr;
		bpix = temp_bmp->header.bit_count;
		dprintf(INFO,"temp_bmp->header.bit_count :%d.\n", bpix);
		logo_type = LOGO_TYPE_BIN;
		debug("logo type: bin file .\n");
	} else {
		debugf("invalid logo pic type, failed to support\n");
		return -1;
	}

	return bpix;
}
