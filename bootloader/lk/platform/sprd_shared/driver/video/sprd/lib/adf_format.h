/*
 *  <adf_format.h> - <declaration of adf format>
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

#ifndef _VIDEO_ADF_FORMAT_H
#define _VIDEO_ADF_FORMAT_H

#include "logo_bin.h"

bool adf_format_is_standard(u32 format);
bool adf_format_is_rgb(u32 format);
u8 adf_format_num_planes(u32 format);
u8 adf_format_bpp(u32 format);
u8 adf_format_plane_cpp(u32 format, int plane);
u8 adf_format_horz_chroma_subsampling(u32 format);
u8 adf_format_vert_chroma_subsampling(u32 format);

#endif /* _VIDEO_ADF_FORMAT_H */
