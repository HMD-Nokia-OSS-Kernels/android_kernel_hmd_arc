/*
 *  <splash.h> - <declaration of splash>
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

#ifndef _SPLASH_H_
#define _SPLASH_H_

#include <errno.h>
#include <logo_bin.h>

enum splash_storage {
	SPLASH_STORAGE_NAND,
	SPLASH_STORAGE_SF,
};

struct splash_location {
	char *name;
	enum splash_storage storage;
	u32 offset;	/* offset from start of storage */
};

int splash_source_load(struct splash_location *locations, uint size);
int splash_screen_prepare(char *logo_part_name, u8 *addr);


int lcd_splash(char *logo_part_name);
int splash_get_bpix(char *logo_part_name);


#define BMP_ALIGN_CENTER	0x7FFF

#endif
