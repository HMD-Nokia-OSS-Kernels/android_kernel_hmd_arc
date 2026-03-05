/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

void *sec_memset(void *s, int c, unsigned int cnt)
{
	char *xs = s;

	while (cnt--)
		*xs++ = c;
	return s;
}

void *sec_memcpy(void *dest, const void *src, unsigned int count)
{
	char *tmp = dest;
	const char *s = src;

	while (count--)
		*tmp++ = *s++;
	return dest;
}

int sec_memcmp(const void *cs, const void *st, unsigned int count)
{
	const unsigned char *su1, *su2;
	int res = 0;

	for (su1 = cs, su2 = st; 0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}
