#include "exfat_priv.h"
#include "sprd_bitops.h"

exfat_fs_t *g_exfat;
exfat_meta_info minfo;
exfat_blk_info bd;
exfat_user_config ui;

extern struct rtc_time get_time_by_sec(void);

uint8_t default_volume_label[DEFAULT_VOLUME_LABEL_LEN] =
{
    0x61, 0x6e, 0x64, 0x72, 0x6f, 0x69, 0x64
};

/* random serial generator based on current time */
static uint32_t get_new_serial(void)
{
    struct rtc_time tm;

    tm = get_time_by_sec();

    return (uint32_t)(tm.tm_min << 12 | tm.tm_sec);
}

static void boot_calc_checksum(uint8_t *sector, uint16_t size,
        bool is_boot_sec, uint32_t *checksum)
{
    uint32_t index;

    if (is_boot_sec) {
        for (index = 0; index < size; index++) {
            if ((index == 106) || (index == 107) || (index == 112))
                continue;
            *checksum = ((*checksum & 1) ? 0x80000000 : 0) +
                (*checksum >> 1) + sector[index];
        }
    } else {
        for (index = 0; index < size; index++) {
            *checksum = ((*checksum & 1) ? 0x80000000 : 0) +
                (*checksum >> 1) + sector[index];
        }
    }
}

static inline uint32_t size_to_bits(uint32_t size)
{
    uint32_t bits = 8;

    do {
        bits++;
        size >>= 1;
    } while (size > 256);

    return bits;
}

static int disk_write(exfat_blk_info *bd, uint32_t block, uint32_t nr_blocks, void *buf)
{
    block_dev_desc_t *dev = g_exfat->dev;
    int ret = 0;

    if (!dev || !dev->block_write)
        return -1;

    ret = dev->block_write(dev->dev_num, bd->offset + block, nr_blocks, buf);
    if (ret == 0) {
        errorf("mmc_bwrite Error!!!\n");
        ret = -1;
    }
    return ret;
    return 0;
}

void exfat_set_bit(exfat_blk_info *bd, char *bitmap,
        uint32_t clu)
{
    int b;

    b = clu & ((bd->sector_size << 3) - 1);

    generic_set_bit(b, bitmap);
}


static int exfat_write_sector(exfat_blk_info *bd, void *buf, uint32_t sec_off)
{
    int ret;

    ret = disk_write(bd, sec_off, 1, buf);
    if (ret < 0) {
        dprintf(INFO, "write sector failed, sec_off : %u\n", sec_off);
    }
    return ret;
}

static void exfat_setup_boot_sector(exfat_boot_sector *ppbr,
        exfat_blk_info *bd, exfat_user_config *ui)
{
    uint32_t i;

    /* Fill exfat BIOS paramemter block */
    ppbr->ignored[0] = 0xeb;
    ppbr->ignored[1] = 0x76;
    ppbr->ignored[2] = 0x90;
    memcpy(ppbr->fs_name, "EXFAT   ", 8);
    memset(ppbr->zero_bytes, 0, 53);

    /* Fill exfat extend BIOS paramemter block */
    ppbr->part_off = cpu_to_le64(bd->offset);
    ppbr->vol_len = cpu_to_le64(bd->size / bd->sector_size);
    ppbr->fat_off = cpu_to_le32(minfo.fat_sec_off);
    ppbr->fat_len = cpu_to_le32(minfo.fat_byte_len / bd->sector_size);
    ppbr->cluster_heap_off = cpu_to_le32(minfo.bitmap_sec_off);
    ppbr->cluster_cnt = cpu_to_le32(minfo.total_clu_cnt);
    ppbr->root_cluster = cpu_to_le32(minfo.root_start_clu);
    ppbr->vol_serial = cpu_to_le32(minfo.volume_serial);
    ppbr->vol_flags = cpu_to_le32(2);
    ppbr->sector_size_bits = bd->sector_size_bits;
    ppbr->sectors_per_clu_bits = 0;
    /* Compute base 2 logarithm of ui->cluster_size / bd->sector_size */
    for (i = ui->cluster_size / bd->sector_size; i > 1; i /= 2)
        ppbr->sectors_per_clu_bits++;
    ppbr->num_fats = 1;
    /* fs_version[0] : minor and fs_version[1] : major */
    ppbr->fs_version = cpu_to_le16(0x0100);
    memset(ppbr->reserved, 0, 7);

    memset(ppbr->boot_code, 0, 390);
    ppbr->boot_signature[0] = 0x55;
    ppbr->boot_signature[1] = 0xAA;

    dprintf(INFO, "Volume Offset(sectors) : %llu \n",
        le64_to_cpu(ppbr->part_off));
    dprintf(INFO, "Volume Length(sectors) : %llu \n",
        le64_to_cpu(ppbr->vol_len));
    dprintf(INFO, "FAT Offset(sector offset) : %u\n",
        le32_to_cpu(ppbr->fat_off));
    dprintf(INFO, "FAT Length(sectors) : %u\n",
        le32_to_cpu(ppbr->fat_len));
    dprintf(INFO, "Cluster Heap Offset (sector offset) : %u\n",
        le32_to_cpu(ppbr->cluster_heap_off));
    dprintf(INFO, "Upcase table Offset (sector offset) : %u\n",
        le32_to_cpu(minfo.ut_sec_off));
    dprintf(INFO, "Cluster Count : %u\n",
        le32_to_cpu(ppbr->cluster_cnt));
    dprintf(INFO, "Root Cluster (cluster offset) : %u\n",
        le32_to_cpu(ppbr->root_cluster));
    dprintf(INFO, "Root Cluster (sector offset) : %u\n",
        le32_to_cpu(minfo.root_sec_off));
    dprintf(INFO, "Volume Serial : 0x%x\n", le32_to_cpu(ppbr->vol_serial));
    dprintf(INFO, "Sector Size Bits : %u\n",
        ppbr->sector_size_bits);
    dprintf(INFO, "Sector per Cluster bits : %u\n",
        ppbr->sectors_per_clu_bits);
}

static int exfat_write_boot_sector(exfat_blk_info *bd,
        exfat_user_config *ui, uint32_t *checksum,
        bool is_backup)
{
    exfat_boot_sector *ppbr;
    uint32_t sec_idx = BOOT_SEC_IDX;
    int ret = 0;

    if (is_backup)
        sec_idx += BACKUP_BOOT_SEC_IDX;

    ppbr = malloc(sizeof(exfat_boot_sector));
    if (ppbr == NULL) {
        dprintf(INFO, "Cannot allocate pbr: out of memory\n");
        return -1;
    }
    memset(ppbr, 0, sizeof(exfat_boot_sector));

    exfat_setup_boot_sector(ppbr, bd, ui);

    /* write main boot sector */
    ret = exfat_write_sector(bd, ppbr, sec_idx);
    if (ret < 0) {
        dprintf(INFO, "main boot sector write failed\n");
        ret = -1;
        goto free_ppbr;
    }

    boot_calc_checksum((uint8_t *)ppbr, sizeof(exfat_boot_sector),
        true, checksum);

free_ppbr:
    free(ppbr);
    return ret;
}

static int exfat_write_extended_boot_sectors(exfat_blk_info *bd,
        uint32_t *checksum, bool is_backup)
{
    exfat_extend_boot_sectors eb;
    int i;
    uint32_t sec_idx = EXBOOT_SEC_IDX;

    if (is_backup)
        sec_idx += BACKUP_BOOT_SEC_IDX;

    memset(&eb, 0, sizeof(exfat_extend_boot_sectors));
    eb.signature = cpu_to_le16(PBR_SIGNATURE);
    for (i = 0; i < EXBOOT_SEC_NUM; i++) {
        if (exfat_write_sector(bd, &eb, sec_idx++) < 0) {
            dprintf(INFO, "extended boot sector write failed\n");
            return -1;
        }

        boot_calc_checksum((uint8_t *) &eb, sizeof(exfat_extend_boot_sectors),
            false, checksum);
    }

    return 0;
}

static int exfat_write_checksum_sector(exfat_blk_info *bd,
        uint32_t checksum, bool is_backup)
{
    uint32_t *checksum_buf;
    int ret = 0;
    uint32_t i;
    uint32_t sec_idx = CHECKSUM_SEC_IDX;

    checksum_buf = malloc(bd->sector_size);
    if (checksum_buf == NULL){
        dprintf(INFO, "malloc failed\n");
        return -1;
    }
    memset(checksum_buf, 0, bd->sector_size);

    if (is_backup)
        sec_idx += BACKUP_BOOT_SEC_IDX;

    for (i = 0; i < bd->sector_size / sizeof(int); i++)
        checksum_buf[i] = cpu_to_le32(checksum);

    ret = exfat_write_sector(bd, checksum_buf, sec_idx);
    if (ret < 0) {
        dprintf(INFO, "checksum sector write failed\n");
        goto free;
    }

free:
    free(checksum_buf);
    return ret;
}

static int exfat_write_oem_sector(exfat_blk_info *bd,
        uint32_t *checksum, bool is_backup)
{
    char *oem;
    int ret = 0;
    uint32_t sec_idx = OEM_SEC_IDX;

    oem = malloc(bd->sector_size);
    if (oem == NULL)
        return -1;

    if (is_backup)
        sec_idx += BACKUP_BOOT_SEC_IDX;

    memset(oem, 0xFF, bd->sector_size);
    ret = exfat_write_sector(bd, oem, sec_idx);
    if (ret < 0) {
        dprintf(INFO, "oem sector write failed\n");
        ret = -1;
        goto free_oem;
    }

    boot_calc_checksum((uint8_t *)oem, bd->sector_size, false,
        checksum);

    memset(oem, 0, bd->sector_size);
    ret = exfat_write_sector(bd, oem, sec_idx + 1);
    if (ret < 0) {
        dprintf(INFO, "reserved sector write failed\n");
        ret = -1;
        goto free_oem;
    }

    boot_calc_checksum((uint8_t *)oem, bd->sector_size, false,
        checksum);

free_oem:
    free(oem);
    return ret;
}

static int exfat_create_volume_boot_record(exfat_blk_info *bd,
        exfat_user_config *ui, bool is_backup)
{
    uint32_t checksum = 0;
    int ret = -1;

    if (exfat_write_boot_sector(bd, ui, &checksum, is_backup) < 0)
        return ret;

    if (exfat_write_extended_boot_sectors(bd, &checksum, is_backup) < 0)
        return ret;

    if (exfat_write_oem_sector(bd, &checksum, is_backup) < 0)
        return ret;

    return exfat_write_checksum_sector(bd, checksum, is_backup);
}

static int set_fat_entries(exfat_user_config *ui, uint32_t clu, uint32_t length, uint32_t *fat_table)
{
    int ret;
    uint32_t count;

    count = clu + ROUND_UP(length, ui->cluster_size) / ui->cluster_size;

    for (; clu < count - 1; clu++) {
        fat_table[clu] = cpu_to_le32(clu+1);
    }

    fat_table[clu] = cpu_to_le32(EXFAT_CLUSTER_END);

    return clu;
}

static int exfat_create_fat_table(exfat_blk_info *bd,
        exfat_user_config *ui)
{
    int ret, clu;
    uint32_t count;
    uint32_t *fat_table;

    fat_table = calloc(bd->sector_size / sizeof(uint32_t), sizeof(uint32_t));
    if (fat_table == NULL)
        return -1;

    memset(fat_table, 0, sizeof(bd->sector_size));

    /* fat entry 0 should be media type field(0xF8) */
    fat_table[0] = cpu_to_le32(0xfffffff8);

    /* fat entry 1 is historical precedence(0xFFFFFFFF) */
    fat_table[1] = cpu_to_le32(0xffffffff);

    /* write bitmap entries */
    /* write bitmap entries */
    clu = set_fat_entries(ui, EXFAT_FIRST_DATA_CLUSTER, minfo.bitmap_byte_len, fat_table);

    /* write upcase table entries */
    clu = set_fat_entries(ui, clu + 1, minfo.ut_byte_len, fat_table);

    /* write root directory entries */
    clu = set_fat_entries(ui, clu + 1, minfo.root_byte_len, fat_table);

    minfo.used_clu_cnt = clu + 1;
    dprintf(INFO, "Total used cluster count : %d\n", minfo.used_clu_cnt);

    ret = disk_write(bd, minfo.fat_sec_off, 1, fat_table);
    if (ret < 0)
        dprintf(INFO, "Error: write fat table failed. \n");

    free(fat_table);
    return ret;
}

static int exfat_create_bitmap(exfat_blk_info *bd)
{
    char *bitmap;
    uint32_t i;

    bitmap = calloc(minfo.bitmap_byte_len, sizeof(*bitmap));
    if (!bitmap)
        return -1;

    for (i = 0; i < minfo.used_clu_cnt - EXFAT_FIRST_DATA_CLUSTER; i++)
        exfat_set_bit(bd, bitmap, i);

    if (disk_write(bd, minfo.bitmap_sec_off, minfo.bitmap_byte_len / bd->sector_size, bitmap) < 0) {
        dprintf(INFO, "write failed, bitmap_len : %d\n", minfo.bitmap_byte_len);
        free(bitmap);
        return -1;
    }

    free(bitmap);
    return 0;
}

static int exfat_create_upcase_table(exfat_blk_info *bd)
{
    uint32_t uct_sec_size;

    uct_sec_size = g_exfat->uptab_sec_size;
    if (disk_write(bd, minfo.ut_sec_off, uct_sec_size, g_exfat->uptab_buf) < 0) {
        return -1;
    }
    return 0;
}

static int exfat_create_root_dir(exfat_blk_info *bd,
        exfat_user_config *ui)
{
    exfat_label_entry lable_ent;
    exfat_bitmap_entry bitmap_ent;
    exfat_upcase_entry upcase_ent;
    char *rootdir;
    int i, ret = 0;

    rootdir = malloc(bd->sector_size);
    if (rootdir == NULL)
        return -1;

    memset(rootdir, 0, bd->sector_size);
    memset(&upcase_ent, 0, sizeof(exfat_upcase_entry));

    /* Set volume label entry */
    memset(&lable_ent, 0, sizeof(exfat_label_entry));
    lable_ent.entry_type = EXFAT_DTYPE_LABLE | EXFAT_DTYPE_USED;
    lable_ent.length = ui->volume_label_len;
    for (i = 0; i < lable_ent.length; i++) {
        lable_ent.name[i] = default_volume_label[i];
    }

    /* Set bitmap entry */
    memset(&bitmap_ent, 0, sizeof(exfat_bitmap_entry));
    bitmap_ent.entry_type = EXFAT_DTYPE_BM | EXFAT_DTYPE_USED;
    bitmap_ent.bitmap_flags = 0;
    bitmap_ent.start_clu = cpu_to_le32(EXFAT_FIRST_DATA_CLUSTER);
    bitmap_ent.data_len = cpu_to_le64(minfo.bitmap_byte_len);

    /* Set upcase table entry */
    upcase_ent.entry_type = EXFAT_DTYPE_UC | EXFAT_DTYPE_USED;
    upcase_ent.checksum = cpu_to_le32(0xe619d30d);
    upcase_ent.start_cluster = cpu_to_le32(minfo.ut_start_clu);
    upcase_ent.size = cpu_to_le64(UPCASE_TABLE_SIZE);

    exfat_dentry *dentptr;
    dentptr = (exfat_dentry *)rootdir;

    memcpy(dentptr, &lable_ent, sizeof(exfat_label_entry));
    dentptr++;
    memcpy(dentptr, &bitmap_ent, sizeof(exfat_bitmap_entry));
    dentptr++;
    memcpy(dentptr, &upcase_ent, sizeof(exfat_upcase_entry));

    if (disk_write(bd, minfo.root_sec_off, 1, rootdir) < 0) {
        dprintf(INFO, "Error: write rootdir failed.\n");
        ret = -1;
    }

    free(rootdir);
    return ret;
}

static int exfat_build_info(exfat_blk_info *bd,
        exfat_user_config *ui)
{
    unsigned long long total_clu_cnt;
    int clu_len;

    if (ui->boundary_align < bd->sector_size) {
        dprintf(INFO, "boundary alignment is too small (min %d)\n",
                bd->sector_size);
        return -1;
    }
    minfo.fat_byte_off = ROUND_UP(bd->byte_offset + (uint64_t)24 * bd->sector_size,
            ui->boundary_align) - bd->byte_offset;
    minfo.fat_sec_off = minfo.fat_byte_off / bd->sector_size;
    minfo.fat_byte_len = ROUND_UP((bd->num_clusters * sizeof(int)),
        ui->cluster_size);
    minfo.clu_byte_off = ROUND_UP(bd->byte_offset + minfo.fat_byte_off +
        minfo.fat_byte_len, ui->boundary_align) - bd->byte_offset;
    if (bd->size <= minfo.clu_byte_off) {
        dprintf(INFO, "boundary alignment is too big\n");
        return -1;
    }
    total_clu_cnt = (bd->size - minfo.clu_byte_off) / ui->cluster_size;
    if (total_clu_cnt > MAX_NUM_CLUSTER) {
        dprintf(INFO, "cluster size is too small\n");
        return -1;
    }
    minfo.total_clu_cnt = (uint32_t) total_clu_cnt;

    minfo.bitmap_byte_off = minfo.clu_byte_off;
    minfo.bitmap_sec_off = minfo.clu_byte_off / bd->sector_size;
    minfo.bitmap_byte_len = ROUND_UP(minfo.total_clu_cnt, 8) / 8;

    clu_len = ROUND_UP(minfo.bitmap_byte_len, ui->cluster_size);

    minfo.ut_start_clu = EXFAT_FIRST_DATA_CLUSTER + clu_len / ui->cluster_size;
    minfo.ut_byte_off = minfo.bitmap_byte_off + clu_len;
    minfo.ut_byte_len = UPCASE_TABLE_SIZE;
    minfo.ut_sec_off = minfo.ut_byte_off / bd->sector_size;
    clu_len = ROUND_UP(minfo.ut_byte_len, ui->cluster_size);

    minfo.root_start_clu = minfo.ut_start_clu + clu_len / ui->cluster_size;
    minfo.root_byte_off = minfo.ut_byte_off + clu_len;
    minfo.root_byte_len = sizeof(exfat_dentry) * 3;
    minfo.root_sec_off = minfo.root_byte_off / bd->sector_size;
    minfo.volume_serial = ui->volume_serial;

    return 0;
}

static int exfat_zero_out_metadata(exfat_blk_info *bd,
        exfat_user_config *ui)
{
    uint32_t start_sec;
    unsigned long long total_sec = 0;
    char *buf;
    uint32_t sec_per_clu;
    unsigned long long size;
    int ret = 0;

    if (ui->quick)
        size = minfo.root_byte_off + ui->cluster_size;
    else
        size = bd->size;

    total_sec = size / bd->sector_size;

    buf = malloc(ui->cluster_size);

    if (buf == NULL)
        return -1;

    memset(buf, 0, ui->cluster_size);

    start_sec = 0;
    sec_per_clu = ui->sec_per_clu;

    while (start_sec < total_sec) {
        ret = disk_write(bd, start_sec, sec_per_clu, buf);
        if (ret < 0) {
            dprintf(INFO, "Error: zero out metadata failed. \n");
            break;
        }
        start_sec += sec_per_clu;
    }

    free(buf);
    dprintf(INFO, "zero out sectors : %llu, disk total sectors : %llu\n",
        total_sec, bd->num_sectors);
    return ret;
}

static int exfat_mkfs(exfat_blk_info *bd, exfat_user_config *ui)
{

    dprintf(INFO, "Creating exFAT filesystem(%s, cluster size=%u)\n",
        ui->dev_name, ui->cluster_size);

    dprintf(INFO, "Writing volume boot record: \n");
    if (exfat_create_volume_boot_record(bd, ui, 0) < 0) {
        dprintf(INFO, "Error: Writing volume boot record failed\n");
        return -1;
    }

    dprintf(INFO, "Writing backup volume boot record: \n");
    /* backup sector */
    if (exfat_create_volume_boot_record(bd, ui, 1) < 0) {
        dprintf(INFO, "Error: Writing backup volume boot record failed\n");
        return -1;
    }

    dprintf(INFO, "Fat table creation: \n");
    if (exfat_create_fat_table(bd, ui) < 0) {
        dprintf(INFO, "Error: Fat table creation failed\n");
        return -1;
    }

    dprintf(INFO, "Allocation bitmap creation: \n");
    if (exfat_create_bitmap(bd) < 0) {
        dprintf(INFO, "Error: Allocation bitmap creation failed\n");
        return -1;
    }

    dprintf(INFO, "Upcase table creation: \n");
    if (exfat_create_upcase_table(bd) < 0) {
        dprintf(INFO, "Upcase table creation failed\n");
        return -1;
    }

    dprintf(INFO, "Writing root directory entry: \n");
    if (exfat_create_root_dir(bd, ui) < 0) {
        dprintf(INFO, "Writing root directory entry failed\n");
        return -1;
    }

    return 0;
}

static void exfat_init_user_config(exfat_user_config *ui, disk_partition_t *part_info)
{
    memset(ui, 0, sizeof(exfat_user_config));
    ui->boundary_align = DEFAULT_BOUNDARY_ALIGNMENT;
    ui->cluster_size = DEFAULT_CLUSTER_SIZE;
    ui->sector_size = DEFAULT_SECTOR_SIZE;
    memset(ui->dev_name, 0, sizeof(ui->dev_name));
    memcpy(ui->dev_name, part_info->part_name, sizeof(part_info->part_name));
    ui->pack_bitmap = false;
    ui->quick = true;
    ui->sec_per_clu = DEFAULT_CLUSTER_SIZE / DEFAULT_SECTOR_SIZE;
    ui->volume_label_len = DEFAULT_VOLUME_LABEL_LEN;
    ui->volume_serial = get_new_serial();
    ui->writeable = true;
}

static void exfat_get_dev_info(exfat_user_config *ui, exfat_blk_info *bd, disk_partition_t *part_info)
{

    bd->offset = (uint64_t)part_info->start_blk;
    bd->size = (uint64_t)part_info->blk_cnt * (uint64_t)part_info->blksz;

    bd->sector_size = (uint64_t)part_info->blksz;
    bd->sector_size_bits = size_to_bits(bd->sector_size);
    bd->byte_offset = bd->offset * bd->sector_size;
    bd->num_sectors = bd->size / ui->sector_size;
    bd->num_clusters = bd->size / ui->cluster_size;

    dprintf(INFO, "Block device name : %s\n", ui->dev_name);
    dprintf(INFO, "Block device sec offset : %llu\n", bd->offset);
    dprintf(INFO, "Block device byte offset : %llu\n", bd->byte_offset);
    dprintf(INFO, "Block device size : %llu\n", bd->size);
    dprintf(INFO, "Block sector size : %u\n", bd->sector_size);
    dprintf(INFO, "Number of the sectors : %llu\n", bd->num_sectors);
    dprintf(INFO, "Number of the clusters : %u\n", bd->num_clusters);
}

status_t exfat_format(block_dev_desc_t *dev, const void *args, disk_partition_t *part_info)
{
    status_t result;
    fscookie *cookie;

    if (exfat_mount(dev, &cookie, part_info) < 0) {
        dprintf(INFO, "Error: get exfat super block failed,\n");
        return ERR_IO;
    }
    g_exfat = (exfat_fs_t *)cookie;

    bd.dev = dev;

    dprintf(INFO, "Start format .... .\n");

    exfat_init_user_config(&ui, part_info);

    exfat_get_dev_info(&ui, &bd, part_info);

    if (exfat_build_info(&bd, &ui) < 0) {
        dprintf(INFO, "Error: build device info failed!\n");
        result = ERR_GENERIC;
        goto exit;
    }

    if (exfat_zero_out_metadata(&bd, &ui) < 0) {
        dprintf(INFO, "Error: zero out metadata failed!\n");
        result = ERR_GENERIC;
        goto exit;
    }

    if (exfat_mkfs(&bd, &ui) < 0) {
        dprintf(INFO, "Error: mkfs failed!\n");
        result = ERR_GENERIC;
        goto exit;
    }

    result = NO_ERROR;
    dprintf(INFO,"File system created successfully.\n");

exit:
    free_exfat(g_exfat);
    return result;
}
