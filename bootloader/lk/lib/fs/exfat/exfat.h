#ifndef _EXFAT_H_
#define _EXFAT_H_

#include <linux/byteorder/little_endian.h>
#include <lk/err.h>
#include <lk/debug.h>
#include <endian.h>
#include <stdint.h>
#include <sprd_common_rw.h>
#include <ctype.h>
#include <lib/fs.h>
#include <lk/trace.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sprd_rtc_def.h>

#if __LITTLE_ENDIAN
#define EXFAT2CPU16(x)  (x)
#define EXFAT2CPU32(x)  (x)
#define EXFAT2CPU64(x)  (x)
#else
#define EXFAT2CPU16(x)  SWAP_16(x)
#define EXFAT2CPU32(x)  SWAP_32(x)
#define EXFAT2CPU64(x)  SWAP_64(x)
#endif

#define DIV_ROUND_UP(x, d) (((x) + (d) - 1) / (d))
#define ROUND_UP(x, d) (DIV_ROUND_UP(x, d) * (d))


#define FNAME_MAXLEN_BYTES         256        /* Maximum LFN buffer in bytes */
#define MAX_RD_BLK_SIZE            65536
#define EXFAT_FAT_ENTRY_SIZE       4
#define EXFAT_EXTRA_BUFFER_SIZE    64
#define DEFAULT_BOUNDARY_ALIGNMENT (1024*1024)
#define DEFAULT_SECTOR_SIZE        512
#define DEFAULT_CLUSTER_SIZE       32768
#define DEFAULT_VOLUME_LABEL_LEN   7
#define MAX_NUM_CLUSTER            0xFFFFFFF5
#define UPCASE_TABLE_SIZE          5836
#define PBR_SIGNATURE              0xAA55


#define EXFAT_ATTR_RDONLY         0x01
#define EXFAT_ATTR_HIDDEN         0x02
#define EXFAT_ATTR_SYSTEM         0x04
#define EXFAT_ATTR_DIR            0x10
#define EXFAT_ATTR_FILE           0x20
#define EXFAT_ATTR_IGNORE         0xff


#define EXFAT_ENAME_MAX           15
#define VOLUME_LABEL_MAX_LEN      11
#define EXFAT_FIRST_DATA_CLUSTER  2
#define EXFAT_LAST_DATA_CLUSTER   0xfffffff6
#define EXFAT_CLUSTER_FREE        0          /* free cluster */
#define EXFAT_CLUSTER_BAD         0xfffffff7 /* cluster contains bad sector */
#define EXFAT_CLUSTER_END         0xffffffff /* final cluster of file or directory */
#define	CHAR_BIT                  8          /* number of bits in a char */

#define DOS_BOOT_MAGIC_OFFSET     0x1fe
#define EXFAT_FS_TYPE_OFFSET      0x3

enum {
    EXFAT_DTYPE_BM = 0x01,                   /* bitmap entry */
    EXFAT_DTYPE_UC = 0x02,                   /* up-case table entry */
    EXFAT_DTYPE_LABLE = 0x03,                /* label */
    EXFAT_DTYPE_FILE = 0x05,                 /* regular file or directory */
    EXFAT_DTYPE_STREXT = 0x40,               /* stream extension entry */
    EXFAT_DTYPE_FNEXT = 0x41,                /* file name extension entry */
    EXFAT_DTYPE_MASK = 0x7f,                 /* entry type mask */
    EXFAT_DTYPE_USED = 0x80,                 /* flag bit of whether entry is in used or not */
};

enum {
    BOOT_SEC_IDX = 0,
    EXBOOT_SEC_IDX,
    EXBOOT_SEC_NUM = 8,
    OEM_SEC_IDX,
    RESERVED_SEC_IDX,
    CHECKSUM_SEC_IDX,
    BACKUP_BOOT_SEC_IDX,
};

typedef struct exfat_boot_sector {
    uint8_t       ignored[3];
    char          fs_name[8];
    uint8_t       zero_bytes[53];
    uint64_t      part_off;
    uint64_t      vol_len;
    uint32_t      fat_off;
    uint32_t      fat_len;
    uint32_t      cluster_heap_off;
    uint32_t      cluster_cnt;
    uint32_t      root_cluster;
    uint32_t      vol_serial;
    uint16_t      fs_version;
    uint16_t      vol_flags;
    uint8_t       sector_size_bits;
    uint8_t       sectors_per_clu_bits;
    uint8_t       num_fats;
    uint8_t       phy_drv_no;
    uint8_t       perc_in_use;
    uint8_t       reserved[7];
    uint8_t       boot_code[390];
    uint8_t       boot_signature[2];
} exfat_boot_sector;

typedef struct exfat_label_entry        /* volume label */
{
    uint8_t  entry_type;                /* EXFAT_ENTRY_LABEL */
    uint8_t  length;                    /* number of characters */
    uint16_t name[EXFAT_ENAME_MAX];     /* in UTF-16LE */
}exfat_label_entry;

typedef struct exfat_bitmap_entry {
    uint8_t entry_type;
    uint8_t bitmap_flags;
    uint8_t reserved[18];
    uint32_t start_clu;
    uint64_t data_len;
}exfat_bitmap_entry;

typedef struct exfat_upcase_entry       /* upper case translation table */
{
    uint8_t  entry_type;                /* EXFAT_ENTRY_UPCASE */
    uint8_t  __unknown1[3];
    uint32_t checksum;
    uint8_t  __unknown2[12];
    uint32_t start_cluster;
    uint64_t size;                      /* in bytes */
} exfat_upcase_entry;

typedef struct exfat_strext_entry {
    uint8_t    entry_type;
    uint8_t    secondary_flags;
    uint8_t    reserved0;
    uint8_t    name_len;
    uint16_t   name_hash;
    uint8_t    reserved1[2];
    uint64_t   val_data_len;
    uint8_t    reserved2[4];
    uint32_t   first_cluster;
    uint64_t   data_len;
} exfat_strext_entry;

typedef struct exfat_file_entry {
    uint8_t    entry_type;
    uint8_t    secondary_cnt;
    uint16_t   checksum;
    uint16_t   attr;
    uint8_t    reserved0[2];
    uint32_t   creat_time;
    uint32_t   mod_time;
    uint32_t   access_time;
    uint8_t    creat_ms_inc;
    uint8_t    mod_ms_inc;
    uint8_t    creat_utc_off;
    uint8_t    mod_utc_off;
    union  {
        struct  {
            uint8_t    access_utc_off;
            uint8_t    reserved1[8];
        }a;
        exfat_strext_entry  *str_dent;
    };
} exfat_file_entry;

typedef struct exfat_fnext_entry {
    uint8_t  entry_type;
    uint8_t  secondary_flags;
    uint16_t uniname[15];
}exfat_fnext_entry;
typedef struct exfat_dentry {
    uint8_t entry_type;
    uint8_t secondary_flags;
    uint8_t bytes[30];
}exfat_dentry;

typedef struct exfat_buf_info {
    uint8_t  *pbuf;
    uint32_t  cluster;
    uint32_t  start_sec;
    uint32_t  sectors;
}exfat_buf_info;

typedef struct exfat_whole_dentry {
    exfat_buf_info bf0;
    exfat_buf_info bf1;
    exfat_file_entry *empty;
    exfat_file_entry *prim;
    exfat_strext_entry *strext;
    exfat_fnext_entry *fname_dent[17];
    void *parent;
}exfat_whole_dentry;

typedef struct exfat_buf_array {
    exfat_buf_info bf0;
    exfat_buf_info bf1;
}exfat_buf_array;


/*
 * Private filesystem parameters
 *
 * Note: exFAT buffer has to be 32 bit aligned
 * (see exFAT accesses)
 */
typedef struct {
    block_dev_desc_t *dev;

    uint8_t      *fatbuf;
    uint8_t      *bitmap;
    uint8_t      *bitmap_fatbuf;
    uint16_t     *uptab_buf;
    uint32_t     uptab_sec_size;
    uint32_t     fat_num;
    uint32_t     fatlength;
    uint32_t     bitmaplen;
    uint32_t     cluster_cnt;
    uint32_t     fat_sect;
    uint32_t     bitmap_sect;
    uint32_t     bitmap_fatbuf_size;
    uint32_t     cur_bitmap_cluster;
    uint32_t     rootdir_sect;
    uint32_t     root_cluster;
    uint16_t     bytes_per_sector;
    uint16_t     sectors_per_cluster;
    uint32_t     bytes_per_cluster;
    uint32_t     rd_sectors;
    uint32_t     fatbuf_idx;
    uint32_t     bitmap_idx;
    uint32_t     data_begin;
    uint32_t     total_sectors;
    uint64_t     part_start;
    uint64_t     total_part_size;
    uint64_t     part_block_size;
    uint64_t     vol_len;
} exfat_fs_t;

typedef struct {
    exfat_fs_t *exfat_fs;
    uint32_t start_cluster;
    uint64_t length;
    uint16_t attributes;
    uint8_t  cont_flag;
    exfat_whole_dentry file_ent;
} exfat_file_t;

typedef struct exfat_blk_info {
    block_dev_desc_t *dev;
    uint64_t offset;
    uint64_t byte_offset;
    uint64_t size;
    uint32_t sector_size;
    uint32_t sector_size_bits;
    uint64_t num_sectors;
    uint32_t num_clusters;
    uint32_t cluster_size;
} exfat_blk_info;

typedef struct exfat_meta_info {
    uint32_t total_clu_cnt;
    uint32_t used_clu_cnt;
    uint32_t fat_byte_off;
    uint32_t fat_byte_len;
    uint32_t fat_sec_off;
    uint32_t clu_byte_off;
    uint32_t bitmap_byte_off;
    uint32_t bitmap_byte_len;
    uint32_t bitmap_sec_off;
    uint32_t ut_byte_off;
    uint32_t ut_start_clu;
    uint32_t ut_sec_off;
    uint32_t ut_clus_off;
    uint32_t ut_byte_len;
    uint32_t root_byte_off;
    uint32_t root_byte_len;
    uint32_t root_start_clu;
    uint32_t root_sec_off;
    uint32_t volume_serial;
} exfat_meta_info;

typedef struct exfat_user_config {
    char dev_name[255];
    bool writeable;
    uint32_t cluster_size;
    uint32_t sector_size;
    uint32_t sec_per_clu;
    uint32_t boundary_align;
    bool pack_bitmap;
    bool quick;
    uint16_t volume_label[VOLUME_LABEL_MAX_LEN];
    uint32_t volume_label_len;
    uint32_t volume_serial;
} exfat_user_config;

typedef struct exfat_extend_boot_sectors {
    __u8 zero[510];
    __le16 signature;
}exfat_extend_boot_sectors;

extern uint8_t bitmap_mask[9];
extern uint8_t bitmap_clr_mask[8];
extern uint8_t bits_num_table[256];
extern uint8_t bitmap_avail[256];

void exfat_downcase(char *str);
void exfat_bits_table_init(void);
int exfat_disk_read(exfat_fs_t *exfat, uint32_t block, uint32_t nr_blocks, void *buf);
int exfat_disk_write(exfat_fs_t *exfat, uint32_t block, uint32_t nr_blocks, void *buf);
int get_exfat_blocks(exfat_fs_t *exfat, uint32_t start_sec, uint8_t *buffer,  uint32_t rd_sec);
int get_upcase_table(exfat_fs_t *exfat, uint32_t start_cl, uint32_t length);
int exfat_valid_check(exfat_fs_t *exfat);
int exfat_dentry_init(exfat_fs_t *exfat, exfat_whole_dentry *p_ent);
int read_exfat_bootsec(exfat_fs_t *exfat, exfat_boot_sector *bs);
void exfat_dentry_exchange(exfat_whole_dentry *p_ent0, exfat_whole_dentry *p_ent1);
void exfat_dentry_clear(exfat_fs_t *exfat, exfat_whole_dentry *p_ent);
void free_exfat(exfat_fs_t *exfat);
void free_malloc(void *ptr);
void *exfat_malloc(size_t len);

#endif /* _EXFAT_H_ */
