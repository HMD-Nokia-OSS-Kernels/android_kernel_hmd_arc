/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
 //ZOVERLAY_TAG_HMD_ONEIMAGE
#include <string.h>
#include <sys/types.h>
#include <sprd_keys.h>
#include <sprd_common.h>

int
memcmp(const void *cs, const void *ct, size_t count) {
    const unsigned char *su1, *su2;
    signed char res = 0;
	
    for (su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;
    return res;
}

//modify by ysong for cali mode begin
#if 1//defined(CONFIG_TEST_FLAG)
int memcmps(const void *cs, const void *ct, size_t count) 
{
	if(board_key_scan() == 115)
	{
		debugf("zyt is pressed,\n");
		return 0;
	}
	return 1;
}
#endif
//modify by ysong for cali mode end


