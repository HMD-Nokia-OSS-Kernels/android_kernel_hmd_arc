#ifndef _OEM_FASTBOOTCMD_H_
#define _OEM_FASTBOOTCMD_H_

enum MODE_FASTBOOT {
    MODE_FASTBOOT_BASIC = 0,
    MODE_FASTBOOT_FLASH,
    MODE_FASTBOOT_REPAIR,
    MODE_FASTBOOT_SIMLOCK,
    MODE_FASTBOOT_FACTORY,
    MODE_FASTBOOT_NUM,
};

enum rsa_encrypt_mode{
	RSA_ENCRYPT_HMD = 0,
	RSA_ENCRYPT_FACTORY,
	RSA_ENCRYPT_ALL,
};

#define AUTH_DATA_LEN 344
#define SW_PUB_KEY_LEN 392
#define UPLOADE_SIZE 512

extern char upload_buf[UPLOADE_SIZE];
extern unsigned int upload_len;
extern int fastboot_mode_flag;
extern char powp_buf[MISCDATA_POWP_DATA_LEN];
extern char cali_buf[MISCDATA_POWP_DATA_LEN]; //modify by ysong for cali fastboot begin
extern int POWP_flag;
extern int powp_size;
extern int auth_flag;

struct mode_key_table {
	char *mode;
	int mode_flag;
};

struct mode_cmd_check_table {
	char **cmd_disallow;
	int  cmd_disallows;
};

int fastboot_mode_cmd_filter(const char *cmd_prefix, const unsigned char *buff);
int cmd_fastboot_oem_handle(const char *subcmd, const char *arg, void *data, uint64_t size);
void oem_fastboot_register_commands(void);


#endif /* !_OEM_FASTBOOTCMD_H_ */
//ZOVERLAY_TAG_HMD_ONEIMAGE


