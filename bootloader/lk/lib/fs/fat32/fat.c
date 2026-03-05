/*
 * Copyright (c) 2015 Steve White
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <lk/err.h>
//#include <lib/bio.h>
#include <lib/fs.h>
#include <lk/trace.h>
#include <lk/debug.h>
#include <malloc.h>
#include <string.h>
#include <endian.h>
#include <part.h>
#include <sprd_common_rw.h>

#include "fat_format.h"
#include "fat32_priv.h"
#include "fat_fs.h"

#define LOCAL_TRACE 0

int disk_read(fat_fs_t *fat, uint32_t block, uint32_t nr_blocks, void *buf) {
    int ret = 0;

    if (!fat->dev || !fat->dev->block_read)
        return -1;

    ret = fat->dev->block_read(fat->dev->dev_num, fat->part_start+block, nr_blocks, buf);
    if (ret == 0) {
        errorf("mmc_bread Error!!!\n");
        ret = -1;
    }
    return ret;
}

int disk_write(fat_fs_t *fat, uint32_t block, uint32_t nr_blocks, void *buf) {
    int ret = 0;

    if (!fat->dev || !fat->dev->block_write)
        return -1;

    ret = fat->dev->block_write(fat->dev->dev_num, fat->part_start+block, nr_blocks, buf);
    if (ret == 0) {
        errorf("mmc_bwrite Error!!!\n");
        ret = -1;
    }
    return ret;
}

void fat32_dump(fat_fs_t *fat) {
    dprintf(INFO,"bytes_per_sector=%i\n", fat->bytes_per_sector);
    dprintf(INFO,"sectors_per_cluster=%i\n", fat->sectors_per_cluster);
    dprintf(INFO,"bytes_per_cluster=%i\n", fat->bytes_per_cluster);
    dprintf(INFO,"reserved_sectors=%i\n", fat->reserved_sectors);
    dprintf(INFO,"fat_bits=%i\n", fat->fat_bits);
    dprintf(INFO,"fat_count=%i\n", fat->fat_count);
    dprintf(INFO,"sectors_per_fat=%i\n", fat->sectors_per_fat);
    dprintf(INFO,"total_sectors=%i\n", fat->total_sectors);
    dprintf(INFO,"active_fat=%i\n", fat->active_fat);
    dprintf(INFO,"data_start=%i\n", fat->data_start);
    dprintf(INFO,"total_clusters=%i\n", fat->total_clusters);
    dprintf(INFO,"root_cluster=%i\n", fat->root_cluster);
    dprintf(INFO,"root_entries=%i\n", fat->root_entries);
    dprintf(INFO,"root_start=%i\n", fat->root_start);
    dprintf(INFO,"free_cluster=%i\n", fat->free_cluster);

}

status_t fat32_mount(block_dev_desc_t *dev, fscookie **cookie, disk_partition_t *part_info) {
    status_t result = NO_ERROR;

    if (!dev)
        return ERR_NOT_VALID;

    fat_fs_t *fat = malloc(sizeof(fat_fs_t));
    fat->total_part_size = (uint64_t)part_info->blk_cnt;
    fat->part_start = (uint64_t)part_info->start_blk;
    fat->part_block_size = (uint64_t)part_info->blksz;
    fat->dev = dev;

    int err;
    uint8_t *bs = malloc(512);
    err = disk_read(fat, 0, 1, bs);

    if (err < 0) {
        result = ERR_GENERIC;
        goto end;
    }

    if (((bs[0x1fe] != 0x55) || (bs[0x1ff] != 0xaa)) && (bs[0x15] == 0xf8)) {
        dprintf(INFO,"missing boot signature\n");
        result = ERR_NOT_VALID;
        goto end;
    }

    fat->lba_start = 0;

    fat->bytes_per_sector = fat_read16(bs,0xb);
    if ((fat->bytes_per_sector != 0x200) && (fat->bytes_per_sector != 0x400) && (fat->bytes_per_sector != 0x800)) {
        dprintf(INFO,"unsupported sector size (%x)\n", fat->bytes_per_sector);
        result = ERR_NOT_VALID;
        goto end;
    }

    fat->sectors_per_cluster = bs[0xd];
    switch (fat->sectors_per_cluster) {
        case 1:
        case 2:
        case 4:
        case 8:
        case 0x10:
        case 0x20:
        case 0x40:
        case 0x80:
            break;
        default:
            dprintf(INFO,"unsupported sectors/cluster (%x)\n", fat->sectors_per_cluster);
            result = ERR_NOT_VALID;
            goto end;
    }

    fat->reserved_sectors = fat_read16(bs, 0xe);
    fat->fat_count = bs[0x10];

    if ((fat->fat_count == 0) || (fat->fat_count > 8)) {
        dprintf(INFO,"unreasonable FAT count (%x)\n", fat->fat_count);
        result = ERR_NOT_VALID;
        goto end;
    }

    /*if (bs[0x15] != 0xf8) {
        dprintf(INFO,"unsupported media descriptor byte (%x)\n", bs[0x15]);
        result = ERR_NOT_VALID;
        goto end;
    }*/

    fat->sectors_per_fat = fat_read16(bs, 0x16);
    if (fat->sectors_per_fat == 0) {
        fat->fat_bits = 32;
        fat->sectors_per_fat = fat_read32(bs,0x24);
        fat->total_sectors = fat_read32(bs,0x20);
        fat->active_fat = (bs[0x28] & 0x80) ? 0 : (bs[0x28] & 0xf);
        fat->data_start = fat->reserved_sectors + (fat->fat_count * fat->sectors_per_fat);
        fat->total_clusters = (fat->total_sectors - fat->data_start) / fat->sectors_per_cluster;

        // In FAT32, root directory appears in data area on given cluster and can be a cluster chain.
        fat->root_cluster = fat_read32(bs,0x2c);
        fat->root_start = 0;
        if (fat->root_cluster >= fat->total_clusters) {
            dprintf(INFO,"root cluster too large (%x > %x)\n", fat->root_cluster, fat->total_clusters);
            result = ERR_NOT_VALID;
            goto end;
        }
        fat->root_entries = 0;
    } else {
        if (fat->fat_count != 2) {
            dprintf(INFO,"illegal FAT count (%x)\n", fat->fat_count);
            result = ERR_NOT_VALID;
            goto end;
        }

        // On a FAT 12 or FAT 16 volumes the root directory is at a fixed position immediately after the File Allocation Tables
        fat->root_cluster = 0;
        fat->root_entries = fat_read16(bs,0x11);
        if (fat->root_entries % (fat->bytes_per_sector / 0x20)) {
            dprintf(INFO,"illegal number of root entries (%x)\n", fat->root_entries);
            result = ERR_NOT_VALID;
            goto end;
        }

        fat->total_sectors = fat_read16(bs,0x13);
        if (fat->total_sectors == 0) {
            fat->total_sectors = fat_read32(bs,0x20);
        }

        fat->root_start = fat->reserved_sectors + fat->fat_count * fat->sectors_per_fat;
        fat->data_start = fat->root_start + fat->root_entries * 0x20 / fat->bytes_per_sector;
        fat->total_clusters = (fat->total_sectors - fat->data_start) / fat->sectors_per_cluster;

        if (fat->total_clusters < 0xff2) {
            dprintf(INFO,"small FAT12, not supported\n");
            result = ERR_NOT_VALID;
            goto end;
        }
        fat->fat_bits = 16;
    }

    fat->bytes_per_cluster = fat->sectors_per_cluster * fat->bytes_per_sector;
    fat->cache = bcache_create(fat->dev, fat->bytes_per_sector, 4, fat->part_start);
    *cookie = (fscookie *)fat;

    uint32_t sec_div = fat->sectors_per_fat / 4;
    uint32_t sec_rem = fat->sectors_per_fat % 4;
    uint32_t bnum;
    uint32_t free_cluster = 0;
    uint32_t *fatable = malloc(4 * fat->bytes_per_sector);
    uint32_t redun_cluster = fat->sectors_per_fat * 128 - fat->total_clusters - 2;
    if (fat->fat_bits == 32) {
        for (int i = 0; i < sec_div; i++) {
            bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + i*4);
            disk_read(fat, bnum, 4, fatable);
            for (int j = 0; j < 128 * 4; j++)
                if (fatable[j] == 0)
                    free_cluster++;
        }
    }

    if (sec_rem != 0) {
        bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + sec_div * 4);
        disk_read(fat, bnum, sec_rem, fatable);
        for (int j = 0; j < 128 * sec_rem; j++)
            if (fatable[j] == 0)
                free_cluster++;
    }
    // the last FAT table sector may use part of it, so there should delet redundant cluster
    fat->free_cluster = free_cluster - redun_cluster;
    free(fatable);
    fat32_dump(fat);

end:
    free(bs);
    return result;
}

status_t fat32_unmount(fscookie *cookie) {
    fat_fs_t *fat = (fat_fs_t *)cookie;
    bcache_destroy(fat->cache);
    free(fat);
    return NO_ERROR;
}

/*
 *construct a FAT16 or FAT32 file system
 */
status_t fat_format(block_dev_desc_t *dev, const void *args,
                                disk_partition_t *part_info)
{
        LTRACEF("dev %p, args %p\n", dev, args);

        if (!dev)
            return ERR_INVALID_ARGS;

        u_int8_t *block;        /*store 0 sector(DBR) content*/
        u_int8_t *img;
        struct bs *bs;
        struct bsbpb *bsbpb;
        struct bsxbpb *bsxbpb;
        struct bsx *bsx;
        struct boot_sector boot_sec;
        struct bpb bpb;
        u_int opt_F = 0, opt_A = 0;       /*default fat version is FAT32*/
        const char * opt_O = NULL;
        u_int fat, sector_num, root_de_sectors, add_sectors,
                temp_spf, extra_res, set_res, set_spf, set_spc, tempx,
                cls, dir, lsn, offset_bytes,
                alignment = 0, attempts = 0;
        long delta;
        int write_ret;
        char *buf;

        opt_A = 1;
        opt_F = 32;
        opt_O = "android";

        block = memalign(ARCH_DMA_MINALIGN, 512);
        if (block == NULL) {
                errorf("Error: allocating block\n");
                return -1;
        }

        if (dev->block_read(dev->dev_num, part_info->start_blk, 1, block) <= 0) {
                errorf("Error: reading block\n");
                free(block);
                return -1;
        }

        memcpy(&boot_sec, block, sizeof(boot_sec));
        memset(&bpb, 0, sizeof(bpb));
        free(block);

        bpb.bytes_per_sector = (boot_sec.sector_size[1] << 8) + boot_sec.sector_size[0];
        bpb.sectors_per_track = LE16SWAP(boot_sec.secs_track);
        bpb.drive_heads_count = LE16SWAP(boot_sec.heads);
        bpb.total_sectors32 = LE32SWAP(boot_sec.total_sect);

        if (!powerof2(bpb.bytes_per_sector) || bpb.bytes_per_sector < MINBPS) {
                errorf("bytes per sector %u is not a power of 2 or too small, minimum is %u",
                        bpb.bytes_per_sector, MINBPS);
                return -1;
        }

        long long temptotal = bpb.total_sectors32;

        if (temptotal * bpb.bytes_per_sector <= (128*1024*1024LL)) {
                opt_F = 16;
                bpb.sectors_per_cluster = 32;
        } else if (temptotal * bpb.bytes_per_sector <= (2*1024*1024*1024LL)) {
                opt_F = 16;
                bpb.sectors_per_cluster = 64;
        }

        delta = bpb.total_sectors32 % bpb.sectors_per_track;
        if (delta != 0) {
                dprintf(INFO,"adjust total sectors, trim %d sectors from %u, so it is a multiple of %d\n",
                        (int)delta, bpb.total_sectors32, bpb.sectors_per_track);
                bpb.total_sectors32 -= delta;
        }

        if (bpb.sectors_per_cluster == 0) {      /*FAT32 set default*/
                if (bpb.total_sectors32 <= 6000)        /*about 3MB*/
                        bpb.sectors_per_cluster = 1;    /*cluster 512 bytes*/
                else if (bpb.total_sectors32 <= (1<<17))        /*64MB*/
                        bpb.sectors_per_cluster = 8;            /*cluster 4KB*/
                else if (bpb.total_sectors32 <= (1<<19))        /*256MB*/
                        bpb.sectors_per_cluster = 16;           /*cluster 8KB*/
                else if (bpb.total_sectors32 <= (1<<22))        /*2GB*/
                        bpb.sectors_per_cluster = 32;           /*cluster 16KB*/
                else
                        bpb.sectors_per_cluster = 64;           /*the size of cluster must no more than 32KB*/
        }

        if (!bpb.fat_count)
                bpb.fat_count = 2;

        fat = opt_F;
        sector_num = 1; /*1 sector is FSINFO*/
        if (fat == 32) {
                if (!bpb.info_sector_num) {
                        if (sector_num == MAXU16 || sector_num == bpb.backup_boot_sector_num) {
                                errorf("no room for info sector\n");
                                return -1;
                        }
                        bpb.info_sector_num = sector_num;
                }
                if (bpb.info_sector_num != MAXU16 && sector_num <= bpb.info_sector_num)
                        sector_num = bpb.info_sector_num + 1;

                if (!bpb.backup_boot_sector_num) {
                        if (sector_num == MAXU16) {
                                errorf("no room for backup sector\n");
                                return -1;
                        }
                        bpb.backup_boot_sector_num = sector_num;
                } else if (bpb.backup_boot_sector_num != MAXU16 &&
                                bpb.backup_boot_sector_num == bpb.info_sector_num) {
                        errorf("backup sector would overwrite info sector\n");
                        return -1;
                }
                if (bpb.backup_boot_sector_num != MAXU16 &&
                        sector_num <= bpb.backup_boot_sector_num)
                        sector_num = bpb.backup_boot_sector_num + 1;
        }

        extra_res = 0;
        set_res = !bpb.reserved_sectors;
        set_spf = !bpb.big_sectors_per_FAT;
        set_spc = !bpb.sectors_per_cluster;

        /*
         *Attempt to align if opt_A is set. This is done by increasing the number of reserved sectors.
         *This can cause other factors to change, which can in turn change the alignment.
         *This should take at most 2 iterations, as increasing the reserved amount
         *may cause the FAT size to decrease by 1, requiring another fat_count reserved sectors.
         *If sectors_per_cluster changes, it will be half of its previous size,
         *and thus will not throw off alignment.
         */
        do {
                if (set_res)
                        bpb.reserved_sectors = (fat == 32 ?
                                MAX(sector_num, MAX(16384 / bpb.bytes_per_sector, 4)) : sector_num)
                                + extra_res;
                else if (bpb.reserved_sectors < sector_num) {
                        errorf("too few reserved sectors\n");
                        return -1;
                }

                if (fat != 32 && !bpb.root_entries_fat16)
                        bpb.root_entries_fat16 = DEFRDE;
                root_de_sectors = howmany(bpb.root_entries_fat16,
                                        bpb.bytes_per_sector / sizeof(struct de));

                if (set_spc)
                        for (bpb.sectors_per_cluster =
                                howmany(fat == 16 ? DEFBLK16 : DEFBLK, bpb.bytes_per_sector);
                                bpb.sectors_per_cluster < MAXSPC &&
                                bpb.reserved_sectors +
                                howmany((RESFTE + maxcls(fat)) * (fat / BPN),
                                        bpb.bytes_per_sector * NPB) * bpb.fat_count +
                                root_de_sectors +
                                (u_int64_t)(maxcls(fat) + 1) *
                                bpb.sectors_per_cluster <= bpb.total_sectors32;
                                bpb.sectors_per_cluster <<= 1);

                if (fat != 32 && bpb.big_sectors_per_FAT > MAXU16) {
                        errorf("too many sectors per fat for FAT16\n");
                        return -1;
                }

                add_sectors = bpb.reserved_sectors + root_de_sectors;
                tempx = bpb.big_sectors_per_FAT ? bpb.big_sectors_per_FAT : 1;
                if (add_sectors + (u_int64_t)tempx * bpb.fat_count > bpb.total_sectors32) {
                        errorf("meta data exceeds file system size\n");
                        return -1;
                }

                add_sectors += tempx * bpb.fat_count;
                tempx = (u_int64_t)(bpb.total_sectors32 - add_sectors) * bpb.bytes_per_sector * NPB /
                        (bpb.sectors_per_cluster * bpb.bytes_per_sector * NPB +
                        fat / BPN * bpb.fat_count);

                temp_spf = howmany((RESFTE + MIN(tempx, maxcls(fat))) * (fat / BPN),
                                bpb.bytes_per_sector * NPB);
                if (set_spf) {
                        if (!bpb.big_sectors_per_FAT)
                                bpb.big_sectors_per_FAT = temp_spf;
                        add_sectors += (bpb.big_sectors_per_FAT - 1) * bpb.fat_count;
                }

                if (set_res) {
                        alignment = (bpb.reserved_sectors + bpb.big_sectors_per_FAT * bpb.fat_count) \
                                % bpb.sectors_per_cluster;
                        extra_res += bpb.sectors_per_cluster - alignment;
                }

                attempts++;
        } while (opt_A && alignment != 0 && attempts < 2);
        if (alignment != 0)
                dprintf(INFO,"warning: Alignment failed.\n");

        cls = (bpb.total_sectors32 - add_sectors) / bpb.sectors_per_cluster;
        tempx = (u_int64_t)bpb.big_sectors_per_FAT * bpb.bytes_per_sector *
                        NPB / (fat / BPN) - RESFTE;
        if (cls > tempx)
                cls = tempx;

        if (bpb.big_sectors_per_FAT < temp_spf)
                dprintf(INFO,"warning: sectors per FAT limits file system to %u clusters\n", cls);

        if (cls < mincls(fat)) {
                errorf("%u clusters too few for FAT%u, need %u\n", cls, fat, mincls(fat));
                return -1;
        }
        if (cls > maxcls(fat)) {
                cls = maxcls(fat);
                bpb.total_sectors32 = add_sectors + (cls + 1) * bpb.sectors_per_cluster - 1;
                dprintf(INFO,"warning: FAT type limits file system to %u sectors\n", bpb.total_sectors32);
        }
        dprintf(INFO,"%u sector%s in %u FAT%u cluster%s (%u bytes/cluster)\n",
                cls * bpb.sectors_per_cluster, cls * bpb.sectors_per_cluster == 1 ? "" : "s",
                cls, fat, cls == 1 ? "" : "s", bpb.bytes_per_sector * bpb.sectors_per_cluster);

        if (!bpb.media_descriptor)
                bpb.media_descriptor = !bpb.hidden_sectors ? 0xf0 : 0xf8;

        if (fat ==32)
                bpb.root_cluster = RESFTE;

        if (bpb.hidden_sectors + bpb.total_sectors32 <= MAXU16) {
                bpb.total_sectors16 = bpb.total_sectors32;
                bpb.total_sectors32 = 0;
        }

        if (fat != 32) {
                bpb.small_sectors_per_FAT = bpb.big_sectors_per_FAT;
                bpb.big_sectors_per_FAT = 0;
        }

        print_bpb(&bpb);

        if (!(img = malloc(bpb.bytes_per_sector))) {
                errorf("img malloc %u failed\n", bpb.bytes_per_sector);
                return -1;
        }
        buf = malloc(MAXPATHLEN);
        memset(buf, 0, MAXPATHLEN);
        dir = bpb.reserved_sectors + (bpb.small_sectors_per_FAT ?
                bpb.small_sectors_per_FAT : bpb.big_sectors_per_FAT) * bpb.fat_count;
        for (lsn = 0; lsn < dir + (fat ==32 ? bpb.sectors_per_cluster : root_de_sectors); lsn++) {
                sector_num = lsn;
                memset(img, 0, bpb.bytes_per_sector);
                if (!lsn || (fat == 32 && bpb.backup_boot_sector_num != MAXU16
                                && lsn == bpb.backup_boot_sector_num)) {
                        offset_bytes = sizeof(struct bs);
                        bsbpb = (struct bsbpb *)(img + offset_bytes);
                        mk2(bsbpb->bytes_per_sector, bpb.bytes_per_sector);
                        mk1(bsbpb->sectors_per_cluster, bpb.sectors_per_cluster);
                        mk2(bsbpb->reserved_sectors, bpb.reserved_sectors);
                        mk1(bsbpb->fat_count, bpb.fat_count);
                        mk2(bsbpb->root_entries_fat16, bpb.root_entries_fat16);
                        mk2(bsbpb->total_sectors16, bpb.total_sectors16);
                        mk1(bsbpb->media_descriptor, bpb.media_descriptor);
                        mk2(bsbpb->small_sectors_per_FAT, bpb.small_sectors_per_FAT);
                        mk2(bsbpb->sectors_per_track, bpb.sectors_per_track);
                        mk2(bsbpb->drive_heads_count, bpb.drive_heads_count);
                        mk4(bsbpb->hidden_sectors, bpb.hidden_sectors);
                        mk4(bsbpb->total_sectors32, bpb.total_sectors32);
                        offset_bytes += sizeof(struct bsbpb);

                        if (fat ==32) {
                                bsxbpb = (struct bsxbpb *)(img + offset_bytes);
                                mk4(bsxbpb->big_sectors_per_FAT, bpb.big_sectors_per_FAT);
                                mk2(bsxbpb->flags, 0);
                                mk2(bsxbpb->filesystem_version, 0);
                                mk4(bsxbpb->root_cluster, bpb.root_cluster);
                                mk2(bsxbpb->info_sector_num, bpb.info_sector_num);
                                mk2(bsxbpb->backup_boot_sector_num, bpb.backup_boot_sector_num);
                                offset_bytes += sizeof(struct bsxbpb);
                        }

                        bsx = (struct bsx *)(img + offset_bytes);
                        mk1(bsx->extend_boot_signature, 0x29);

                        tempx = (((u_int)(1 + 10) << 8 | (u_int)22) + (u_int)(1900 + 87));
                        mk4(bsx->volumeID, tempx);
                        mklabel(bsx->volume_label, "NO NAME");
                        sprintf(buf, "FAT%u", fat);
                        setstr(bsx->filesystem_type, buf, sizeof(bsx->filesystem_type));

                        offset_bytes += sizeof(struct bsx);
                        bs = (struct bs *)img;
                        mk1(bs->jmp[0], 0xeb);
                        mk1(bs->jmp[1], offset_bytes - 2);
                        mk1(bs->jmp[2], 0x90);
                        setstr(bs->oem, opt_O ? opt_O : "BSD 4.4", sizeof(bs->oem));

                        memcpy(img + offset_bytes, bootcode, sizeof(bootcode));
                        mk2(img + MINBPS - 2, DOSMAGIC);
                } else if (fat == 32 && bpb.info_sector_num != MAXU16 &&
                                (lsn == bpb.info_sector_num ||
                                        (bpb.backup_boot_sector_num != MAXU16 &&
                                        lsn == bpb.backup_boot_sector_num + bpb.info_sector_num))) {
                        mk4(img, 0x41615252);
                        mk4(img + MINBPS - 28, 0x61417272);
                        mk4(img + MINBPS - 24, 0xffffffff);
                        mk4(img + MINBPS - 20, bpb.root_cluster);
                        mk2(img + MINBPS -2, DOSMAGIC);
                } else if (lsn >= bpb.reserved_sectors && lsn < dir &&
                        !((lsn - bpb.reserved_sectors) %
                        (bpb.small_sectors_per_FAT ? bpb.small_sectors_per_FAT : bpb.big_sectors_per_FAT))) {
                        mk1(img[0], bpb.media_descriptor);

                        for (tempx = 1; tempx < fat * (fat == 32 ? 3 : 2) / 8; tempx++)
                                mk1(img[tempx], fat == 32 && tempx % 4 == 3 ? 0x0f : 0xff);
                }

                write_ret = dev->block_write(dev->dev_num, part_info->start_blk + lsn, 1, img);
                if (write_ret != 1) {
                        errorf("can't write sector %u, ret is %d\n", lsn, write_ret);
                        free(img);
                        free(buf);
                        return -1;
                }
        }
        free(img);
        free(buf);
        return 0;
}

status_t fat32_stat_fs(fscookie *cookie, struct fs_stat *stat) {
    fat_fs_t *fat = (fat_fs_t *)cookie;
    uint64_t temp = fat->free_cluster;
    stat->free_space = temp * fat->bytes_per_cluster;
    temp = fat->total_sectors;
    stat->total_space = temp * fat->bytes_per_sector;
    stat->free_inodes = fat->free_cluster;
    stat->total_inodes = fat->total_sectors / fat->sectors_per_cluster;
    return 0;
}

static const struct fs_api fat32_api = {
    .format = fat_format,
    .fs_stat = fat32_stat_fs,
    .mount = fat32_mount,
    .unmount = fat32_unmount,
    .open = fat32_open_file,
    .create = fat32_create_file,
    .stat = fat32_stat_file,
    .read = fat32_read_file,
    .write = fat32_write_file,
    .rename = fat32_rename,
    .close = fat32_close_file,
    .remove = fat_remove,
    .opendir = fat_open_dir,
    .mkdir = fat_make_dir,
    .readdir = fat_read_dir,
    .closedir = fat_close_dir,
};

STATIC_FS_IMPL(fat32, &fat32_api);
