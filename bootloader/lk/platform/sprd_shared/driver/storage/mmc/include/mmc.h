/*
*  mmc.h  - unisoc mmc config
*
*  Copyright (C) 2019 Unisoc Communications Inc.
*  History:
*      2021-08-17 wenchao.chen@unisoc.com
*      Add mmc.h
*/
#ifndef _MMC_H_
#define _MMC_H_
#include <lk/list.h>
#include <part.h>

/* SD/MMC version bits; 8 flags, 8 major, 8 minor, 8 change */
#define SD_CARD_VERSION	(1U << 31)
#define MMC_CARD_VERSION (1U << 30)

#define SD_VERSION_SD SD_CARD_VERSION
#define MMC_VERSION_MMC MMC_CARD_VERSION

#define GET_SDMMC_VERSION(a, b, c)	\
	((((u32)(a)) << 16) | ((u32)(b) << 8) | (u32)(c))
#define GET_SD_VERSION(a, b, c)	\
	(SD_CARD_VERSION | GET_SDMMC_VERSION(a, b, c))
#define GET_MMC_VERSION(a, b, c)	\
	(MMC_CARD_VERSION | GET_SDMMC_VERSION(a, b, c))

#define SD_VERSION_3		GET_SD_VERSION(3, 0, 0)
#define SD_VERSION_2		GET_SD_VERSION(2, 0, 0)
#define SD_VERSION_1_0		GET_SD_VERSION(1, 0, 0)
#define SD_VERSION_1_10		GET_SD_VERSION(1, 10, 0)

#define MMC_VERSION_UNKNOWN	GET_MMC_VERSION(0, 0, 0)
#define MMC_VERSION_1_2		GET_MMC_VERSION(1, 2, 0)
#define MMC_VERSION_1_4		GET_MMC_VERSION(1, 4, 0)
#define MMC_VERSION_2_2		GET_MMC_VERSION(2, 2, 0)
#define MMC_VERSION_3		GET_MMC_VERSION(3, 0, 0)
#define MMC_VERSION_4		GET_MMC_VERSION(4, 0, 0)
#define MMC_VERSION_4_1		GET_MMC_VERSION(4, 1, 0)
#define MMC_VERSION_4_2		GET_MMC_VERSION(4, 2, 0)
#define MMC_VERSION_4_3		GET_MMC_VERSION(4, 3, 0)
#define MMC_VERSION_4_41	GET_MMC_VERSION(4, 4, 1)
#define MMC_VERSION_4_5		GET_MMC_VERSION(4, 5, 0)
#define MMC_VERSION_5_0		GET_MMC_VERSION(5, 0, 0)
#define MMC_VERSION_5_1		GET_MMC_VERSION(5, 1, 0)

#define MMC_CAP(mode)		(1 << mode)
#define MMC_MODE_LEGACY		MMC_CAP(MMC_LEGACY)
#define MMC_MODE_HS		(MMC_CAP(MMC_HS) | MMC_CAP(SD_HS))
#define MMC_MODE_HS_52MHz	MMC_CAP(MMC_HS_52)
#define MMC_MODE_DDR_52MHz	MMC_CAP(MMC_DDR_52)
#define MMC_MODE_HS200		MMC_CAP(MMC_HS_200)
#define MMC_MODE_HS400		MMC_CAP(MMC_HS_400)
#define MMC_MODE_HS401		MMC_CAP(MMC_HS_400_ES)
#define SD_MODE_SDR12		MMC_CAP(UHS_SDR12)
#define SD_MODE_SDR25		MMC_CAP(UHS_SDR25)
#define SD_MODE_SDR50		MMC_CAP(UHS_SDR50)
#define SD_MODE_SDR104		MMC_CAP(UHS_SDR104)
#define MODE_MASK		(MMC_MODE_LEGACY | MMC_MODE_HS | MMC_MODE_HS_52MHz | \
				MMC_MODE_DDR_52MHz | MMC_MODE_HS200 | MMC_MODE_HS400 \
				| MMC_MODE_HS401 | SD_MODE_SDR12 | SD_MODE_SDR25 | \
				SD_MODE_SDR50 | SD_MODE_SDR104)

#define MMC_MODE_8BIT		MMC_CAP(30)
#define MMC_MODE_4BIT		MMC_CAP(29)
#define MMC_MODE_1BIT		MMC_CAP(28)
#define MMC_MODE_SPI		MMC_CAP(27)

#define SD_DATA_4BIT		(0x00040000)

#define IS_SD(x)	((x)->version & SD_CARD_VERSION)
#define IS_MMC(x)	((x)->version & MMC_CARD_VERSION)

#define MMC_DATA_READ		(1)
#define MMC_DATA_WRITE		(2)

/* ERROR */
#define UNUSABLE_ERR		(-17)
#define COMM_ERR		(-18)
#define TIMEOUT			(-19)
#define SWITCH_ERR		(-20)

/* MMC CMD */
#define MMC_CMD_GO_IDLE_STATE		(0)
#define MMC_CMD_SEND_OP_COND		(1)
#define MMC_CMD_ALL_SEND_CID		(2)
#define MMC_CMD_SET_RELATIVE_ADDR	(3)
#define MMC_CMD_SET_DSR			(4)
#define MMC_CMD_SWITCH			(6)
#define MMC_CMD_SELECT_CARD		(7)
#define MMC_CMD_SEND_EXT_CSD		(8)
#define MMC_CMD_SEND_CSD		(9)
#define MMC_CMD_SEND_CID		(10)
#define MMC_CMD_STOP_TRANSMISSION	(12)
#define MMC_CMD_SEND_STATUS		(13)
#define MMC_CMD_SET_BLOCKLEN		(16)
#define MMC_CMD_READ_SINGLE_BLOCK	(17)
#define MMC_CMD_READ_MULTIPLE_BLOCK	(18)
#define MMC_CMD_SEND_TUNING_BLOCK	(19)
#define MMC_CMD_SEND_TUNING_BLOCK_HS200	(21)
#define MMC_CMD_SET_BLOCK_COUNT         (23)
#define MMC_CMD_WRITE_SINGLE_BLOCK	(24)
#define MMC_CMD_WRITE_MULTIPLE_BLOCK	(25)
#define MMC_CMD_SET_WR_PROT		(28)
#define MMC_CMD_SEND_WR_PROT		(30)
#define MMC_CMD_SEND_WR_PROT_TYPE	(31)
#define MMC_CMD_ERASE_GROUP_START	(35)
#define MMC_CMD_ERASE_GROUP_END		(36)
#define MMC_CMD_ERASE			(38)
#define MMC_CMD_APP_CMD			(55)
#define MMC_CMD_SPI_READ_OCR		(58)
#define MMC_CMD_SPI_CRC_ON_OFF		(59)
#define MMC_CMD_RES_MAN			(62)
#define MMC_CMD62_ARG1			(0xefac62ec)
#define MMC_CMD62_ARG2			(0xcbaea7)

/* SD CMD */
#define SD_CMD_SEND_RELATIVE_ADDR	(3)
#define SD_CMD_SWITCH_FUNC		(6)
#define SD_CMD_SEND_IF_COND		(8)
#define SD_CMD_SWITCH_UHS18V		(11)
#define SD_CMD_APP_SET_BUS_WIDTH	(6)
#define SD_CMD_APP_SD_STATUS		(13)
#define SD_CMD_ERASE_WR_BLK_START	(32)
#define SD_CMD_ERASE_WR_BLK_END		(33)
#define SD_CMD_APP_SEND_OP_COND		(41)
#define SD_CMD_APP_SEND_SCR		(51)

/* SCR */
#define SD_HIGHSPEED_BUSY	(0x00020000)
#define SD_HIGHSPEED_SUPPORTED	(0x00020000)

#define UHS_SDR12_BUS_SPEED	0
#define HIGH_SPEED_BUS_SPEED	1
#define UHS_SDR25_BUS_SPEED	1
#define UHS_SDR50_BUS_SPEED	2
#define UHS_SDR104_BUS_SPEED	3

#define SD_MODE_UHS_SDR12	(1 << UHS_SDR12_BUS_SPEED)
#define SD_MODE_UHS_SDR25	(1 << UHS_SDR25_BUS_SPEED)
#define SD_MODE_UHS_SDR50	(1 << UHS_SDR50_BUS_SPEED)
#define SD_MODE_UHS_SDR104	(1 << UHS_SDR104_BUS_SPEED)

#define OCR_BUSY		(0x80000000)
#define OCR_HCS			(0x40000000)
#define OCR_S18R		(0x1000000)
#define OCR_VOLTAGE_MASK	(0x007FFF80)
#define OCR_ACCESS_MODE		(0x60000000)

#define MMC_STATUS_MASK		(~0x0206BF7F)
#define MMC_STATUS_SWITCH_ERROR	(1 << 7)
#define MMC_STATUS_RDY_FOR_DATA (1 << 8)
#define MMC_STATUS_CURR_STATE	(0xf << 9)
#define MMC_STATUS_ERROR	(1 << 19)

#define MMC_STATE_PRG		(7 << 9)

#define MMC_VDD_165_195		(0x00000080)	/* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_29_30		(0x00020000)	/* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31		(0x00040000)	/* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32		(0x00080000)	/* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33		(0x00100000)	/* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34		(0x00200000)	/* VDD voltage 3.3 ~ 3.4 */

#define MMC_SWITCH_MODE_CMD_SET		(0x00)
#define MMC_SWITCH_MODE_SET_BITS	(0x01)
#define MMC_SWITCH_MODE_CLEAR_BYTE	(0x02)
#define MMC_SWITCH_MODE_WRITE_BYTE	(0x03) /* Set target byte to value */

#define SD_SWITCH_CHECK		(0)
#define SD_SWITCH_SWITCH	(1)

/* EXT_CSD */
#define EXT_CSD_ENH_START_ADDR		(136)	/* R/W */
#define EXT_CSD_ENH_SIZE_MULT		(140)	/* R/W */
#define EXT_CSD_GP_SIZE_MULT		(143)	/* R/W */
#define EXT_CSD_PARTITION_SETTING	(155)	/* R/W */
#define EXT_CSD_PARTITIONS_ATTRIBUTE	(156)	/* R/W */
#define EXT_CSD_MAX_ENH_SIZE_MULT	(157)	/* R */
#define EXT_CSD_PARTITIONING_SUPPORT	(160)	/* RO */
#define EXT_CSD_RST_N_FUNCTION		(162)	/* R/W */
#define EXT_CSD_WR_REL_PARAM		(166)	/* R */
#define EXT_CSD_WR_REL_SET		(167)	/* R/W */
#define EXT_CSD_RPMB_MULT		(168)	/* RO */
#define EXT_CSD_USER_WP			(171)	/* R/W */
#define EXT_CSD_ERASE_GROUP_DEF		(175)	/* R/W */
#define EXT_CSD_BOOT_BUS_WIDTH		(177)
#define EXT_CSD_PART_CONF		(179)	/* R/W */
#define EXT_CSD_ERASE_MEM_CONT		(181)	/* R */
#define EXT_CSD_BUS_WIDTH		(183)	/* R/W */
#define EXT_CSD_STROBE_SUPPORT	(184)	/* R/W */
#define EXT_CSD_HS_TIMING		(185)	/* R/W */
#define EXT_CSD_REV			(192)	/* RO */
#define EXT_CSD_CARD_TYPE		(196)	/* RO */
#define EXT_CSD_SEC_CNT			(212)	/* RO, 4 bytes */
#define EXT_CSD_HC_WP_GRP_SIZE		(221)	/* RO */
#define EXT_CSD_REL_WR_SEC_C              (222)	/* RO */
#define EXT_CSD_HC_ERASE_GRP_SIZE	(224)	/* RO */
#define EXT_CSD_BOOT_MULT		(226)	/* RO */

/* EXT_CSD */
#define EXT_CSD_CMD_SET_NORMAL		(1 << 0)

#define MMC_EXT_CSD_BUS_WIDTH_1	0	/* Card is in 1 bit mode */
#define MMC_EXT_CSD_BUS_WIDTH_4	1	/* Card is in 4 bit mode */
#define MMC_EXT_CSD_BUS_WIDTH_8	2	/* Card is in 8 bit mode */
#define MMC_EXT_CSD_DDR_BUS_WIDTH_4	5	/* Card is in 4 bit DDR mode */
#define MMC_EXT_CSD_DDR_BUS_WIDTH_8	6	/* Card is in 8 bit DDR mode */
#define EXT_CSD_DDR_FLAG	(1 << 2)	/* Flag for DDR mode */
#define EXT_CSD_BUS_WIDTH_STROBE (1 << 7)	/* Enhanced strobe mode */

#define EXT_CSD_TIMING_LEGACY	0	/* no high speed */
#define EXT_CSD_TIMING_HS		1	/* HS */
#define EXT_CSD_TIMING_HS200	2	/* HS200 */
#define EXT_CSD_TIMING_HS400	3	/* HS400 */
#define EXT_CSD_DRV_STR_SHIFT	4	/* Driver Strength shift */

#define MMC_EXT_CSD_CARD_TYPE_26	(1 << 0)	/* Card can run at 26MHz */
#define MMC_EXT_CSD_CARD_TYPE_52	(1 << 1)	/* Card can run at 52MHz */
#define MMC_EXT_CSD_CARD_TYPE_DDR_1_8V	(1 << 2)
#define MMC_EXT_CSD_CARD_TYPE_DDR_1_2V	(1 << 3)
#define MMC_EXT_CSD_CARD_TYPE_HS200_1_8V	(1 << 4)	/* Card can run at 200MHz */
													/* SDR mode @1.8V I/O */
#define MMC_EXT_CSD_CARD_TYPE_HS200_1_2V	(1 << 5)	/* Card can run at 200MHz */
													/* SDR mode @1.2V I/O */
#define MMC_EXT_CSD_CARD_TYPE_HS200		(MMC_EXT_CSD_CARD_TYPE_HS200_1_8V | \
									 MMC_EXT_CSD_CARD_TYPE_HS200_1_2V)
#define MMC_EXT_CSD_CARD_TYPE_HS400_1_8V	(1 << 6)
#define MMC_EXT_CSD_CARD_TYPE_HS400_1_2V	(1 << 7)
#define MMC_EXT_CSD_CARD_TYPE_HS400		(MMC_EXT_CSD_CARD_TYPE_HS400_1_8V | \
									 MMC_EXT_CSD_CARD_TYPE_HS400_1_2V)

#define EXT_CSD_BOOT_PARTITION_ENABLE		(1 << 3)

#define EXT_CSD_US_PWR_WP_EN	(1 << 0) /* power-on write protection */
#define EXT_CSD_US_PWR_WP_DIS	(1 << 3) /* disable power-on write protection */

#define EXT_CSD_PARTITION_SETTING_COMPLETED	(1 << 0)

#define MMC_RESPONSE_PRESENT (1 << 0)
#define MMC_RSP_136	(1 << 1)		/* 136 bit response */
#define MMC_RESPONSE_CRC	(1 << 2)		/* expect valid crc */
#define MMC_RSP_BUSY	(1 << 3)		/* card may send busy */
#define MMC_RESPONSE_OPCODE	(1 << 4)		/* response contains opcode */

#define MMC_RSP_NONE	(0)
#define MMC_RSP_R1	(MMC_RESPONSE_PRESENT|MMC_RESPONSE_CRC|MMC_RESPONSE_OPCODE)
#define MMC_RSP_R1b	(MMC_RESPONSE_PRESENT|MMC_RESPONSE_CRC|MMC_RESPONSE_OPCODE| \
			MMC_RSP_BUSY)
#define MMC_RSP_R2	(MMC_RESPONSE_PRESENT|MMC_RSP_136|MMC_RESPONSE_CRC)
#define MMC_RSP_R3	(MMC_RESPONSE_PRESENT)
#define MMC_RSP_R4	(MMC_RESPONSE_PRESENT)
#define MMC_RSP_R5	(MMC_RESPONSE_PRESENT|MMC_RESPONSE_CRC|MMC_RESPONSE_OPCODE)
#define MMC_RSP_R6	(MMC_RESPONSE_PRESENT|MMC_RESPONSE_CRC|MMC_RESPONSE_OPCODE)
#define MMC_RSP_R7	(MMC_RESPONSE_PRESENT|MMC_RESPONSE_CRC|MMC_RESPONSE_OPCODE)

#define MMC_RSP_PRESENT MMC_RESPONSE_PRESENT
#define MMC_RSP_CRC MMC_RESPONSE_CRC
#define MMC_RSP_OPCODE MMC_RESPONSE_OPCODE

#define PARTITION_USER		(0)
#define PARTITION_BOOT1		(1)
#define PARTITION_BOOT2		(2)
#define PARTITION_RPMB		(3)

#define MMCPART_NOAVAILABLE	(0xff)
#define PART_ACCESS_MASK	(0x3F)
#define PART_SUPPORT		(0x1)
#define ENHNCD_SUPPORT		(0x2)
#define PART_ENH_ATTRIB		(0x1f)

struct mmc_cmd {
	ushort cmdidx;
	uint resp_type;
	uint cmdarg;
	uint response[4];
};

enum mmc_voltage {
	MMC_SIGNAL_VOLTAGE_000 = 0,
	MMC_SIGNAL_VOLTAGE_120 = 1,
	MMC_SIGNAL_VOLTAGE_180 = 2,
	MMC_SIGNAL_VOLTAGE_330 = 4,
};

#define MMC_MAX_BLOCK_LEN	(512)

struct mmc_data {
	union {
		char *dest;
		const char *src;
	};
	uint flags;
	uint blocks;
	uint blocksize;
};

struct mmc;

struct mmc_hwpart_conf {
	struct {
		uint enh_start;	/* in 512-byte sectors */
		uint enh_size;	/* in 512-byte sectors, if 0 no enh area */
		unsigned wr_rel_change:1;
		unsigned wr_rel_set:1;
	} user;
	struct {
		uint size;	/* in 512-byte sectors */
		unsigned enhanced:1;
		unsigned wr_rel_change:1;
		unsigned wr_rel_set:1;
	} gp_part[4];
};

struct mmc_ops {
	int (*send_cmd)(struct mmc *mmc,
			struct mmc_cmd *cmd, struct mmc_data *data);
	void (*set_ios)(struct mmc *mmc);
	int (*init)(struct mmc *mmc);
	int (*getcd)(struct mmc *mmc);
	int (*getwp)(struct mmc *mmc);
#ifdef CONFIG_MMC_SUPPORTS_TUNING
	/**
	 * execute_tuning() - Start the tuning process
	 *
	 * @dev:	Device to start the tuning
	 * @opcode: Command opcode to send
	 * @return 0 if OK, -ve on error
	 */
	int (*execute_tuning)(struct mmc *mmc, uint opcode);
#endif
	int (*close_clock)(struct mmc *mmc);
	int (*start_signal_voltage_switch)(struct mmc *mmc);
	int (*card_busy)(struct mmc *mmc);
	void (*set_dll_dly)(struct mmc *mmc, uint32_t delay_value, uint32_t mask);
	uint32_t (*get_dll_dly)(struct mmc *mmc);
	int (*enable_dpll_scan)(struct mmc *mmc);
	int (*reset_for_scan)(struct mmc *mmc);
};

enum mmc_hwpart_conf_mode {
	MMC_HWPART_CONF_CHECK,
	MMC_HWPART_CONF_SET,
	MMC_HWPART_CONF_COMPLETE,
};

struct mmc_cid {
	unsigned long psn;
	unsigned short oid;
	unsigned char mid;
	unsigned char prv;
	unsigned char mdt;
	char pnm[7];
};

struct mmc_config {
	const char *name;
	const struct mmc_ops *ops;
	uint host_caps;
	uint voltages;
	uint f_min;
	uint f_max;
	uint b_max;
	unsigned char part_type;
};

enum bus_mode {
	MMC_LEGACY,
	MMC_HS,
	SD_HS,
	MMC_HS_52,
	MMC_DDR_52,
	UHS_SDR12,
	UHS_SDR25,
	UHS_SDR50,
	UHS_DDR50,
	UHS_SDR104,
	MMC_HS_200,
	MMC_HS_400,
	MMC_HS_400_ES,
	MMC_MODES_END
};

typedef enum {
	MMC = 1,
	SDIO0 = 2,
	SDIO1 = 4,
	SDIO2 = 8,
	SDIO_HOST_TYPE_MAX
} SDIO_HOST_E;

typedef enum {
	eMMC = 1,
	SD = 2,
	SDIO = 4,
	SDIO_SLAVE_TYPE_MAX
} SDIO_SLAVE_E;

typedef enum {
	SCAN_MMC_HS = 1,
	SCAN_MMC_DDR50 = 2,
	SCAN_MMC_HS200 = 4,
	SCAN_MMC_HS400 = 8,
	SCAN_MMC_HS401 = 0x10,
	SDIO_SPEED_MODE_MAX
} SDIO_SPEED_MODE_E;

struct mmc {
	struct list_head link;
	struct mmc_config *cfg;
	uint version;
	void *priv;
	uint has_init;
	int high_capacity;
	uint bus_width;
	uint clock;
	uint card_caps;
	uint ocr;
	uint dsr;
	uint dsr_imp;
	uint scr[2];
	uint csd[4];
	uint cid[4];
	ushort rca;
	u8 part_support;
	u8 part_attr;
	u8 wr_rel_set;
	char part_config;
	int part_num;
	uint tran_speed;
	uint read_bl_len;
	uint write_bl_len;
	uint erase_grp_size;	/* in 512-byte sectors */
	uint hc_wp_grp_size;	/* in 512-byte sectors */
	u8 erase_mem_cont;
	u64 capacity;
	u64 capacity_user;
	u64 capacity_boot;
	u64 capacity_rpmb;
	u8   rel_wr_sec_c;
	u64 capacity_gp[4];
	u64 enh_user_start;
	u64 enh_user_size;
	block_dev_desc_t block_dev;
	char op_cond_pending;
	char init_in_progress;
	char preinit;
	int ddr_mode;
	uint wp_enable;

	uint host_caps;
	uint legacy_speed; /* speed for the legacy mode provided by the card */
	u8 *ext_csd;
	u32 cardtype;		/* cardtype read from the MMC */
	enum bus_mode selected_mode; /* mode currently used */
	enum bus_mode best_mode; /* best mode is the supported mode with the
				  * highest bandwidth. It may not always be the
				  * operating mode due to limitations when
				  * accessing the boot partitions
				  */
	enum mmc_voltage select_voltage;
	bool in_scan;
};

int mmc_get_wp_grp_size(void);
struct mmc *mmc_create(const struct mmc_config *cfg, void *priv);
int mmc_init(struct mmc *mmc);
void mmc_set_clock(struct mmc *mmc, uint clock);
struct mmc *board_sd_init(void);
struct mmc *find_mmc_device(int dev_num);
void print_mmc_devices(char separator);
int mmc_switch_part(int dev_num, unsigned int part_num);
int mmc_set_rst_n_function(struct mmc *mmc, u8 enable);
int mmc_enable_pwr_wp(struct mmc *mmc);
int mmc_rpmb_set_key(struct mmc *mmc, void *key);
int mmc_rpmb_get_counter(struct mmc *mmc, unsigned long *counter);
int mmc_rpmb_read(struct mmc *mmc, void *addr, unsigned short blk,
		  unsigned short cnt, unsigned char *key);
int mmc_rpmb_write(struct mmc *mmc, void *addr, unsigned short blk,
		   unsigned short cnt, unsigned char *key);
int mmc_start_init(struct mmc *mmc);
void mmc_get_cid(char *cid);
void mmc_get_ext_csd(char *ext_csd);


#ifdef CONFIG_GENERIC_MMC
#ifdef CONFIG_MMC_SPI
#define mmc_host_is_spi(mmc)	((mmc)->cfg->host_caps & MMC_MODE_SPI)
#else
#define mmc_host_is_spi(mmc)	0
#endif
struct mmc *mmc_spi_init(uint bus, uint cs, uint speed, uint mode);
#else
int mmc_legacy_init(int verbose);
#define mmc_host_is_spi(mmc)	0
#endif

u64 emmc_get_capacity(char part_num);
ulong emmc_write_backstage(int part_num, uint32_t start_block,
			uint32_t num, uint8_t *buf);
ulong emmc_query_backstage(int part_num, uint32_t num, uint8_t *buf);
ulong emmc_read_backstage(int part_num, uint32_t start_block,
			uint32_t num, uint8_t *buf);
ulong emmc_query_read_backstage(int part_num, uint32_t num, uint8_t *buf);

#ifndef CONFIG_SYS_MMC_MAX_BLK_COUNT
#define CONFIG_SYS_MMC_MAX_BLK_COUNT 65535
#endif

void sprd_mmc_exit(int sdio_type);
int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data);

#endif
