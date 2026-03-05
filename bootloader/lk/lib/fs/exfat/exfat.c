#include <lib/fs.h>
#include <lk/trace.h>
#include <lk/debug.h>
#include <malloc.h>
#include <string.h>
#include <sprd_common_rw.h>

#include "exfat.h"
#include "exfat_priv.h"

uint8_t bitmap_mask[9] = {
    0x00,
    0x01,
    0x03,
    0x07,
    0x0f,
    0x1f,
    0x3f,
    0x7f,
    0xff
};

/* use '&' clr_mask to clear bit */
uint8_t bitmap_clr_mask[8] = {
    0xfe,
    0xfd,
    0xfb,
    0xf7,
    0xef,
    0xdf,
    0xbf,
    0x7f
};

uint8_t bits_num_table[256];

uint8_t bitmap_avail[256] =
{
/*000*/8,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*016*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*032*/5,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*048*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*064*/6,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*080*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*096*/5,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*112*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*128*/7,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*144*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*160*/5,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*176*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*192*/6,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*208*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*224*/5,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
/*240*/4,0,1,0,2,0,1,0,  3,0,1,0,2,0,1,0,
};

/*
 * Convert a string to lowercase.
 */
void exfat_downcase(char *str)
{
    while (*str != '\0') {
        *str = tolower(*str);
        str++;
    }
}

void exfat_bits_table_init(void)
{
    int bits_mask, bytes_in_bitmap, count;
    for (bytes_in_bitmap = 0; bytes_in_bitmap < 256; bytes_in_bitmap++) {
        count = 0;
        for (bits_mask = 0x80; bits_mask > 0; bits_mask >>= 1) {
            if (bits_mask & bytes_in_bitmap)
                count++;
        }
        bits_num_table[bytes_in_bitmap] = count;
    }
    return;
}

/*
 * Read boot sector and volume info from a exFAT filesystem
 */
int read_exfat_bootsec(exfat_fs_t *exfat, exfat_boot_sector *bs)
{
    uint8_t *block;
    int result = 0;
    block_dev_desc_t *cur_dev = exfat->dev;

    block = (uint8_t *)exfat_malloc(cur_dev->blksz);
    if (block == NULL) {
        dprintf(INFO, "Error: allocating block\n");
        return -1;
    }

    if (exfat_disk_read(exfat, 0, 1, block) < 0) {
        dprintf(INFO, "Error: reading block\n");
        goto fail;
    }

    dprintf(INFO, "Size of exfat boot sector: %lu\n", sizeof(struct exfat_boot_sector));
    memcpy(bs, block, sizeof(struct exfat_boot_sector));
    bs->part_off = EXFAT2CPU64(bs->part_off);
    bs->vol_len = EXFAT2CPU64(bs->vol_len);
    bs->fat_off = EXFAT2CPU32(bs->fat_off);
    bs->fat_len = EXFAT2CPU32(bs->fat_len);
    bs->cluster_heap_off = EXFAT2CPU32(bs->cluster_heap_off);
    bs->cluster_cnt = EXFAT2CPU32(bs->cluster_cnt);
    bs->root_cluster = EXFAT2CPU32(bs->root_cluster);
    bs->vol_serial = EXFAT2CPU32(bs->vol_serial);
    bs->fs_version = EXFAT2CPU16(bs->fs_version);
    bs->vol_flags = EXFAT2CPU16(bs->vol_flags);
    if (bs->fs_version != 0x0100) {
        dprintf(INFO, "Error: exfat version %04x may not be supported.\n", bs->fs_version);
        goto fail;
    }

    if (bs->root_cluster < 2 || bs->root_cluster >= bs->cluster_cnt) {
        dprintf(INFO, "Error: root dir start cluster %d out of range.\n", bs->root_cluster);
        goto fail;
    }

    if (bs->sector_size_bits > 12 || bs->sector_size_bits < 9 ||
        (bs->sectors_per_clu_bits+bs->sector_size_bits) > 25) {
        dprintf(INFO, "Error: sector or cluster size not supported. sec=%d, cluster=%d\n", bs->sector_size_bits, bs->sectors_per_clu_bits);
        goto fail;
    }
    exfat->total_sectors = bs->cluster_heap_off + bs->cluster_cnt * (1<<bs->sectors_per_clu_bits);
    dprintf(INFO, "Total sector: %u\n", exfat->total_sectors);
    goto exit;
fail:
    dprintf(INFO, "Error: broken fs_type sign\n");
    result = -1;
exit:
    free_malloc(block);
    return result;
}

int exfat_disk_read(exfat_fs_t *exfat, uint32_t block, uint32_t nr_blocks, void *buf)
{
    block_dev_desc_t *dev = exfat->dev;
    int ret = 0;

    if (!dev || !dev->block_read)
        return -1;

    ret = dev->block_read(dev->dev_num, exfat->part_start + block, nr_blocks, buf);
    if (ret == 0) {
        errorf("mmc_bread Error!!!\n");
        ret = -1;
    }
    return ret;
}

int exfat_disk_write(exfat_fs_t *exfat, uint32_t block, uint32_t nr_blocks, void *buf)
{
    block_dev_desc_t *dev = exfat->dev;
    int ret = 0;

    if (!dev || !dev->block_write)
        return -1;

    ret = dev->block_write(dev->dev_num, exfat->part_start + block, nr_blocks, buf);
    if (ret == 0) {
        errorf("mmc_bwrite Error!!!\n");
        ret = -1;
    }
    return ret;
}

int get_exfat_blocks(exfat_fs_t *exfat, uint32_t start_sec, uint8_t *buffer,  uint32_t rd_sec)
{
    uint32_t idx = 0;
    int ret;

    ret = exfat_disk_read(exfat, start_sec, rd_sec, buffer);
    if (ret != rd_sec) {
        dprintf(INFO, "Error reading data (got %d)\n", ret);
        return -1;
    }
    return 0;
}

int get_upcase_table(exfat_fs_t *exfat, uint32_t start_cl, uint32_t length)
{
    uint32_t  start_sec, rd_sec;
    start_sec = exfat->data_begin + start_cl * exfat->sectors_per_cluster;
    rd_sec = (length + exfat->bytes_per_sector - 1) / exfat->bytes_per_sector;

    exfat->uptab_buf = (uint16_t *)exfat_malloc(rd_sec*exfat->bytes_per_sector);
    exfat->uptab_sec_size = rd_sec;

    if (get_exfat_blocks(exfat, start_sec, exfat->uptab_buf, rd_sec) != 0) {
        dprintf(INFO, "Error: reading upcase table\n");
        return  -1;
    }
    return 0;
}

int exfat_valid_check(exfat_fs_t *exfat)
{
    block_dev_desc_t *cur_dev = exfat->dev;
    void *buffer;
    int ret = -1;

    buffer = exfat_malloc(cur_dev->blksz);
    if (buffer == NULL) {
        dprintf(INFO, "Error: malloc failed\n");
        return ret;
    }

    /* Make sure it has a valid FAT header */
    if (exfat_disk_read(exfat, 0, 1, buffer) != 1)
        goto exit;

    /* Check if it's actually a DOS volume */
    if (memcmp(buffer + DOS_BOOT_MAGIC_OFFSET, "\x55\xAA", 2)) {
        dprintf(INFO, "Error: not a DOS volume\n");
        goto exit;
    }

    /* Check for EXFAT filesystem */
    if (memcmp(buffer + EXFAT_FS_TYPE_OFFSET, "EXFAT   ", 8)) {
        dprintf(INFO, "Error: not exfat fs\n");
        goto exit;
    }

    ret = 0;

exit:
    free_malloc(buffer);
    return ret;
}

int exfat_dentry_init(exfat_fs_t *exfat, exfat_whole_dentry *p_ent) {
    p_ent->bf0.pbuf = exfat_malloc(exfat->bytes_per_sector + EXFAT_EXTRA_BUFFER_SIZE);
    if (p_ent->bf0.pbuf == NULL) {
        debugf("Error: allocate memory failed.\n");
        return -1;
    }
    p_ent->bf1.pbuf = exfat_malloc(exfat->bytes_per_sector + EXFAT_EXTRA_BUFFER_SIZE);
    if (p_ent->bf1.pbuf == NULL) {
        debugf("Error: allocate memory failed.\n");
        return -1;
    }
    return 0;
}

void exfat_dentry_exchange(exfat_whole_dentry *p_ent0, exfat_whole_dentry *p_ent1) {
    exfat_whole_dentry tmp;
    memcpy(&tmp, p_ent0, sizeof(exfat_whole_dentry));
    memcpy(p_ent0, p_ent1, sizeof(exfat_whole_dentry));
    memcpy(p_ent1, &tmp, sizeof(exfat_whole_dentry));
}

void exfat_dentry_clear(exfat_fs_t *exfat, exfat_whole_dentry *p_ent) {
    void *tmp_pbuf0, *tmp_pbuf1, *tmp_parent;
    tmp_pbuf0 = p_ent->bf0.pbuf;
    tmp_pbuf1 = p_ent->bf1.pbuf;
    tmp_parent = p_ent->parent;
    memset(tmp_pbuf0, 0, exfat->bytes_per_sector + EXFAT_EXTRA_BUFFER_SIZE);
    memset(tmp_pbuf1, 0, exfat->bytes_per_sector + EXFAT_EXTRA_BUFFER_SIZE);
    memset(p_ent, 0, sizeof(exfat_whole_dentry));
    p_ent->bf0.pbuf = tmp_pbuf0;
    p_ent->bf1.pbuf = tmp_pbuf1;
    p_ent->parent = tmp_parent;
}


void free_malloc(void *ptr) {
    if (ptr != NULL)
        free(ptr);
    ptr = NULL;
}

void *exfat_malloc(size_t len) {
    void *ptr = NULL;
    ptr = malloc(len);
    if (ptr != NULL)
        memset(ptr, 0, len);
    return ptr;
}

void free_exfat(exfat_fs_t *exfat)
{
    if (exfat == NULL)
        return;
    if (exfat->fatbuf) {
        free_malloc(exfat->fatbuf);
    }
    if (exfat->bitmap) {
        free_malloc(exfat->bitmap);
    }

    if (exfat->uptab_buf) {
        free_malloc(exfat->uptab_buf);
    }

    if (exfat->bitmap_fatbuf) {
        free_malloc(exfat->bitmap_fatbuf);
    }
    dprintf(INFO, "free exfat\n");
    free_malloc(exfat);
}
