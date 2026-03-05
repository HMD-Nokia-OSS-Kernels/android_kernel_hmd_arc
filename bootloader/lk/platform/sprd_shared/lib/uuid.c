#include <sprd_common.h>
#include <errno.h>
#include <part_efi.h>
#include <malloc.h>
#include <stdio.h>
#include <linux/byteorder/little_endian.h>
#include <linux/byteorder/generic.h>
#include <uuid.h>
#include <string.h>
#include <ctype.h>

int is_uuid_str_validate(const char *uuid)
{
	int i, valid;

	if (uuid == NULL)
		return 0;

	for (i = 0, valid = 1; uuid[i] && valid; i++) {
		switch (i) {
		case 8: case 13: case 18: case 23:
			valid = (uuid[i] == '-');
			break;
		default:
			valid = isxdigit(uuid[i]);
			break;
		}
	}

	if (i != UUID_STR_LEN || !valid)
		return 0;

	return 1;
}

/*
 * uuid_str_to_bin() - convert to big endian binary data from string UUID or GUID.
 *
 * uuid_str - pointer to UUID or GUID string [37B]
 * uuid_bin - pointer big endian binary data array [16B]
 */
int uuid_str_to_bin(char *target_str, unsigned char *target_bin, int str_format)
{
	uint16_t tmp_uint16;
	uint32_t tmp_uint32;
	uint64_t tmp_uint64;

	if (!is_uuid_str_validate(target_str))
		return -EINVAL;

	if (str_format == UUID_STR_FORMAT_STD) {
		tmp_uint32 = cpu_to_be32(simple_strtoul(target_str, NULL, 16));
		memcpy(target_bin, &tmp_uint32, 4);

		tmp_uint16 = cpu_to_be16(simple_strtoul(target_str + 9, NULL, 16));
		memcpy(target_bin + 4, &tmp_uint16, 2);

		tmp_uint16 = cpu_to_be16(simple_strtoul(target_str + 14, NULL, 16));
		memcpy(target_bin + 6, &tmp_uint16, 2);
	} else {
		tmp_uint32 = cpu_to_le32(simple_strtoul(target_str, NULL, 16));
		memcpy(target_bin, &tmp_uint32, 4);

		tmp_uint16 = cpu_to_le16(simple_strtoul(target_str + 9, NULL, 16));
		memcpy(target_bin + 4, &tmp_uint16, 2);

		tmp_uint16 = cpu_to_le16(simple_strtoul(target_str + 14, NULL, 16));
		memcpy(target_bin + 6, &tmp_uint16, 2);
	}

	tmp_uint16 = cpu_to_be16(simple_strtoul(target_str + 19, NULL, 16));
	memcpy(target_bin + 8, &tmp_uint16, 2);

	tmp_uint64 = cpu_to_be64(simple_strtoull(target_str + 24, NULL, 16));
	memcpy(target_bin + 10, (char *)&tmp_uint64 + 2, 6);

	return 0;
}

/*
 * uuid_bin_to_str() - convert to string UUID or GUID from big endian binary data.
 *
 * target_bin - pointer to UUID (binary data with big endian) [16B]
 * target_str - pointer to output string array [37B]
 * str_format - gpt always use UUID_STR_FORMAT_GUID
 */
void uuid_bin_to_str(unsigned char *target_bin, char *target_str, int str_format)
{
	int i;
	const u8 *target_char;
	const u8 guid_target_char[UUID_BIN_LEN] = {3, 2, 1, 0, 5, 4, 7, 6, 8,
						  9, 10, 11, 12, 13, 14, 15};
	/*
	 * UUID and GUID bin data - always in big endian.Just for UUID_STR_FORMAT_GUID
	 */
	target_char = guid_target_char;

	for (i = 0; i < 16; i++) {
		sprintf(target_str, "%02x", target_bin[target_char[i]]);
		target_str += 2;
		switch (i) {
		case 3:
		case 5:
		case 7:
		case 9:
			*target_str++ = '-';
			break;
		}
	}
}
