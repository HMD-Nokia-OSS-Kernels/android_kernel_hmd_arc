#ifndef __UUID_H__
#define __UUID_H__

#define UUID_STR_LEN		36
#define UUID_BIN_LEN		sizeof(struct uuid)
enum {
	UUID_STR_FORMAT_STD,	/* 0 */
	UUID_STR_FORMAT_GUID	/* 1 */
};

/*
 * This is structure is in big-endian
 * Layout of UUID:
 * timestamp - 60-bit: time_low, time_mid, time_hi_and_version
 * version   - 4 bit (bit 4 through 7 of the time_hi_and_version)
 * clock seq - 14 bit: clock_seq_hi_and_reserved, clock_seq_low
 * variant:  - bit 6 and 7 of clock_seq_hi_and_reserved
 * node      - 48 bit
 */
struct uuid {
	unsigned int time_low;
	unsigned short time_mid;
	unsigned short time_hi_and_version;
	unsigned char clock_seq_hi_and_reserved;/* clock seq high */
	unsigned char clock_seq_low;/* clock seq low */
	unsigned char node[6];
} __packed;

int is_uuid_str_validate(const char *uuid);
int uuid_str_to_bin(char *uuid_str, unsigned char *uuid_bin, int str_format);
void uuid_bin_to_str(unsigned char *uuid_bin, char *uuid_str, int str_format);
#endif
