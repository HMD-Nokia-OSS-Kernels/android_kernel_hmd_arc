/*
 * Copyright (c)
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <ctype.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/types.h>
#include <endian.h>

#define MAXU16  0xffff
#define BPN     4       /*bytes per nibble*/
#define NPB     2       /*nibbles per byte*/

#define MINBPS  512     /*minimum of bytes per sector*/
#define DEFRDE  512     /*default root directory entries*/
#define RESFTE  2       /*reserved FAT entries*/

#define MAXCLS16        0xfff5          /*maximum FAT16 clusters*/
#define MAXCLS32        0xfffffff5      /*maximum FAT132 clusters*/
#define MINCLS16        0x1000          /*minimum FAT16 cluster*/
#define MINCLS32        2               /*minimum FAT32 cluster*/
#define maxcls(fat)     ((fat)==16 ? MAXCLS16 : MAXCLS32)
#define mincls(fat)     ((fat)==16 ? MINCLS16 : MINCLS32)

#define MAXPATHLEN      256
#define DOSMAGIC        0xaa55
#define DEFBLK          4096            /*default block size*/
#define DEFBLK16        2048            /*default block size FAT16*/
#define MAXSPC          128             /*maximum sectors per cluster*/

#define powerof2(x)     (((x)&((x)-1))==0)
#define howmany(x, y)   (((x)+((y)-1))/(y))
#define MIN(a, b)       (((a) < (b)) ? (a) : (b))
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))

#define mk1(p, x)       (p) = (u_int8_t)(x)
#define mk2(p, x)                       \
        (p)[0] = (u_int8_t)(x),         \
        (p)[1] = (u_int8_t)((x) >> 8)
#define mk4(p, x)                       \
        (p)[0] = (u_int8_t)(x),         \
        (p)[1] = (u_int8_t)((x) >> 8),  \
        (p)[2] = (u_int8_t)((x) >> 16), \
        (p)[3] = (u_int8_t)((x) >> 24)

/*
 *bs part in DBR
 */
struct bs {
        u_int8_t jmp[3];        /*bootstrap entry point*/
        u_int8_t oem[8];        /*OEM name and version*/
};

/*
 *BIOS Parametre Block(bpb) in DBR
 */
struct bsbpb {
        u_int8_t bytes_per_sector[2];           /*bytes per sector*/
        u_int8_t sectors_per_cluster;           /*sectors per cluster*/
        u_int8_t reserved_sectors[2];           /*numbers of reserved sectors*/
        u_int8_t fat_count;                     /*numbers of FATs*/
        u_int8_t root_entries_fat16[2];         /*numbers of directory entries in root directory*/
        u_int8_t total_sectors16[2];            /*small total sectors*/
        u_int8_t media_descriptor;              /*media descriptor*/
        u_int8_t small_sectors_per_FAT[2];      /*small sectors per FAT*/
        u_int8_t sectors_per_track[2];          /*sectors per track*/
        u_int8_t drive_heads_count[2];          /*numbers of drive heads*/
        u_int8_t hidden_sectors[4];             /*numbers of hidden sectors*/
        u_int8_t total_sectors32[4];            /*big total sectors*/
};

/*
 *BIOS Parametre Block(bpb) in DBR only for FAT32
 */
struct bsxbpb {
        u_int8_t big_sectors_per_FAT[4];        /*big sectors per FAT*/
        u_int8_t flags[2];                      /*FAT control flags*/
        u_int8_t filesystem_version[2];         /*file system version*/
        u_int8_t root_cluster[4];               /*root directory start cluster*/
        u_int8_t info_sector_num[2];            /*file system info sector num*/
        u_int8_t backup_boot_sector_num[2];     /*backup boot sector num*/
        u_int8_t reserved[12];                  /*reserved*/
};

/*
 *bs part in DBR
 *the same structure for FAT16 and FAT32
 *but different location in DBR
 */
struct bsx {
        u_int8_t drive_number;          /*disk drive parameters*/
        u_int8_t reserved;              /*reserved*/
        u_int8_t extend_boot_signature; /*extended boot signature*/
        u_int8_t volumeID[4];           /*volume ID number*/
        u_int8_t volume_label[11];      /*volume label*/
        u_int8_t filesystem_type[8];    /*file system type*/
};

/*
 *directory entry
 */
struct de {
        u_int8_t name_extend[11];       /*name and extension*/
        u_int8_t attributes;            /*attributes*/
        u_int8_t reserved[10];          /*reserved*/
        u_int8_t creat_time[2];         /*creation time*/
        u_int8_t creat_date[2];         /*creation date*/
        u_int8_t start_cluster[2];      /*starting cluster*/
        u_int8_t file_size[4];          /*size*/
};

/*
 *all bpb
 */
struct bpb {
        u_int bytes_per_sector;         /*bytes per sector*/
        u_int sectors_per_cluster;      /*sectors per cluster*/
        u_int reserved_sectors;         /*numbers of reserved sectors*/
        u_int fat_count;                /*numbers of FATs*/
        u_int root_entries_fat16;       /*numbers of directory entries in root directory*/
        u_int total_sectors16;          /*small total sectors*/
        u_int media_descriptor;         /*media descriptor*/
        u_int small_sectors_per_FAT;    /*small sectors per FAT*/
        u_int sectors_per_track;        /*sectors per track*/
        u_int drive_heads_count;        /*numbers of drive heads*/
        u_int hidden_sectors;           /*numbers of hidden sectors*/
        u_int total_sectors32;          /*big total sectors*/
        u_int big_sectors_per_FAT;      /*big sectors per FAT*/
        u_int root_cluster;             /*root directory start cluster*/
        u_int info_sector_num;          /*file system info sector num*/
        u_int backup_boot_sector_num;   /*backup boot sector num*/
};

static const u_int8_t bootcode[] = {
        0xfa,               /* cli             */
        0x31, 0xc0,         /* xor    ax,ax    */
        0x8e, 0xd0,         /* mov    ss,ax    */
        0xbc, 0x00, 0x7c,   /* mov    sp,7c00h */
        0xfb,               /* sti             */
        0x8e, 0xd8,         /* mov    ds,ax    */
        0xe8, 0x00, 0x00,   /* call   $ + 3    */
        0x5e,               /* pop    si       */
        0x83, 0xc6, 0x19,   /* add    si,+19h  */
        0xbb, 0x07, 0x00,   /* mov    bx,0007h */
        0xfc,               /* cld             */
        0xac,               /* lodsb           */
        0x84, 0xc0,         /* test   al,al    */
        0x74, 0x06,         /* jz     $ + 8    */
        0xb4, 0x0e,         /* mov    ah,0eh   */
        0xcd, 0x10,         /* int    10h      */
        0xeb, 0xf5,         /* jmp    $ - 9    */
        0x30, 0xe4,         /* xor    ah,ah    */
        0xcd, 0x16,         /* int    16h      */
        0xcd, 0x19,         /* int    19h      */
        0x0d, 0x0a,
        'N', 'o', 'n', '-', 's', 'y', 's', 't',
        'e', 'm', ' ', 'd', 'i', 's', 'k',
        0x0d, 0x0a,
        'P', 'r', 'e', 's', 's', ' ', 'a', 'n',
        'y', ' ', 'k', 'e', 'y', ' ', 't', 'o',
        ' ', 'r', 'e', 'b', 'o', 'o', 't',
        0x0d, 0x0a,
        0
};

struct boot_sector {
        u_int8_t    ignored[3];     /* Bootstrap code */
        char        system_id[8];   /* Name of fs */
        u_int8_t    sector_size[2]; /* Bytes/sector */
        u_int8_t    cluster_size;   /* Sectors/cluster */
        u_int16_t   reserved;       /* Number of reserved sectors */
        u_int8_t    fats;           /* Number of FATs */
        u_int8_t    dir_entries[2]; /* Number of root directory entries */
        u_int8_t    sectors[2];     /* Number of sectors */
        u_int8_t    media;          /* Media code */
        u_int16_t   fat_length;     /* Sectors/FAT */
        u_int16_t   secs_track;     /* Sectors/track */
        u_int16_t   heads;          /* Number of heads */
        u_int32_t   hidden;         /* Number of hidden sectors */
        u_int32_t   total_sect;     /* Number of sectors (if sectors == 0) */

        /* FAT32 only */
        u_int32_t   fat32_length;   /* Sectors/FAT */
        u_int16_t   flags;          /* Bit 8: fat mirroring, low 4: active fat */
        u_int8_t    version[2];     /* Filesystem version */
        u_int32_t   root_cluster;   /* First cluster in root directory */
        u_int16_t   info_sector;    /* Filesystem info sector */
        u_int16_t   backup_boot;    /* Backup boot sector */
        u_int16_t   reserved2[6];   /* Unused */
};

static void print_bpb(struct bpb *);
static void mklabel(u_int8_t *, const char *);
static void setstr(u_int8_t *, const char *, size_t);

static void print_bpb(struct bpb *bpb)
{
        dprintf(INFO,"bytes per sector=%u sectors per cluster=%u \
                reserved_sectors=%u fat_count=%u\n",
                bpb->bytes_per_sector, bpb->sectors_per_cluster, bpb->reserved_sectors,
                bpb->fat_count);
        if (bpb->root_entries_fat16)
                dprintf(INFO,"root_entries_fat16=%u\n", bpb->root_entries_fat16);
        if (bpb->total_sectors16)
                dprintf(INFO,"total_sectors16=%u\n", bpb->total_sectors16);

        dprintf(INFO,"media_descriptor=%#x\n", bpb->media_descriptor);

        if (bpb->small_sectors_per_FAT)
                dprintf(INFO,"small_sectors_per_FAT=%u\n", bpb->small_sectors_per_FAT);

        dprintf(INFO,"sectors_per_track=%u drive_heads_count=%u hidden_sectors=%u\n",
                bpb->sectors_per_track, bpb->drive_heads_count, bpb->hidden_sectors);

        if (bpb->total_sectors32)
                dprintf(INFO,"total_sectors32=%u\n",bpb->total_sectors32);
        if (!bpb->small_sectors_per_FAT) {
                dprintf(INFO,"big_sectors_per_FAT=%u root_cluster=%u\n",
                        bpb->big_sectors_per_FAT, bpb->root_cluster);
                dprintf(INFO,"info_sector_num=");
                dprintf(INFO,bpb->info_sector_num == MAXU16 ? "%#x\n" : "%u\n",
                        bpb->info_sector_num);
                dprintf(INFO,"backup_boot_sector_num=");
                dprintf(INFO,bpb->backup_boot_sector_num == MAXU16 ? "%#x\n" : "%u\n",
                        bpb->backup_boot_sector_num);
        }
}

/*
 *Make a volume label
 */
static void mklabel(u_int8_t *dest, const char *src)
{
        int i, c;
        for (i = 0; i < 11; i++) {
                c = *src ? toupper(*src++) : ' ';
                *dest++ = !i && c == '\xe5' ? 5 : c;
        }
}

/*
 *Copy string, padding with spaces.
 */
static void setstr(u_int8_t *dest, const char *src, size_t len)
{
        while (len--)
                *dest++ = *src ? *src++ : ' ';
}

