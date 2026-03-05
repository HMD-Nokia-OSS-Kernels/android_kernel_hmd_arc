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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int itoa(int num, unsigned char* str, int len, int base)
{
	int sum = num;
	int i = 0;
	int digit;

	if (len == 0)
		return -1;

	do
	{
		digit = sum % base;
		if (digit < 0xA)
			str[i++] = '0' + digit;
		else
			str[i++] = 'A' + digit - 0xA;

		sum /= base;
	}while (sum && (i < (len - 1)));

	if (i == (len - 1) && sum)
		return -1;

	str[i] = '\0';
	strrev(str);

	return 0;
}
