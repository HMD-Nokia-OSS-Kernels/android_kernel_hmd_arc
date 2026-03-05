/*
 * Copyright (c) 2015 Steve White
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <lk/err.h>
#include <lib/fs.h>
#include <lk/trace.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <lk/debug.h>
#include <part.h>
#include <sprd_rtc_def.h>

#include "fat_fs.h"
#include "fat32_priv.h"

#define DIR_ENTRY_LENGTH 32
#define USE_CACHE 1
#define LOCAL_TRACE 0

static inline uint32_t div_cnt(size_t dividend, uint32_t divisor) {
    uint32_t cnt = dividend / divisor;
    uint32_t remainder = dividend % divisor;
    if (remainder)
        cnt++;

    return cnt;
}

int filename_len(const char *s) {
    int i=0;
    while (*(s++)!='\0') {
        i++;
        if (*s=='/')
            break;
    }
    return i;
}

/* FAT-LFN: Calculate checksum of an SFN entry */
static uint8_t sum_sfn (const char* dir) {
    unsigned char sum = 0;
    uint n = 11;

    do {
        sum = (sum >> 1) + (sum << 7) + *dir++;
    } while (--n);
    return sum;
}

static int get_sname (char* s_filename, char* filename, int name_length) {
    int namelen = 0;
    int extlen = 0;

    while (filename[namelen++] != '.') {
        if (namelen > name_length)
            break;
    }
    int temp = namelen;
    namelen--;
    if (temp <= name_length) {
        while (filename[temp++] != '\0')
            extlen ++;
    }
    for (int slen = 0; slen < 8; slen ++) {
        if (slen < namelen) {
            s_filename[slen] = filename[slen];
        } else {
            s_filename[slen] = 0x20;
        }
    }
    namelen++;
    for (int elen = 0; elen < 3; elen ++) {
        if (elen < extlen) {
            s_filename[8 + elen] = filename[namelen + elen];
        } else {
            s_filename[8 + elen] = 0x20;
        }
    }
    return --namelen;
}


/* Create Short name */
static void gen_sname (char* dst, const char* src, const char* lfn, uint seq) {
    char ns[8], c;
    int i, j;
    char wc;
    ulong sr;

    memcpy(dst, src, 11);
    if (seq > 5) {
    /* In case of many collisions, generate a hash number instead of sequential number */
        sr = seq;
        while (*lfn) {
        /* Create a CRC */
            wc = *lfn++;
            for (i = 0; i < 16; i++) {
                sr = (sr << 1) + (wc & 1);
                wc >>= 1;
                if (sr & 0x10000) sr ^= 0x11021;
            }
        }
        seq = (int)sr;
    }

    /* itoa (hexdecimal) */
    i = 7;
    do {
        c = (unsigned char)((seq % 16) + '0');
        if (c > '9') c += 7;
        ns[i--] = c;
        seq /= 16;
    } while (seq);
    ns[i] = '~';

    /* Append the number */
    for (j = 0; j < i && dst[j] != ' '; j++) {
        if (j == i - 1) break;
        j++;
    }
    do {
        dst[j++] = (i < 8) ? ns[i++] : ' ';
    } while (j < 8);
}

static void set_long_entry(long_dir_entry *ldentptr, int lentrynum, char *filename, uint8_t chksum) {
    int nameloop = 0;
    int nameptr = 0;
    int temp;

    for (int lnum = 0; lnum < lentrynum; lnum++) {
        ldentptr[lnum].chksum = chksum;
        ldentptr[lnum].lattr = fat_attribute_lfn;
        ldentptr[lnum].LDIR_ord = lnum+1;

        for (nameloop = 0; nameloop < 10; nameloop++) {
            ldentptr[lnum].name0[nameloop] = filename[nameptr];
            nameptr++;
            nameloop++;
            if (filename[nameptr] == '\0') {
                nameloop += 3;
                for (temp = nameloop; temp < 10; temp++) {
                    ldentptr[lnum].name0[temp] = 0xff;
                }
                if (nameloop >= 10) {
                    nameloop = 2;
                } else {
                    nameloop = 0;
                }
                for (; nameloop < 12; nameloop++) {
                    ldentptr[lnum].name1[nameloop] = 0xff;
                }
                for (nameloop = 0; nameloop < 4; nameloop++) {
                    ldentptr[lnum].name2[nameloop] = 0xff;
                }
                return;
            }
        }
        for (nameloop = 0; nameloop < 12; nameloop++) {
            ldentptr[lnum].name1[nameloop] = filename[nameptr];
            nameptr++;
            nameloop++;
            if (filename[nameptr] == '\0') {
                nameloop += 3;
                for (temp = nameloop; temp < 12; temp++) {
                    ldentptr[lnum].name1[temp] = 0xff;
                }
                if (nameloop >= 12) {
                    nameloop = 2;
                } else {
                    nameloop = 0;
                }
                for (; nameloop < 4; nameloop++) {
                    ldentptr[lnum].name2[nameloop] = 0xff;
                }
                return;
            }
        }
        for (nameloop = 0; nameloop < 4; nameloop++) {
            ldentptr[lnum].name2[nameloop] = filename[nameptr];
            nameptr++;
            nameloop++;
            if (filename[nameptr] == '\0') {
                nameloop += 3;
                for (temp = nameloop; temp < 4; temp++) {
                    ldentptr[lnum].name2[temp] = 0xff;
                }
                return;
            }
        }
    }
    return;
}

extern struct rtc_time get_time_by_sec(void);

static void set_entry_time(short_dir_entry *dirent) {
    struct rtc_time tm;
    int  mod_time;

    tm = get_time_by_sec();
    mod_time = ((tm.tm_year - 1980) << 9) | (tm.tm_mon << 5) | tm.tm_mday;
    dirent->cdate  = LE16SWAP(mod_time);
    dirent->adate = LE16SWAP(mod_time);
    mod_time = (tm.tm_hour << 11) | (tm.tm_min << 5) |  (tm.tm_sec / 2);
    dirent->ctime = LE16SWAP(mod_time);
    dirent->date = dirent->cdate;
    dirent->time = dirent->ctime;
    return;
}

static inline off_t fat32_offset_for_cluster(fat_fs_t *fat, uint32_t cluster) {
    off_t cluster_begin_lba = fat->reserved_sectors + (fat->fat_count * fat->sectors_per_fat);
    return fat->lba_start + (cluster_begin_lba + (cluster - 2) * fat->sectors_per_cluster) * fat->bytes_per_sector;
}

static inline uint32_t fat32_offset_sector(fat_fs_t *fat, uint32_t cluster) {
    off_t cluster_begin_lba = fat->reserved_sectors + (fat->fat_count * fat->sectors_per_fat);
    return fat->lba_start / fat->bytes_per_sector + cluster_begin_lba + (cluster - 2) * fat->sectors_per_cluster;
}

/* Check whether adding a file makes the file system to exceed the size of the block device */
static int check_overflow(fat_fs_t *fat, uint32_t clustnum, size_t size) {
    uint32_t startsect, sect_num;
    if (clustnum > 2) {
        startsect = fat->data_start + (clustnum - 2) * fat->sectors_per_cluster;
    } else {
        startsect = fat->data_start;
    }

    sect_num = div_cnt(size, fat->bytes_per_sector);
    if (startsect + sect_num > fat->total_sectors) {
        errorf("fat: check_overflow clustnum=%d\n", clustnum);
        return -1;
    }
    return 0;
}

/* cluster != 0:find empty cluster, not flush bcache
   cluster = 0:find first empty cluster, flush bcache */
uint32_t  find_empty_cluster(fat_fs_t *fat, uint32_t cluster, bool flag) {
    uint32_t empty_cluster;
    uint32_t start_cluster = cluster;
    uint32_t fatsec = 0;
    uint8_t *bbuf;

    for (fatsec = (start_cluster) >> 7; fatsec < fat->sectors_per_fat; fatsec++ ) {
        uint32_t bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fatsec);

#if USE_CACHE
        void *cache_ptr;
        if (bcache_get_block(fat->cache, &cache_ptr, bnum) < 0) {
            dprintf(INFO,"fat: bcache_get_block fail\n");
            return 0;
        } else {
            if (fat->fat_bits == 32) {
                uint32_t *table = (uint32_t *)cache_ptr;
                for (uint32_t index = (start_cluster) & 127; index < 128; index++) {
                    if (table[index] == 0) {
                        uint32_t mms=fatsec << 7;
                        empty_cluster = mms + index;
                        if (check_overflow(fat, empty_cluster, fat->bytes_per_cluster) < 0) {
                                return 0xffffffff;
                        }
                        table[index] = 0x0fffffff;
                        bcache_mark_block_dirty(fat->cache, bnum);
                        bcache_put_block(fat->cache, bnum);
                        if (flag == true) {
                            /* find first empty cluster flush bcache */
                            if (bcache_flush(fat->cache) < 0) {
                                dprintf(INFO,"fat: bcache_flush fail\n");
                                return 0;
                            }
                        }
                        bbuf = malloc(fat->bytes_per_cluster);
                        if (bbuf == NULL)
                            return 0;
                        memset(bbuf,0,fat->bytes_per_cluster);
                        uint32_t lba_addr = fat32_offset_sector(fat, empty_cluster);
                        int err = disk_write(fat, lba_addr, fat->sectors_per_cluster, bbuf);
                        free(bbuf);
                        if (err < 0) {
                            dprintf(INFO,"fat: disk write new cluster fail, empty_cluster %d\n", empty_cluster);
                            return 0;
                        }
                        return empty_cluster;
                    }
                }
                bcache_put_block(fat->cache, bnum);
                start_cluster = 0;
            }
        }
#else
        uint32_t *fatable = malloc(512);
        disk_read(fat, bnum, 1, &fatable);
        for (int index = (start_cluster) & 127; index < 128; index ++) {
            if (fatable[index] == 0) {
                empty_cluster = fatsec << 7 + index;
                fatable[index] = 0xfffffff0;
                if (disk_write(fat, bnum, 1, &fatable) < 0)
                    empty_cluster = 0xffffffff;
                free(fatable);
                return empty_cluster;
            }
        }
        start_cluster = 0;
        free(fatable);
#endif
    }
    errorf("fat: There are no free clusters\n");
    return 0xffffffff;
}


/* stretch a chain or creat a new one, cluster must be the last one */
uint32_t fat32_creat_chain(fat_fs_t *fat, uint32_t cluster, uint32_t cnt) {
    uint32_t bnum = 0;
    uint32_t fat_sector = 0;
    uint32_t fat_index = 0;
    uint32_t next_cluster = 0x0fffffff;

    if (cnt <= 1) {
        return next_cluster;
    }
#if !USE_CACHE
    fat_sector = (cluster) >> 7;
    fat_index = (cluster) & 127;
    bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fat_sector);
    uint32_t *fatable = malloc(512);
    disk_read(fat, bnum, 1, &fatable);
#endif

    do {
        fat_sector = (cluster) >> 7;
        fat_index = (cluster) & 127;
        bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fat_sector);
        next_cluster = find_empty_cluster(fat, cluster, false);
        if (next_cluster == 0xffffffff) {
            next_cluster = find_empty_cluster(fat, 0, false);
            if (next_cluster == 0xffffffff) {
                errorf("fat: 1 There are no free clusters\n");
                return 0xffffffff;
            } else if (next_cluster == 0) {
                errorf("fat: 1 clusters chain error!!!\n");
                return 0;
            }
        } else if (next_cluster == 0) {
                errorf("fat: 2 clusters chain error!!!\n");
                return 0;
        }

#if USE_CACHE
        void *cache_ptr;
        if (bcache_get_block(fat->cache, &cache_ptr, bnum) < 0) {
            errorf("fat: bcache_get_block fail\n");
        } else {
            if (fat->fat_bits == 32) {
                uint32_t *table = (uint32_t *)cache_ptr;
                table[fat_index] = LE32SWAP(next_cluster);
                bcache_mark_block_dirty(fat->cache, bnum);
                bcache_put_block(fat->cache, bnum);
                if ((next_cluster) >> 7 != fat_sector) {
                    if (bcache_flush(fat->cache) < 0) {
                        errorf("fat: bcache_flush fail\n");
                    }
                }
                cluster = next_cluster;
                cnt--;
                if (cnt == 1) {
                    if (bcache_flush(fat->cache) < 0) {
                        errorf("fat: bcache_flush fail\n");
                    }
                    return next_cluster;
                }
            }
        }
        #else
        fatable[fat_index] = LE32SWAP(next_cluster);
        cluster = next_cluster;
        if ((next_cluster) >> 7 != fat_sector) {
            if (disk_write(fat, bnum, 1, &fatable) < 0)
                errorf("fat: disk_write error");
            fat_sector = (cluster) >> 7;
            fat_index = (cluster) & 127;
            bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fat_sector);
            disk_read(fat, bnum, 1, &fatable);
        }
        cnt--;
        if (cnt == 1) {
            free(fatable);
            return next_cluster;
        }
#endif
    } while (true);

    return 0;
}


uint32_t fat32_next_cluster_in_chain(fat_fs_t *fat, uint32_t cluster) {
    uint32_t fat_sector = (cluster) >> 7;
    uint32_t fat_index = (cluster ) & 127;

    uint32_t bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fat_sector);
    uint32_t next_cluster = 0x0fffffff;

#if USE_CACHE
    void *cache_ptr;
    int err = bcache_get_block(fat->cache, &cache_ptr, bnum);
    if (err < 0) {
        next_cluster = 0xffffffff;
        errorf("fat: bcache_get_block fail\n");
    } else {
        if (fat->fat_bits == 32) {
            uint32_t *table = (uint32_t *)cache_ptr;
            next_cluster = table[fat_index];
            LE32SWAP(next_cluster);
        } else if (fat->fat_bits == 16) {
            uint16_t *table = (uint16_t *)cache_ptr;
            next_cluster = table[fat_index];
            LE16SWAP(next_cluster);
            if (next_cluster > 0xfff0) {
                next_cluster |= 0x0fff0000;
            }
        }

        bcache_put_block(fat->cache, bnum);
    }
#else
    uint32_t *fatable = malloc(512);
    //suint32_t offset = (bnum * fat->bytes_per_sector) + (fat_index * (fat->fat_bits / 8));
    //bio_read(fat->dev, &next_cluster, offset, 4);
    disk_read(fat, bnum, 1, fatable);
    next_cluster = fatable[fat_index];
    LE32SWAP(next_cluster);
    free(fatable);
#endif
    return next_cluster;
}

int fat32_del_chain(fat_fs_t *fat, uint32_t cluster) {
    uint32_t bnum = 0;
    uint32_t fat_sector = 0;
    uint32_t fat_index = 0;
    uint32_t next_cluster = 0x0fffffff;
    int err = 0;
    if (cluster == 0) {
        return err;
    }

#if !USE_CACHE
    fat_sector = (cluster) >> 7;
    fat_index = (cluster) & 127;
    bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fat_sector);
    uint32_t *fatable = malloc(512);
    disk_read(fat, bnum, 1, &fatable);
#endif

    do{
        fat_sector = (cluster) >> 7;
        fat_index = (cluster) & 127;
        bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fat_sector);

#if USE_CACHE
        void *cache_ptr;
        if (bcache_get_block(fat->cache, &cache_ptr, bnum) < 0) {
            errorf("fat: bcache_get_block fail\n");
            err = -1;
            break;
        }
        uint32_t *table = (uint32_t *)cache_ptr;
        next_cluster = fat32_next_cluster_in_chain(fat, cluster);
        if (next_cluster == 0xffffffff) {
            err = -1;
            break;
        }
        table[fat_index] = 0;
        bcache_mark_block_dirty(fat->cache, bnum);
        bcache_put_block(fat->cache, bnum);
        fat->free_cluster ++;
        if ((next_cluster) >> 7 != fat_sector) {
            if (bcache_flush(fat->cache) < 0) {
               errorf("fat: bcache_flush fail\n");
               err = -1;
               break;
            }
        }
        cluster = next_cluster;
#else
        next_cluster = fat32_next_cluster_in_chain(fat, cluster);
        fatable[fat_index] = 0;
        if ((next_cluster) >> 7 != fat_sector) {
            if (disk_write(fat, bnum, 1, &fatable) < 0)
                errorf("fat: disk_write error\n");
            fat_sector = (cluster) >> 7;
            fat_index = (cluster) & 127;
            bnum = (fat->lba_start / fat->bytes_per_sector) + (fat->reserved_sectors + fat_sector);
            disk_read(fat, bnum, 1, &fatable);
        }
        cluster = next_cluster;
#endif
    }while (cluster < 0x0ffffff8);

#if USE_CACHE
    //bcache_put_block(fat->cache, bnum);
    if (bcache_flush(fat->cache) < 0) {
        err = -1;
        errorf("fat: bcache_flush fail\n");
    }
#else
    free(fatable);
#endif

    return err;
}


char *fat32_dir_get_filename(uint8_t *dir, off_t offset, int lfn_sequences) {
    int result_len = 1 + (lfn_sequences == 0 ? 12 : (lfn_sequences * 26));
    char *result;
    int j = 0;

    result = malloc(result_len);
    if (result == NULL)
        return NULL;
    memset(result, 0x00, result_len);

    if (lfn_sequences == 0) {
        // Ignore trailing spaces in filename and/or extension
        int fn_len=8, ext_len=3;
        for (int i=7; i>=0; i--) {
            if (dir[offset + i] == 0x20) {
                fn_len--;
            } else {
                break;
            }
        }
        for (int i=10; i>=8; i--) {
            if (dir[offset + i] == 0x20) {
                ext_len--;
            } else {
                break;
            }
        }

        for (int i=0; i<fn_len; i++) {
            result[j++] = dir[offset + i];
        }
        if (ext_len > 0) {
            result[j++] = '.';
            for (int i=0; i<ext_len; i++) {
                result[j++] = dir[offset + 8 + i];
            }
        }
    } else {
        // XXX: not unicode aware.
        for (int sequence=1; sequence<=lfn_sequences; sequence++) {
            for (int i=1; i<DIR_ENTRY_LENGTH; i++) {
                int char_offset = (offset - (sequence * DIR_ENTRY_LENGTH)) + i;
                if (dir[char_offset] != 0x00 && dir[char_offset] != 0xff) {
                    result[j++] = dir[char_offset];
                }

                if (i == 10) {
                    i = 13;
                } else if (i == 25) {
                    i = 27;
                }
            }
        }
    }
    return result;
}

static int find_entry_byname(fat_fs_t *fat, uint8_t *dir, const char *name_ptr, uint32_t *dir_cluster, fat_dircluster_info 
*dircluster_info, int num) {
    uint32_t offset = 0;
    uint32_t lfn_sequences = 0;
    uint32_t first_cluster = 0;
    uint32_t last_cluster = 0;
    int empty_cnt = 0;
    bool matched = false;
    int err = 0;

    first_cluster = *dir_cluster;

    do{
        err = disk_read(fat, fat32_offset_sector(fat, *dir_cluster), fat->sectors_per_cluster, dir);
        if (err < 0) {
            err = -1;
            return err;
        }

        while (dir[offset] != 0x00 && offset < fat->bytes_per_cluster) {
            if ( dir[offset] == 0xE5 /*deleted*/) {
                offset += DIR_ENTRY_LENGTH;
                continue;
            } else if ((dir[offset + 0x0B] & 0x08)) {
                if (dir[offset + 0x0B] == 0x0f) {
                    lfn_sequences++;
                }
                offset += DIR_ENTRY_LENGTH;
                continue;
            }

            char *filename;
            filename = fat32_dir_get_filename(dir, offset, lfn_sequences);
            if (filename == NULL)
                return -1;

            if (strlen(filename) == strlen(name_ptr)) {
                matched = (strnicmp(name_ptr, filename, strlen(filename)) == 0);
            }
            free(filename);

            if (matched) {
                uint32_t target_cluster = fat_read16(dir, offset + 0x1a);
                target_cluster += (uint16_t)fat_read16(dir, offset + 0x14) << 16;
                dircluster_info->attr = dir[offset + 0x0b];
                dircluster_info->cluster = *dir_cluster;
                dircluster_info->offset = offset;
                dircluster_info->lfn_sequences = lfn_sequences;
                *dir_cluster = target_cluster;
                err = 1;
                return err;
            }
            lfn_sequences = 0;
            offset += DIR_ENTRY_LENGTH;
        }

        if (num !=0 ) {
            offset = 0;
            while (offset < fat->bytes_per_cluster) {
                if ( dir[offset] == 0xE5 || dir[offset] == 0x00) {
                    empty_cnt++;
                    offset += DIR_ENTRY_LENGTH;
                    if (empty_cnt == num) {
                        dircluster_info->cluster = *dir_cluster;
                        dircluster_info->offset = offset - (num * DIR_ENTRY_LENGTH);
                        num = 0;
                        break;
                    }
                   continue;
                }
                empty_cnt = 0;
                offset += DIR_ENTRY_LENGTH;
            }
        }
        last_cluster = *dir_cluster;
        err = 0;
        *dir_cluster = fat32_next_cluster_in_chain(fat, *dir_cluster);
        if (*dir_cluster == 0xffffffff) {
            err = -1;
            return err;
        }
    }while (*dir_cluster < 0x0ffffff8);

    if ((num !=0) && (err == 0)) {
     // need apply a new cluster
        dircluster_info->offset = 0;
        err = fat32_creat_chain(fat, last_cluster, 2);
        if (err == 0) {
            err = -1;
            return err;
        } else if (err == 0xffffffff) {
            errorf("fat: no empty clusters on disk\n");
            return err;
        }
        dircluster_info->cluster = fat32_next_cluster_in_chain(fat, last_cluster);
    }

    *dir_cluster = first_cluster;
    return err;
}

static int add_folder_faultext(fat_fs_t *fat, uint8_t *dir, uint32_t dir_cluster, uint32_t father_cluster, bool isroot) {
    int err = 0;
    off_t lba_addr = 0;
    memset(dir,0,fat->bytes_per_cluster);
    short_dir_entry *sdentptr;
    short_dir_entry *sdent;
    sdentptr = malloc(sizeof(short_dir_entry));
    if (sdentptr == NULL)
        return -1;
    sdent = malloc(sizeof(short_dir_entry));
    if (sdent == NULL) {
        free(sdentptr);
        return -1;
    }
    memcpy(sdentptr->name, ".          ", 11);
    set_entry_time(sdentptr);
    uint16_t temp1 = (uint16_t)(dir_cluster&0x0000ffff);
    uint16_t temp2 = (uint16_t)(dir_cluster >> 16);
    sdentptr->start = LE16SWAP(temp1);
    sdentptr->starthi = LE16SWAP(temp2);
    sdentptr->size = 0;
    sdentptr->attr = fat_attribute_directory;

    memcpy(sdent->name, "..         ", 11);
    set_entry_time(sdent);
    if (isroot) {
        sdent->start = 0;
        sdent->starthi = 0;
    } else{
        uint16_t temp1 = (uint16_t)(father_cluster & 0x0000ffff);
        uint16_t temp2 = (uint16_t)(father_cluster >> 16);
        sdent->start = LE16SWAP(temp1);
        sdent->starthi = LE16SWAP(temp2);
    }
    sdent->size = 0;
    sdent->attr = fat_attribute_directory;

    memcpy(dir, sdentptr, DIR_ENTRY_LENGTH);
    uint8_t *tdir=dir + 32;
    memcpy(tdir, sdent, DIR_ENTRY_LENGTH);

    lba_addr = fat32_offset_sector(fat, dir_cluster);
    err = disk_write(fat, lba_addr, fat->sectors_per_cluster, dir);
    free(sdentptr);
    free(sdent);

    return err;
}


static int del_entry(fat_fs_t *fat, uint8_t *dir, uint32_t dir_cluster, uint32_t offset, int lnum) {
    dir[offset] = 0xE5;
    for (;lnum > 0; lnum--)
        dir[offset - lnum*DIR_ENTRY_LENGTH] = 0xE5;

    return disk_write(fat, fat32_offset_sector(fat, dir_cluster), fat->sectors_per_cluster, dir);
}


static int del_folder(fat_fs_t *fat, uint8_t *dir, uint32_t cluster, uint32_t dir_cluster, uint32_t offset, int lnum) {
    uint32_t lfn_sequences = 0;
    uint32_t first_cluster = cluster;
    bool notempty = true;

    /* del file entry in father dir */
    if (del_entry(fat, dir, dir_cluster, offset, lnum) < 0)
        return -1;

    offset = 0;
    do {
        off_t lba_addr = fat32_offset_sector(fat, cluster);
        int err = disk_read(fat, lba_addr, fat->sectors_per_cluster, dir);
        if (err < 0)
            return -1;

        while (dir[offset] != 0x00 && offset < fat->bytes_per_cluster) {
            if ( dir[offset] == 0xE5 /*deleted*/ ||  dir[offset] == 0x2E /*./..*/) {
                offset += DIR_ENTRY_LENGTH;
                continue;
            } else if ((dir[offset + 0x0B] & 0x08)) {
                if (dir[offset + 0x0B] == 0x0f) {
                    lfn_sequences++;
                }
                offset += DIR_ENTRY_LENGTH;
                continue;
            }
            notempty = false;
            uint32_t target_cluster = fat_read16(dir, offset + 0x1a);
            target_cluster += (uint16_t)fat_read16(dir, offset + 0x14) << 16;

            if (dir[0x0B + offset] == fat_attribute_archive) {
                dir[offset] = 0xE5;
                for (;lfn_sequences > 0; lfn_sequences--)
                    dir[offset - lfn_sequences*DIR_ENTRY_LENGTH] = 0xE5;

                if (fat32_del_chain(fat, target_cluster) < 0 )
                    return -1;
            } else {
                del_folder(fat, dir, target_cluster, cluster, offset, lfn_sequences);
            }
            lfn_sequences = 0;
            offset += DIR_ENTRY_LENGTH;
        }
        if (notempty == false) {
            if (disk_write(fat, lba_addr, fat->sectors_per_cluster, dir) < 0)
                return -1;
        }

        cluster = fat32_next_cluster_in_chain(fat, cluster);
        if (cluster == 0xffffffff)
            return -1;
    }while (cluster < 0x0ffffff8);

    /* del cluster chain in fat table */
    if (fat32_del_chain(fat, first_cluster) < 0) {
        return -1;
    }

    return 0;
}

uint32_t write_cluster(fat_fs_t *fat, const void *buf, uint32_t cluster, uint32_t length) {
    uint32_t amount_write = 0;
    do {
           off_t lba_addr = fat32_offset_sector(fat, cluster);

           uint32_t to_write = fat->bytes_per_cluster;
           uint32_t next_cluster = 0;
           while ((next_cluster = fat32_next_cluster_in_chain(fat, cluster)) == cluster + 1) {
               cluster = next_cluster;
               to_write += fat->bytes_per_cluster;
           }
           cluster = next_cluster;
           to_write = MIN(length - amount_write, to_write);
           int err = disk_write(fat, lba_addr, div_cnt(to_write, fat->bytes_per_sector), buf+amount_write);
           if (err < 0) {
               errorf("fat: writing: to_write=0x%x\n",to_write);
               return err;
           }
           amount_write += to_write;
           if (amount_write >= length) {
               dprintf(INFO,"fat: finished write, amount_write=%i, to_write=%i\n", amount_write, to_write);
               return amount_write;
           }
    } while (amount_write < length);
    return -1;
}


status_t fat32_create_fileordir(fscookie *cookie, const char *path, filecookie **fcookie, uint64_t len, bool flag) {
    fat_fs_t *fat = (fat_fs_t *)cookie;
    status_t result = ERR_GENERIC;
    uint8_t *dir;
    char filename[128] = {0};
    uint32_t dir_cluster = fat->root_cluster;
    int name_length = 0;
    int entry_num;
    fat_file_t *file = NULL;
    fat_dircluster_info *dircluster_info;

    dir = malloc(fat->bytes_per_cluster);
    if (dir == NULL)
        return result;
    dircluster_info = malloc(sizeof(fat_dircluster_info));
    if (dircluster_info == NULL) {
        free(dir);
        return result;
    }
    dircluster_info->cluster = 0;
    dircluster_info->offset = 0;
    dircluster_info->attr = 0;
    bool f_isroot = true;

    const char *ptr;
    /* chew up leading slashes */
    ptr = &path[0];
    while (*ptr == '/')
        ptr++;

    bool done = false;
    bool folder_is_new = false;
    uint32_t last_cluster = fat->root_cluster;
    uint32_t temp_cluster;
    do {
        char *next_sep = strchr(ptr, '/');
        name_length = filename_len(ptr);
        strncpy(filename, ptr, (size_t)name_length);
        filename[name_length] = '\0';
        entry_num = 1 + (name_length < 12 ? 1 : div_cnt(name_length, 13));

        if (next_sep) {
            /* terminate the next component, giving us a substring */
            *next_sep = 0;
        } else {
            /* this is the last component */
            done = true;
        }

        int matched = 0;
        temp_cluster = dir_cluster;
        if (folder_is_new == false) {
            matched = find_entry_byname(fat, dir, filename, &dir_cluster, dircluster_info, entry_num);
            if ((done != true) && (dircluster_info->attr == fat_attribute_archive))
                matched = false;
        }

        if (matched == true) {
            if (done == true) {
                result = ERR_ALREADY_EXISTS;
                break;
            } else {
                /* move to the next separator */
                ptr = next_sep + 1;
                /* consume multiple separators */
                while (*ptr == '/') {
                    ptr++;
                }
                last_cluster = temp_cluster;
            }
        } else if (matched == false) {
            // New file or file path does not exist
            char s_filename[11] = {0};
            uint32_t dir_offset = 0;
            uint32_t fatherdir_cluster = 0;
            off_t lba_addr = 0;
            int err = 0;

            int lentrynum = entry_num - 1;
            int tempoff = lentrynum - 1;
            uint8_t *dentry_img;
            uint8_t *long_ptr;
            short_dir_entry *sdentptr;
            dentry_img = malloc(entry_num * DIR_ENTRY_LENGTH);
            if (dentry_img == NULL)
                break;

            long_ptr = malloc(sizeof(long_dir_entry) * lentrynum);
            if (long_ptr == NULL) {
                free(dentry_img);
                break;
            }

            long_dir_entry *ldentptr = (long_dir_entry *)long_ptr;
            memset(long_ptr, 0, sizeof(long_dir_entry) * lentrynum);

            sdentptr = malloc(sizeof(short_dir_entry));
            if (sdentptr == NULL) {
                free(dentry_img);
                free(long_ptr);
                break;
            }
            /* create short entry img */
            int namlen = get_sname(s_filename, filename, name_length);
            int res = 0;
            if (namlen <= 8) {
                memcpy(sdentptr->name, s_filename, 11);
            } else {
                for (int n = 1; n < 100; n++) {
                    gen_sname(sdentptr->name, s_filename, filename, n);
                    if (folder_is_new == false)
                        res = find_entry_byname(fat, dir, sdentptr->name, &dir_cluster, dircluster_info, entry_num);

                    if (!res) break;
                }
            }
            set_entry_time(sdentptr);

            /* create long entry img */
            /* Checksum value of the SFN tied to the LFN */
            uint8_t chksum = sum_sfn(sdentptr->name);

            set_long_entry(ldentptr, lentrynum, filename, chksum);

            ldentptr[tempoff].LDIR_ord |= 0x40;

            /* find entry_num empty entry in fatherdir cluster */
            fatherdir_cluster = dircluster_info->cluster;
            dir_offset = dircluster_info->offset;

            /* find empty cluster for new file or folder */
            if (!((done == true) && (flag == true) && (len == 0))) {
                dir_cluster = find_empty_cluster(fat, fatherdir_cluster, true);
                if (dir_cluster == 0xffffffff) {
                    dir_cluster = find_empty_cluster(fat, 0, true);
                    if (dir_cluster == 0xffffffff) {
                        errorf("fat mkdir: There are no free clusters\n");
                        free(dentry_img);
                        free(ldentptr);
                        free(sdentptr);
                        result = ERR_GENERIC;
                        break;
                    }
                } else if (dir_cluster == 0) {
                    errorf("fat: clusters chain error!!!\n");
                    free(dentry_img);
                    free(ldentptr);
                    free(sdentptr);
                    break;
                }

                uint16_t temp1 = (uint16_t)(dir_cluster&0x0000ffff);
                uint16_t temp2 = (uint16_t)(dir_cluster >> 16);
                sdentptr->start = LE16SWAP(temp1);
                sdentptr->starthi = LE16SWAP(temp2);

                /* if folder add . and ..*/
                if (flag == false || done != true) {
                    if (last_cluster == fat->root_cluster)
                        f_isroot = true;

                    if (add_folder_faultext(fat, dir, dir_cluster, fatherdir_cluster, f_isroot) < 0) {
                        free(dentry_img);
                        free(ldentptr);
                        free(sdentptr);
                        err = result;
                        break;
                    }
                }
            }

            for (int off = tempoff; off >= 0; off--)
                memcpy(dentry_img+(tempoff-off)*DIR_ENTRY_LENGTH, long_ptr+off*DIR_ENTRY_LENGTH, DIR_ENTRY_LENGTH);

            lba_addr = fat32_offset_sector(fat, fatherdir_cluster);
            err = disk_read(fat, lba_addr, fat->sectors_per_cluster, dir);
            if (err < 0) {
                free(dentry_img);
                free(ldentptr);
                free(sdentptr);
                err = result;
                break;
            }

            /* put short and long entry img*/
            if (done == true) {
                if (flag == true) {
                    if (len == 0) {
                        sdentptr->start = 0;
                        sdentptr->starthi = 0;
                        dir_cluster = 0;
                    } else {
                        uint32_t cnt = div_cnt(len, fat->bytes_per_cluster);
                        uint32_t ret = fat32_creat_chain(fat, dir_cluster, cnt);
                        if (ret == 0) {
                            free(dentry_img);
                            free(ldentptr);
                            free(sdentptr);
                            break;
                        } else if (ret == 0xffffffff) {
                            errorf("fat: no empty clusters on disk\n");
                            free(dentry_img);
                            free(ldentptr);
                            free(sdentptr);
                            result = ERR_NO_MEMORY;
                            break;
                        }
                    }
                    uint32_t temlen = (uint32_t)len;
                    sdentptr->size = LE32SWAP(temlen);
                    sdentptr->attr = fat_attribute_archive;
                } else {
                    sdentptr->size = 0;
                    sdentptr->attr = fat_attribute_directory;
                }
                memcpy(dentry_img + (lentrynum * DIR_ENTRY_LENGTH), sdentptr, DIR_ENTRY_LENGTH);

                uint8_t *tdir=dir + dir_offset;
                memcpy(tdir, dentry_img, (entry_num * DIR_ENTRY_LENGTH));
                err = disk_write(fat, lba_addr, fat->sectors_per_cluster, dir);
                free(dentry_img);
                free(ldentptr);
                free(sdentptr);
                if (err < 0) {
                    err = result;
                    break;
                }
                if (flag == true) {
                    file = malloc(sizeof(fat_file_t));
                    if (file == NULL)
                        break;

                    file->fat_fs = fat;
                    file->start_cluster = dir_cluster;
                    file->length = len;
                    file->attributes = fat_attribute_archive;
                    file->father_cluster = fatherdir_cluster;
                    file->fclus_offset = dir_offset + (lentrynum * DIR_ENTRY_LENGTH) ;
                }
                result = NO_ERROR;
                break;
            } else {
                sdentptr->size = 0;
                sdentptr->attr = fat_attribute_directory;
                memcpy(dentry_img + (lentrynum * DIR_ENTRY_LENGTH), sdentptr, DIR_ENTRY_LENGTH);
                uint8_t *tdir = dir + dir_offset;
                memcpy(tdir, dentry_img, (entry_num * DIR_ENTRY_LENGTH));
                err = disk_write(fat, lba_addr, fat->sectors_per_cluster, dir);
                free(dentry_img);
                free(ldentptr);
                free(sdentptr);
                if (err < 0) {
                    err = result;
                    break;
                }
            }

            dircluster_info->cluster = dir_cluster;
            /* offset . .. */
            dircluster_info->offset = 64;
            /* move to the next separator */
            ptr = next_sep + 1;
            while (*ptr == '/')
                ptr++;

            folder_is_new = true;
            last_cluster = temp_cluster;
        }else {
            break;
        }
    } while (true);

out:
    if (flag == true)
        *fcookie = (filecookie *)file;

    free(dircluster_info);
    free(dir);
    return result;
}

status_t fat32_create_file(fscookie *cookie, const char *path, filecookie **fcookie, uint64_t len) {
    status_t result = fat32_create_fileordir(cookie, path, fcookie, len, true);
    return result;
}

status_t fat32_open_file(fscookie *cookie, const char *path, filecookie **fcookie) {
    fat_fs_t *fat = (fat_fs_t *)cookie;
    status_t result = ERR_GENERIC;

    uint8_t *dir = malloc(fat->bytes_per_cluster);
    uint32_t dir_cluster = fat->root_cluster;
    fat_file_t *file = NULL;

    const char *ptr;
    /* chew up leading slashes */
    ptr = &path[0];
    while (*ptr == '/')
        ptr++;

    bool done = false;
    do {
        // XXX: use the cache!
        //bio_read(fat->dev, dir, fat32_offset_for_cluster(fat, dir_cluster), fat->bytes_per_cluster);
        disk_read(fat, fat32_offset_sector(fat, dir_cluster), fat->sectors_per_cluster, dir);

        char *next_sep = strchr(ptr, '/');
        if (next_sep) {
            /* terminate the next component, giving us a substring */
            *next_sep = 0;
        } else {
            /* this is the last component */
            done = true;
        }

        uint32_t offset = 0;
        uint32_t lfn_sequences = 0;
        bool matched = false;
        while (dir[offset] != 0x00 && offset < fat->bytes_per_cluster) {
            if ( dir[offset] == 0xE5 /*deleted*/) {
                offset += DIR_ENTRY_LENGTH;
                continue;
            } else if ((dir[offset + 0x0B] & 0x08)) {
                if (dir[offset + 0x0B] == 0x0f)
                    lfn_sequences++;

                offset += DIR_ENTRY_LENGTH;
                continue;
            }

            char *filename;
            filename = fat32_dir_get_filename(dir, offset, lfn_sequences);
            if (filename == NULL)
                break;

            lfn_sequences = 0;

            matched = (strnicmp(ptr, filename, strlen(filename)) == 0);
            free(filename);

            if (matched) {
                uint16_t target_cluster = fat_read16(dir, offset + 0x1a);
                if (done == true) {
                    file = malloc(sizeof(fat_file_t));
                    file->fat_fs = fat;
                    file->start_cluster = target_cluster;
                    file->father_cluster = dir_cluster;
                    file->fclus_offset = offset ;
                    file->length = fat_read32(dir, offset + 0x1c);
                    file->attributes = dir[0x0B + offset];
                    result = NO_ERROR;
                } else {
                    dir_cluster = target_cluster;
                }
                break;
            }
            offset += DIR_ENTRY_LENGTH;
        }

        if (matched == true) {
            if (done == true) {
                break;
            } else {
                /* move to the next separator */
                ptr = next_sep + 1;

                /* consume multiple separators */
                while (*ptr == '/') {
                    ptr++;
                }
            }
        } else {
            // XXX: untested!!!
            dir_cluster = fat32_next_cluster_in_chain(fat, dir_cluster);
            if (dir_cluster >= 0x0ffffff8)
                // no more clusters in the chain
                break;
        }
    } while (true);

out:
    *fcookie = (filecookie *)file;
    free(dir);
    return result;
}

ssize_t fat32_read_file(filecookie *fcookie, void *buf, off_t offset, size_t len) {
    fat_file_t *file = (fat_file_t *)fcookie;
    fat_fs_t *fat = file->fat_fs;
    uint32_t cluster = 0;
    if (offset <= fat->bytes_per_cluster) {
        cluster = file->start_cluster;
    } else {
        // XXX: support non-0 offsets
        TRACE;
        return -1;
    }

    uint32_t length = file->length;
    uint32_t amount_read = 0;

    do {
        //off_t lba_addr = fat32_offset_for_cluster(fat, cluster);
        off_t lba_addr = fat32_offset_sector(fat, cluster);

        uint32_t to_read = fat->bytes_per_cluster;
        uint32_t next_cluster = 0;
        while ((next_cluster = fat32_next_cluster_in_chain(fat, cluster)) == cluster + 1) {
            cluster = next_cluster;
            to_read += fat->bytes_per_cluster;
        }
        cluster = next_cluster;

        to_read = MIN(length - amount_read, to_read);
        // XXX: support non-0 offsets
        //int err = bio_read(dev, buf+amount_read, lba_addr, to_read);
        int err = disk_read(fat, lba_addr, div_cnt(to_read, fat->bytes_per_sector), buf+amount_read);
        if (err < 0)
            return err;

        amount_read += to_read;

        if (amount_read < length) {
            if (cluster == 0x0fffffff) {
                errorf("fat: no more clusters, amount_read=%i, to_read=%i\n", amount_read, to_read);
                break;
            }
        }
    } while (amount_read < length);

    return amount_read;
}


ssize_t fat32_write_file(filecookie *fcookie, const void *buf, off_t offset, size_t len) {
    fat_file_t *file = (fat_file_t *)fcookie;
    fat_fs_t *fat = file->fat_fs;
    ssize_t result = 0;
    uint32_t first_cluster = file->start_cluster;
    uint32_t length = file->length;
    uint32_t all_cnt = 0;
    off_t lba_addr ;

    uint8_t *bbuf;
    bbuf = malloc(fat->bytes_per_cluster);
    if (bbuf == NULL)
        return result;

    memset(bbuf,0,fat->bytes_per_cluster);

    if (check_overflow(fat, first_cluster, len) < 0) {
        free(bbuf);
        return -1;
    }
    if (first_cluster == 0) {
        first_cluster = find_empty_cluster(fat, file->father_cluster, true);
        if (first_cluster == 0xffffffff) {
            first_cluster = find_empty_cluster(fat, 0, true);
            if (first_cluster == 0xffffffff) {
                errorf("fat write: There are no free clusters\n");
                free(bbuf);
                return -1;
            }
        } else if (first_cluster == 0) {
            errorf("fat: clusters chain error!!!\n");
            free(bbuf);
            return -1;
        }
    }  ///first cluster

    dprintf(INFO,"fat: in write file, len=0x%x, length=0x%x\n",len,length);

    all_cnt = div_cnt(len, fat->bytes_per_cluster);
    uint32_t ret = fat32_creat_chain(fat, first_cluster, all_cnt);

    if (ret == 0) {
        free(bbuf);
        return ret;
    } else if (ret == 0xffffffff) {
        errorf("fat: no empty clusters on disk\n");
        free(bbuf);
        return ret;
    }

    result = write_cluster(fat, buf, first_cluster, len);

//modify entry
    short_dir_entry *sdentptr;
    sdentptr = malloc(sizeof(short_dir_entry));
    if (sdentptr == NULL) {
        free(bbuf);
        return result;
    }
    lba_addr = fat32_offset_sector(fat, file->father_cluster);
    disk_read(fat, lba_addr, fat->sectors_per_cluster, bbuf);
    memcpy(sdentptr, bbuf+file->fclus_offset, DIR_ENTRY_LENGTH);

    set_entry_time(sdentptr);
    sdentptr->size = LE32SWAP(len);

    uint16_t temp1 = (uint16_t)(first_cluster&0x0000ffff);
    uint16_t temp2 = (uint16_t)(first_cluster >> 16);
    sdentptr->start = LE16SWAP(temp1);
    sdentptr->starthi = LE16SWAP(temp2);

    memcpy(bbuf+file->fclus_offset, sdentptr, DIR_ENTRY_LENGTH);

    if (disk_write(fat, lba_addr, fat->sectors_per_cluster, bbuf) < 0)
        errorf("fat: disk_write error\n");

    file->length = sdentptr->size;
    free(sdentptr);

    free(bbuf);
    return result;
}


status_t fat32_close_file(filecookie *fcookie) {
    fat_file_t *file = (fat_file_t *)fcookie;
    free(file);
    return NO_ERROR;
}

status_t fat32_stat_file(filecookie *fcookie, struct file_stat *stat) {
    fat_file_t *file = (fat_file_t *)fcookie;
    stat->size = file->length;
    stat->is_dir = (file->attributes == fat_attribute_directory);
    return NO_ERROR;
}

status_t fat32_rename(fscookie *cookie, const char *scr, const char *dest) {
    fat_fs_t *fat = (fat_fs_t *)cookie;
    status_t result = ERR_GENERIC;
    uint8_t *dir;
    uint32_t dir_cluster = fat->root_cluster;
    int name_length = 0;
    int entry_num;
    fat_dircluster_info *dircluster_info;
    dir = malloc(fat->bytes_per_cluster);
    if (dir ==NULL)
         return result;

    dircluster_info = malloc(sizeof(fat_dircluster_info));
    if (dircluster_info == NULL) {
         free(dir);
         return result;
    }
    dircluster_info->cluster = 0;
    dircluster_info->offset = 0;

    const char *ptr;
    const char *dptr;
    /* chew up leading slashes */
    ptr = &scr[0];
    dptr = &dest[0];
    while (*ptr == '/')
        ptr++;

    bool done = false;
    bool folder_is_new = false;
    do {
        char *next_sep = strchr(ptr, '/');
        if (next_sep) {
            *next_sep = 0;
        } else {
            done = true;
        }
        int matched = false;
        matched = find_entry_byname(fat, dir, ptr, &dir_cluster, dircluster_info, 0);

        if (matched == true) {
            if (done == true) {
                char filename[128] = {0};
                char s_filename[11] = {0};
                uint32_t dir_offset = 0;
                uint32_t fatherdir_cluster = 0;
                off_t lba_addr = 0;
                int err = 0;
                int lentrynum;
                int tempoff;
                short_dir_entry *sdentptr;

                lentrynum = dircluster_info->lfn_sequences;
                del_entry(fat, dir, dircluster_info->cluster, dircluster_info->offset, dircluster_info->lfn_sequences);

                sdentptr = malloc(sizeof(short_dir_entry));
                if (sdentptr == NULL)
                    break;

                memcpy(sdentptr, dir+dircluster_info->offset, DIR_ENTRY_LENGTH);

                name_length = filename_len(dptr);
                entry_num = 1 + (name_length < 12 ? 1 : div_cnt(name_length, 13));
                lentrynum = entry_num - 1;
                tempoff = lentrynum - 1;
                strncpy(filename, dptr, (size_t)name_length);
                filename[name_length] = '\0';

                uint8_t *dentry_img;
                uint8_t *long_ptr;
                dentry_img = malloc(entry_num * DIR_ENTRY_LENGTH);
                if (dentry_img == NULL) {
                    free(sdentptr);
                    break;
                }
                long_ptr = malloc(sizeof(long_dir_entry) * lentrynum);
                if (long_ptr == NULL) {
                    free(dentry_img);
                    free(sdentptr);
                    break;
                }
                memset(long_ptr,0,sizeof(long_dir_entry) * lentrynum);
                long_dir_entry *ldentptr = (long_dir_entry *)long_ptr;

                /* create short entry img */
                int namlen = get_sname(s_filename, filename, name_length);
                int res = 0;
                matched = find_entry_byname(fat, dir, dptr, &dircluster_info->cluster, dircluster_info, entry_num);
                if (matched == true) {
                    result = ERR_ALREADY_EXISTS;
                    free(dentry_img);
                    free(ldentptr);
                    free(sdentptr);
                    break;
                }
                if (namlen <= 8) {
                    memcpy(sdentptr->name, s_filename, 11);
                } else {
                    for (int n = 1; n < 100; n++) {
                        gen_sname(sdentptr->name, s_filename, filename, n);
                        if (folder_is_new == false) {
                            res = find_entry_byname(fat, dir, sdentptr->name, &dircluster_info->cluster, dircluster_info, entry_num);
                        }
                        if (!res) break;
                    }
                }
                set_entry_time(sdentptr);

                /* create long entry img */
                uint8_t chksum = sum_sfn(sdentptr->name);
                set_long_entry(ldentptr, lentrynum, filename, chksum);
                ldentptr[tempoff].LDIR_ord |= 0x40;

                /* find entry_num empty entry in fatherdir cluster */
                fatherdir_cluster = dircluster_info->cluster;
                dir_offset = dircluster_info->offset;   /////////////

                for (int off = tempoff; off >= 0; off--)
                    memcpy(dentry_img+(tempoff-off)*DIR_ENTRY_LENGTH, long_ptr+off*DIR_ENTRY_LENGTH, DIR_ENTRY_LENGTH);

                /* put short and long entry img*/
                memcpy(dentry_img + (lentrynum * DIR_ENTRY_LENGTH), sdentptr, DIR_ENTRY_LENGTH);

                lba_addr = fat32_offset_sector(fat, fatherdir_cluster);
                err = disk_read(fat, lba_addr, fat->sectors_per_cluster, dir);
                if (err < 0) {
                    free(dentry_img);
                    free(ldentptr);
                    free(sdentptr);
                    err = result;
                    break;
                }

                uint8_t *tdir=dir + dir_offset;
                memcpy(tdir, dentry_img, (entry_num * DIR_ENTRY_LENGTH));

                err = disk_write(fat, lba_addr, fat->sectors_per_cluster, dir);

                free(dentry_img);
                free(ldentptr);
                free(sdentptr);
                if (err < 0) {
                    err = result;
                    break;
                }
                result = NO_ERROR;
                break;
            } else {
                ptr = next_sep + 1;
                while (*ptr == '/')
                    ptr++;
            }
        } else {
            // New file or file path does not exist
            errorf("fat: file not exist\n");
            result = ERR_NOT_FOUND;
            break;
        }
    } while (true);
    free(dircluster_info);
    free(dir);
    return result;
}

status_t fat_remove(fscookie *cookie, const char *path) {
    fat_fs_t *fat = (fat_fs_t *)cookie;
    status_t result = ERR_GENERIC;
    uint8_t *dir;
    uint32_t dir_cluster = fat->root_cluster;
    fat_dircluster_info *dircluster_info;

    dir = malloc(fat->bytes_per_cluster);
    if (dir == NULL)
        return result;

    dircluster_info = malloc(sizeof(fat_dircluster_info));
    if (dircluster_info == NULL) {
        free(dir);
        return result;
    }
    dircluster_info->cluster = 0;
    dircluster_info->offset = 0;

    const char *ptr;
    /* chew up leading slashes */
    ptr = &path[0];
    while (*ptr == '/')
        ptr++;

    bool done = false;

    do {
        char *next_sep = strchr(ptr, '/');
        if (next_sep) {
            *next_sep = 0;
        } else {
            done = true;
        }
        int matched = false;
        matched = find_entry_byname(fat, dir, ptr, &dir_cluster, dircluster_info, 0);
        if (matched == true) {
            if (done == true) {
               // name_length = filename_len(ptr);
                //entry_num = 1 + (name_length < 12 ? 1 : div_cnt(name_length, 13));
                //int lfn_sequences = entry_num - 1;
                if (dir[dircluster_info->offset+0x0B] == fat_attribute_archive) {
                    del_entry(fat, dir, dircluster_info->cluster, dircluster_info->offset, dircluster_info->lfn_sequences);
                    if (dir_cluster != 0) {
                        if (fat32_del_chain(fat, dir_cluster) < 0)
                            break;
                    }
                } else {
                    del_folder(fat, dir, dir_cluster, dircluster_info->cluster, dircluster_info->offset, dircluster_info->lfn_sequences);
                }
                result = NO_ERROR;
                break;
            } else {
                ptr = next_sep + 1;
                while (*ptr == '/')
                    ptr++;
            }
        } else {
            // New file or file path does not exist
            result = ERR_NOT_FOUND;
            break;
        }
    } while (true);

    free(dircluster_info);
    free(dir);
    return result;
}


status_t fat_open_dir(fscookie *cookie, const char *path, dircookie **dcookie) {
    fat_fs_t *fat = (fat_fs_t *)cookie;
    status_t result = ERR_GENERIC;
    uint8_t *dirbuf;
    uint32_t dir_cluster = fat->root_cluster;
    fat_dircluster_info *dircluster_info;
    fat_dir_t *dir = NULL;

    dirbuf = malloc(fat->bytes_per_cluster);
    if (dirbuf == NULL)
        return result;

    dircluster_info = malloc(sizeof(fat_dircluster_info));
    if (dircluster_info == NULL) {
        free(dirbuf);
        return result;
    }
    dircluster_info->cluster = 0;
    dircluster_info->offset = 0;
    const char *ptr;
    /* chew up leading slashes */
    ptr = &path[0];
    while (*ptr == '/')
        ptr++;

    bool done = false;
    do {
        char *next_sep = strchr(ptr, '/');
        if (next_sep) {
            /* terminate the next component, giving us a substring */
            *next_sep = 0;
        } else {
            /* this is the last component */
            done = true;
        }

        int matched = false;
        matched = find_entry_byname(fat, dirbuf, ptr, &dir_cluster, dircluster_info, 0);
        if ((matched == true) && (dirbuf[0x0B + dircluster_info->offset] == fat_attribute_archive))
            matched = false;

        if (matched == true) {
            if (done == true) {
                dir = malloc(sizeof(fat_dir_t));
                dir->fat_fs = fat;
                dir->start_cluster = dir_cluster;
                dir->read_offset = 0;
                result = NO_ERROR;
                break;
            } else {
                /* move to the next separator */
                ptr = next_sep + 1;
                /* consume multiple separators */
                while (*ptr == '/')
                    ptr++;
            }
        } else {
            result = ERR_NOT_FOUND;
            break;
        }
    } while (true);

    *dcookie = (dircookie *)dir;
    free(dircluster_info);
    free(dirbuf);
    return result;
}

status_t fat_make_dir(fscookie *cookie, const char *path) {
    status_t result = fat32_create_fileordir(cookie, path, 0, 0, false);
    return result;
}

status_t fat_read_dir(dircookie *dircookie, struct dirent *dirinfo) {
    fat_dir_t *dir = (fat_dir_t *)dircookie;
    fat_fs_t *fat = dir->fat_fs;

    uint32_t cluster = dir->start_cluster;
    uint32_t offset = dir->read_offset;
    status_t result = ERR_GENERIC;

    uint8_t *dirdata;
    dirdata = malloc(fat->bytes_per_cluster);
    if (dirdata == NULL)
        return result;

    int cnt = offset / fat->bytes_per_cluster;
    offset %= fat->bytes_per_cluster;
    while (cnt--) {
        cluster = fat32_next_cluster_in_chain(fat, cluster);
        if (cluster == 0x0fffffff) {
            free(dirdata);
            return result;
        }
        if (cluster == 0xffffffff) {
            free(dirdata);
            return ERR_GENERIC;
        }
    }

    do {
        //off_t lba_addr = fat32_offset_for_cluster(fat, cluster);
        off_t lba_addr = fat32_offset_sector(fat, cluster);
        //bio_read(fat->dev, dirdata, lba_addr + offset, fat->bytes_per_cluster);
        disk_read(fat, lba_addr, fat->sectors_per_cluster, dirdata);

        uint32_t lfn_sequences = 0;
        while (dirdata[offset] != 0x00 && offset < fat->bytes_per_cluster) {
            if ( dirdata[offset] == 0xE5 /*deleted*/) {
                offset += DIR_ENTRY_LENGTH;
                continue;
            } else if ((dirdata[offset + 0x0B] & 0x08)) {
                if (dirdata[offset + 0x0B] == 0x0f) {
                    lfn_sequences++;
                }
                offset += DIR_ENTRY_LENGTH;
                continue;
            }
            char *tempname;
            tempname = fat32_dir_get_filename(dirdata, offset, lfn_sequences);
            if (tempname == NULL)
                break;

            memcpy(dirinfo->name, tempname, strlen(tempname));
            free(tempname);
            dir->start_cluster = cluster;
            dir->read_offset = offset + DIR_ENTRY_LENGTH;
            result = NO_ERROR;
            break;
        }
        cluster = fat32_next_cluster_in_chain(fat, cluster);
        offset = 0;
        if (cluster == 0x0fffffff) {
            // no more clusters in the chain
            break;
        }
        if (cluster == 0xffffffff) {
            free(dirdata);
            return ERR_GENERIC;
        }
    } while (true);
    free(dirdata);
    return result;
}

status_t fat_close_dir(dircookie *dircookie) {
    fat_dir_t *dir = (fat_dir_t *)dircookie;
    free(dir);
    return NO_ERROR;
}


