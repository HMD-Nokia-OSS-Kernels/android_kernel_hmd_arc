/*
 *  <cmd_bmp.c> - <BMP handling routines>
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
#include <lcd.h>
#include <logo_bin.h>
#include <bmp_layout.h>
#include <malloc.h>
#include <splash.h>
#include <asm/byteorder.h>

/*
 * Allocate and decompress a BMP image using gunzip().
 *
 * Returns a pointer to the decompressed image data. This pointer is
 * aligned to 32-bit-aligned-address + 2.
 * See doc/README.displaying-bmps for explanation.
 *
 * The allocation address is passed to 'alloc_addr' and must be freed
 * by the caller after use.
 *
 * Returns NULL if decompression failed, or if the decompressed data
 * didn't contain a valid BMP signature.
 */

int bmp_display(ulong addr, int x, int y)
{
	unsigned long len;
	void *bmp_alloc_addr = NULL;
	struct bmp_image *bmp = (struct bmp_image *)addr;
	int ret;

	if (!bmp) {
		pr_info("There is no valid bmp file at the given address\n");
		return 1;
	}

	ret = lcd_display_bitmap((ulong)bmp, x, y);

	if (bmp_alloc_addr)
		free(bmp_alloc_addr);

	return ret;
}

static int bmp_info(ulong addr)
{
	unsigned long len;
	void *bmp_alloc_addr = NULL;
	struct bmp_image *bmp = (struct bmp_image *)addr;

	if (bmp == NULL) {
		pr_info("There is no valid bmp file at the given address\n");
		return 1;
	}

	pr_info("Image size    : %d x %d\n", le32_to_cpu(bmp->header.width),
	       le32_to_cpu(bmp->header.height));
	pr_info("Bits per pixel: %d\n", le16_to_cpu(bmp->header.bit_count));
	pr_info("Compression   : %d\n", le32_to_cpu(bmp->header.compression));

	if (bmp_alloc_addr)
		free(bmp_alloc_addr);

	return(0);
}

int bmp_get_bpix(ulong addr)
{
	struct bmp_image *bmp = (struct bmp_image*)addr;
	return bmp->header.bit_count;
}
