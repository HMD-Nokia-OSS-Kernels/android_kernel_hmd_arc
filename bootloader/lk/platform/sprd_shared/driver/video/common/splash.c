/*
 *  <splash.c> - <splash routines>
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

#include <splash.h>
#include <lcd.h>
#include <logo_bin.h>
#include <sprd_common.h>
#include <sprd_common_rw.h>

int splash_screen_prepare(char *logo_part_name, u8 *addr)
{
	size_t size ;
	u32 bmp_header_size = 8192;
	uint64_t logo_part_size;

	size = lcd_get_size(&lcd_line_length);
	size += bmp_header_size;

	if (get_img_partition_size(logo_part_name, &logo_part_size)) {
		errorf("failed to get logo partition size\n");
		return -1;
	}

	if (size > logo_part_size) {
		errorf("refuse to read size over max logo partition size\n");
		return -1;
	}

	if (0 != common_raw_read(logo_part_name, (uint64_t)size, (uint64_t)0, addr)) {
		errorf("failed to read logo partition:%s\n", logo_part_name);
		return -1;
	}

	return 0;
}

int lcd_splash(char *logo_part_name)
{
	int x = 0, y = 0, ret;
	u8 *addr;

	addr = (u8 *) bmp_base;
	if (logo_type == LOGO_TYPE_BMP) {
		ret = splash_screen_prepare(logo_part_name, addr);
		if (ret)
			return ret;
	}

	return bmp_display(addr, x, y);
}

int splash_get_bpix(char *logo_part_name)
{
	u8 bpix;
	u8 bmp[128] = {0};
	extern int enter_sysdump_flag;

	if (!enter_sysdump_flag) {
		if (0 != common_raw_read(logo_part_name, (uint64_t)sizeof(bmp), (uint64_t)0, bmp)) {
			pr_info("failed to read logo partition:%s\n", logo_part_name);
			return -1;
		}

		bpix = get_logo_bin_info(bmp, logo_part_name);

		if (bpix == 8 || bpix == 16 || bpix == 24 || bpix == 32) {
			panel_info.vl_bpix = bpix;
			pr_info("get bmp bpix = %d\n", bpix);
		} else {
			panel_info.vl_bpix = 8;
			pr_info("get bmp bpix format error! set default bpix = %d\n", panel_info.vl_bpix);
		}
	} else {
		panel_info.vl_bpix = 8;
		pr_info("enter sysdump mode, no need to display logo\n");
	}

	return 0;
}
