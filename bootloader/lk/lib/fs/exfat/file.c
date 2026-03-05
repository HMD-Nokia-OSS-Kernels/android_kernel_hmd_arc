#include "exfat.h"
#include "exfat_priv.h"

#define DIR_ENTRY_LENGTH 32

uint8_t g_dir_content_block[MAX_RD_BLK_SIZE];
uint8_t g_dir_content_block1[MAX_RD_BLK_SIZE];

extern struct rtc_time get_time_by_sec(void);

/*
 * Write bitmap buffer into block device
 */
static int flush_bitmap(exfat_fs_t *exfat)
{
    if(exfat->bitmap_idx < exfat->bitmap_sect)
        return 0;

    if(exfat_disk_write(exfat, exfat->bitmap_idx, 1, exfat->bitmap) < 0) {
        dprintf(INFO, "Error: writting bitmap file\n");
        return -1;
    }
    return 0;
}

/*
 * Write fat buffer into block device
 */
static int flush_exfatent(exfat_fs_t *exfat)
{
    if (exfat->fatbuf_idx < exfat->fat_sect)
        return 0;

    if(exfat_disk_write(exfat, exfat->fatbuf_idx, 1, exfat->fatbuf) < 0) {
        dprintf(INFO, "Error: writting fat table\n");
        return -1;
    }
    if (exfat->fat_num >1) {
        if(exfat_disk_write(exfat, exfat->fatbuf_idx + exfat->fatlength, 1, exfat->fatbuf) < 0) {
             dprintf(INFO, "Error: writting backup fat table\n");
            return -1;
        }
    }
    return 0;
}

static uint32_t get_available_size(exfat_fs_t *exfat)
{
    uint32_t  cl_avail = 0;
    uint32_t  idx, ival, start_sec;
    int checked = 0;

    if (flush_bitmap(exfat) < 0) {
        dprintf(INFO, "Error: flush bitmap buffer\n");
        return  -1;
    }

    start_sec = exfat->bitmap_sect;
    while (checked < exfat->bitmaplen) {
        if (get_exfat_blocks(exfat, start_sec, exfat->bitmap, 1) != 0) {
            dprintf(INFO, "Error: reading bitmap\n");
            return  -1;
        }
        exfat->bitmap_idx = start_sec++;
        for (idx=0; idx < exfat->bytes_per_sector; idx++) {
            ival = exfat->bitmap[idx];
            cl_avail += 8 - (uint32_t)bits_num_table[ival];
            checked++;
            if(checked >= exfat->bitmaplen)
                goto exit;
        }
    }

    exit:
        return cl_avail;
}

static int check_available_size(exfat_fs_t *exfat,  uint64_t size)
{
    uint32_t  cl_size  = exfat->bytes_per_cluster;
    uint32_t  cl_needed = (uint32_t)((size + cl_size -1 )/cl_size);
    uint32_t  cl_avail = 0;
    int result;

    cl_avail = get_available_size(exfat);

    result = (cl_avail > cl_needed) ? 0 : 1;
    return result;
}

static uint32_t clear_bitmap(exfat_fs_t *exfat, uint32_t  curclust)
{
    uint32_t  start_sec, bits_off, bytes_off, sec_off;

    curclust -= 2;    // bitmap starts from cluster 2
    sec_off = (curclust /8) / exfat->bytes_per_sector;
    bits_off  = curclust & 0x7;
    bytes_off = curclust /8 - sec_off * exfat->bytes_per_sector;
    start_sec = exfat->bitmap_sect+ sec_off;

    if (exfat->bitmap_idx != start_sec) {
        if (exfat->bitmap_idx >= exfat->bitmap_sect) {
            if (flush_bitmap(exfat) < 0) {
                return 0;
            }
        }
        if (get_exfat_blocks(exfat, start_sec, exfat->bitmap, 1) != 0) {
            dprintf(INFO, "Error: reading bitmap\n");
            return  0;
        }
        exfat->bitmap_idx = start_sec;
    }
    exfat->bitmap[bytes_off] &= bitmap_clr_mask[bits_off];

    return curclust+2;
}

static uint32_t clear_exfatent_value(exfat_fs_t *exfat, uint32_t  curclust)
{
    uint32_t  nextclust, start_sec, bytes_off, sec_off;

    nextclust = 0;
    sec_off = (curclust * EXFAT_FAT_ENTRY_SIZE) / exfat->bytes_per_sector;
    bytes_off = curclust * EXFAT_FAT_ENTRY_SIZE - sec_off * exfat->bytes_per_sector;
    start_sec = exfat->fat_sect + sec_off;

    if (exfat->fatbuf_idx != start_sec) {
        if (exfat->fatbuf_idx >= exfat->fat_sect) {
            if (flush_exfatent(exfat) < 0) {
                return 0;
            }
        }

        if (get_exfat_blocks(exfat, start_sec, exfat->fatbuf, 1) != 0) {
            dprintf(INFO, "Error: reading fat table\n");
            return  0;
        }
        exfat->fatbuf_idx = start_sec;
    }

    nextclust = *(uint32_t *)(exfat->fatbuf+bytes_off);
    nextclust = EXFAT2CPU32(nextclust);
    if (nextclust==0) {
        dprintf(INFO, "Error: next_cl shoud not be all 00, cur_clust is 0x%x\n", curclust);
        nextclust = curclust + 1;
    }

    *(uint32_t *)(exfat->fatbuf + bytes_off) = 0;
    return nextclust;
}

/*
 * Clear original file's bitmap & fat table entry
 */
static int clear_exfatent_bitmap(exfat_fs_t *exfat, uint32_t start_clust, uint32_t cl_num, int clr_fatent)
{

    uint32_t next_cluster, cur_cluster, i=0;
    cur_cluster = start_clust;

    while (i++ < cl_num) {
        if (clr_fatent)
            next_cluster = clear_exfatent_value(exfat, cur_cluster);
        else
            next_cluster = cur_cluster+1;
        cur_cluster  = clear_bitmap(exfat, cur_cluster);
        if (next_cluster < 2 || next_cluster > exfat->cluster_cnt ||
            cur_cluster < 2 || cur_cluster > exfat->cluster_cnt ) {
            dprintf(INFO, "exFAT broken next cluster or file end: %08x or clear bitmap %08x\n", next_cluster,cur_cluster);
            break;
        }
        cur_cluster = next_cluster;
    }

    if (flush_exfatent(exfat) < 0) {
        return -1;
    }
    if (flush_bitmap(exfat) < 0) {
        return -1;
    }

    return 0;
}

static uint32_t set_exfatent_value(exfat_fs_t *exfat, uint32_t  curclust,  uint32_t val)
{
    uint32_t start_sec, bytes_off, sec_off;

    sec_off = (curclust *EXFAT_FAT_ENTRY_SIZE) / exfat->bytes_per_sector;
    bytes_off = curclust * EXFAT_FAT_ENTRY_SIZE - sec_off * exfat->bytes_per_sector;
    start_sec = exfat->fat_sect + sec_off;

    if (exfat->fatbuf_idx != start_sec) {
        if (exfat->fatbuf_idx >= exfat->fat_sect) {
            if (flush_exfatent(exfat) < 0) {
                return 0;
            }
        }
        if (get_exfat_blocks(exfat, start_sec, exfat->fatbuf, 1) != 0) {
            dprintf(INFO, "Error: reading fat table\n");
            return  0;
        }
        exfat->fatbuf_idx = start_sec;
    }

    *(uint32_t *)(exfat->fatbuf + bytes_off) = cpu_to_le32(val);
    return 0;
}

static int check_bitmap_fatbuf(exfat_fs_t *exfat)
{
    uint32_t curclust, nextclust, start_sec, bytes_off, sec_off;
    uint32_t idx, cl_num, cl_size, *pentry;

    curclust = exfat->cur_bitmap_cluster;

    sec_off = (curclust *EXFAT_FAT_ENTRY_SIZE) / exfat->bytes_per_sector;
    bytes_off = curclust * EXFAT_FAT_ENTRY_SIZE - sec_off * exfat->bytes_per_sector;
    start_sec = exfat->fat_sect + sec_off;

    if (get_exfat_blocks(exfat, start_sec, exfat->bitmap_fatbuf, exfat->bitmap_fatbuf_size) != 0) {
        dprintf(INFO, "Error: reading bitmap fat entries\n");
        return  -1;
    }

    cl_size = exfat->bytes_per_sector*exfat->sectors_per_cluster;
    cl_num = (exfat->bitmaplen + cl_size - 1)/cl_size;
    pentry = (uint32_t *)(exfat->bitmap_fatbuf + bytes_off);
    for (idx=0; idx < cl_num; idx++) {
        nextclust = EXFAT2CPU32(*pentry);
        if (nextclust < exfat->cluster_cnt && (nextclust != (curclust+1))) {
            dprintf(INFO, "Error: non-continuously bitmap, cluster %d to %d.\n", curclust, nextclust);
            return -1;
        }
        curclust = nextclust;
        pentry++;
    }

    return 0;
}

static int write_back_entry(exfat_fs_t *exfat, exfat_whole_dentry *p_ent)
{
    /* Write directory table to device */
    if (p_ent->bf0.sectors > 0) {
        if (exfat_disk_write(exfat, p_ent->bf0.start_sec,  p_ent->bf0.sectors, p_ent->bf0.pbuf) < 0) {
            dprintf(INFO, "Error writing data sector: %u, cnt: %u\n", p_ent->bf0.start_sec, p_ent->bf0.sectors);
            return -1;
        }
    }
    if (p_ent->bf1.sectors > 0) {
        if (exfat_disk_write(exfat, p_ent->bf1.start_sec,  p_ent->bf1.sectors, p_ent->bf1.pbuf) < 0) {
            dprintf(INFO, "Error writing data sector: %u, cnt: %u\n", p_ent->bf1.start_sec, p_ent->bf1.sectors);
            return -1;
        }
    }
    return 0;
}

static uint32_t get_next_cluster(exfat_fs_t *exfat, uint32_t curclust)
{
    uint32_t nextclust, start_sec, bytes_off, sec_off;

    nextclust = 0;
    sec_off = (curclust *EXFAT_FAT_ENTRY_SIZE) / exfat->bytes_per_sector;
    bytes_off = curclust * EXFAT_FAT_ENTRY_SIZE - sec_off * exfat->bytes_per_sector;
    start_sec = exfat->fat_sect + sec_off;

    if (exfat->fatbuf_idx != start_sec) {
        if (get_exfat_blocks(exfat, start_sec, exfat->fatbuf, 1) != 0) {
        dprintf(INFO, "Error: reading fat table\n");
        return  0;
        }
        exfat->fatbuf_idx = start_sec;
    }
    nextclust = *(uint32_t *)(exfat->fatbuf+bytes_off);
    nextclust = EXFAT2CPU32(nextclust);
    if (nextclust==0) {
        dprintf(INFO, "Error nextclust %d \n", nextclust);
        nextclust = curclust + 1;
    }

    return nextclust;
}

static int exfat_path_init(exfat_fs_t *exfat,  exfat_buf_array *bfs , uint32_t startsect)
{
    uint32_t curclust = (startsect - exfat->data_begin) / exfat->sectors_per_cluster;
    uint32_t rd_sec,  rd_sec_total = 0;
    int i, get_bitmap, get_uc;
    get_bitmap = get_uc = 0;

    while (1) {
        exfat_dentry *dentptr;

        if (bfs->bf1.sectors > 0)
            bfs->bf0 = bfs->bf1;

        if (bfs->bf1.pbuf == g_dir_content_block)
            bfs->bf1.pbuf = g_dir_content_block1;
        else
            bfs->bf1.pbuf = g_dir_content_block;

        rd_sec = (exfat->sectors_per_cluster - rd_sec_total) > exfat->rd_sectors ?
                exfat->rd_sectors : (exfat->sectors_per_cluster - rd_sec_total);
        if (get_exfat_blocks(exfat, startsect, bfs->bf1.pbuf, rd_sec) != 0) {
            dprintf(INFO, "Error: reading root directory block......Return -1\n");
            return  -1;
        }
        bfs->bf1.sectors = rd_sec;
        bfs->bf1.cluster = curclust;
        bfs->bf1.start_sec = startsect;
        startsect += rd_sec;
        rd_sec_total += rd_sec;

        dentptr = (exfat_dentry *)bfs->bf1.pbuf;
        for (i = 0; i < (bfs->bf1.sectors * exfat->bytes_per_sector /sizeof(exfat_dentry));  i++, dentptr++) {

            if ((dentptr->entry_type & EXFAT_DTYPE_USED) &&
                ((dentptr->entry_type & EXFAT_DTYPE_MASK) == EXFAT_DTYPE_BM) ) {
                exfat_bitmap_entry *bm_dentptr = dentptr;
                exfat->cur_bitmap_cluster = EXFAT2CPU32(bm_dentptr->start_clu);
                exfat->bitmap_sect = exfat->cur_bitmap_cluster * exfat->sectors_per_cluster + exfat->data_begin;
                exfat->bitmaplen = (uint32_t) EXFAT2CPU64(bm_dentptr->data_len);
                get_bitmap = 1;
                dprintf(INFO, "exFAT bitmap entry got. \n");
            }
            else if ((dentptr->entry_type & EXFAT_DTYPE_USED) &&
                    ((dentptr->entry_type & EXFAT_DTYPE_MASK) == EXFAT_DTYPE_UC) ) {
                uint32_t start_cl = EXFAT2CPU32(*(uint32_t *)((uint8_t *)dentptr + 20));
                uint32_t length = (uint32_t)EXFAT2CPU64(*(uint64_t *)((uint8_t *)dentptr + 24));
                dprintf(INFO, "exFAT up-case table entry. \n");
                if(get_upcase_table(exfat, start_cl, length) < 0)
                    return -1;
                get_uc = 1;
            }
            else {
                // not bitmap or up-case table entry, skip it.
            }

            if (get_bitmap && get_uc)
                return 0;
        }

        if (rd_sec_total >= exfat->sectors_per_cluster) {
            rd_sec_total = 0;
            curclust = get_next_cluster(exfat, curclust);
            if (curclust < 2 || curclust > exfat->cluster_cnt) {
                dprintf(INFO, "Error: exfat get end, or invalid next cluster %08x\n", curclust);
                return -1;
            }
            startsect = exfat->data_begin + curclust * exfat->sectors_per_cluster;
        }
    }

    return -1;
}

/*
 * Write at most 'size' bytes from 'buffer' into the specified cluster.
 * Return 0 if success, -1 otherwise.
 */
static int write_clusters(exfat_fs_t *exfat, uint32_t start_clust, uint8_t *buffer, unsigned long size)
{
    int idx = 0;
    uint32_t startsect,  sect_n;

    startsect = exfat->data_begin + start_clust * exfat->sectors_per_cluster;
    sect_n = size / exfat->bytes_per_sector;

    if (sect_n > 0) {
        if (exfat_disk_write(exfat, startsect, sect_n, buffer) < 0) {
            dprintf(INFO, "Error writing data\n");
            return -1;
        }
    }

    if (size % exfat->bytes_per_sector) {
        uint8_t *tmpbuf;
        tmpbuf = malloc(sizeof(uint8_t) * exfat->bytes_per_sector);
        if (tmpbuf == NULL) {
            dprintf(INFO, "Error malloc tmpbuf\n");
            return -1;
        }

        idx = size / exfat->bytes_per_sector;
        buffer += idx * exfat->bytes_per_sector;
        memcpy(tmpbuf, buffer, size % exfat->bytes_per_sector);

        if (exfat_disk_write(exfat, startsect + idx, 1, tmpbuf) < 0) {
            dprintf(INFO, "Error writing data\n");
            free(tmpbuf);
            return -1;
        }
        free(tmpbuf);
    }

    return 0;
}

/*
 * Clear cluster to all zero. Size should be multiplier of cluster size
 * Return 0 if success, -1 otherwise.
 */
static int
clear_clusters(exfat_fs_t *exfat, uint32_t start_clust, uint8_t *buffer, unsigned long size)
{
    uint32_t startsect,  sect_n;
    uint8_t *tmp_buf = NULL;

    tmp_buf = exfat_malloc(exfat->bytes_per_sector);
    if (tmp_buf == NULL) {
        dprintf(INFO, "Error: allocate memory failed.\n");
        return -1;
    }
    memset(tmp_buf, 0, exfat->bytes_per_sector);

    startsect  = exfat->data_begin + start_clust * exfat->sectors_per_cluster;
    sect_n = size / exfat->bytes_per_sector;

    while (sect_n-- > 0) {
        if (exfat_disk_write(exfat, startsect, 1, tmp_buf) < 0) {
            free_malloc(tmp_buf);
            dprintf(INFO, "Error writing data\n");
            return -1;
        }
        startsect++;
    }

    free_malloc(tmp_buf);
    return 0;
}

static int split_parent_path(const char* path, char *parent_path)
{
    const char *start, *end, *cur;
    int name_len;

    if (path == NULL) {
        dprintf(INFO, "filename is Empty\n");
        return -1;
    }

    cur = path;
    start = path;
    end = NULL;
    while (*cur != 0) {
        while (*cur == '/')
            cur++;
        end = cur;
        while (*cur != '/' && *cur != 0)
            cur++;
    }
    if (end == NULL)
        return -1;

    name_len = end - start;
    memcpy(parent_path, start, name_len);
    parent_path[name_len] = '\0';
    return 0;
}

/* split the last name in complete filename. */
static int split_last_name(const char* complete_filename, char* filename)
{
    const char *start, *end, *cur;
    int name_len;

    if (complete_filename == NULL) {
        dprintf(INFO, "filename is Empty\n");
        return -1;
    }

    end = cur = start = complete_filename;
    while (*cur != 0) {
        while (*cur =='/')
            cur++;
        start = cur;
        while (*cur != '/' && *cur != 0)
            cur++;
        end = cur;
    }
    name_len = end - start;
    if (name_len > FNAME_MAXLEN_BYTES) {
        dprintf(INFO, "Error: filename is too long!\n");
        return -1;
    }
    memcpy(filename, start, name_len);
    filename[name_len] = '\0';
    exfat_downcase(filename);
    return 0;
}

/* calculate primary checksum  */
static void cal_checksum(exfat_whole_dentry *p_ent)
{
    uint8_t *ptr = (uint8_t *)(p_ent->prim);
    uint16_t  val, checksum = 0;
    int i, j, n;
    n = 32;
    for (i=0; i<n; i++) {
        if (i==2 || i==3)
            continue;
        val = (uint16_t)ptr[i];
        checksum = ((checksum&1) ? 0x8000 : 0) + (checksum >>1) + val;
    }

    ptr = (uint8_t *)(p_ent->strext);
    for (i=0; i<n; i++) {
        val = (uint16_t)ptr[i];
        checksum = ((checksum&1) ? 0x8000 : 0) + (checksum >>1) + val;
    }

    for (j=0; j<(p_ent->prim->secondary_cnt-1); j++) {
        ptr = (uint8_t *)(p_ent->fname_dent[j]);
        for (i=0; i<n; i++) {
            val = (uint16_t)ptr[i];
            checksum = ((checksum&1) ? 0x8000 : 0) + (checksum >>1) + val;
        }
    }
    p_ent->prim->checksum = cpu_to_le16(checksum);
}

/* Find first available cluster from *start, return consecutive number, and set bitmap*/
static uint32_t get_available_clusters(exfat_fs_t *exfat, uint32_t  *start, uint32_t max_cl, int search_first)
{
    uint32_t start_sec, bits_off, rem_bits, bytes_off, sec_off, avail, result, curclust, i;
    uint8_t value;

    if (*start < 2)
        curclust = 0;
    else
        curclust = *start - 2;

    /* find first bit-0 from current cluster  */
    while (search_first) {
        sec_off = (curclust / 8) / exfat->bytes_per_sector;
        bits_off  = curclust & 0x7;
        bytes_off = curclust /8 - sec_off * exfat->bytes_per_sector;
        start_sec = exfat->bitmap_sect+ sec_off;

        if (exfat->bitmap_idx != start_sec) {
            if (exfat->bitmap_idx >= exfat->bitmap_sect) {
                if (flush_bitmap(exfat) < 0) {
                    return 0;
                }
            }
            if (get_exfat_blocks(exfat, start_sec, exfat->bitmap, 1) != 0) {
                dprintf(INFO, "Error: reading bitmap\n");
                return  0;
            }
            exfat->bitmap_idx = start_sec;
        }
        value = exfat->bitmap[bytes_off] >> bits_off;
        if (max_cl > 1) {
            if (value==0) {
                *start = (curclust+2);
                break;
            }
            curclust += 8 - bits_off;
        } else {
            for (i = bits_off; i < 8; i++) {
                if ((value&1)==0) {
                    break;
                }
                value >>= 1;
            }
            if (i<8) {
                curclust += i - bits_off;
                *start = (curclust+2);
                break;
            }
            curclust += 8 -  bits_off;
        }
    }

    /* Find consecutive number of bits 0 */
    result = 0;
    while (curclust < exfat->cluster_cnt) {
        sec_off = (curclust /8) / exfat->bytes_per_sector;
        bits_off = curclust & 0x7;
        rem_bits = 8 - bits_off;
        bytes_off = curclust /8 - sec_off * exfat->bytes_per_sector;
        start_sec = exfat->bitmap_sect+ sec_off;

        if (exfat->bitmap_idx != start_sec) {
            if (exfat->bitmap_idx >= exfat->bitmap_sect) {
                if (flush_bitmap(exfat) < 0) {
                    return 0;
                }
            }
            if (get_exfat_blocks(exfat, start_sec, exfat->bitmap, 1) != 0) {
                dprintf(INFO, "Error: reading bitmap\n");
                return  0;
            }
            exfat->bitmap_idx = start_sec;
        }
        value = exfat->bitmap[bytes_off] >> bits_off;
        avail  = (uint32_t)bitmap_avail[value];
        result += avail;

        if (avail > rem_bits) {
            avail = rem_bits;
            result -= bits_off;
        }
        if (result > max_cl) {
            avail -= (result - max_cl);
            result = max_cl;
        }
        value = bitmap_mask[avail] << bits_off;
        exfat->bitmap[bytes_off] |= value;
        if (avail < rem_bits || result >= max_cl) {
            break;
        }
        curclust += rem_bits;
    }
    return  result;
}

static uint64_t write_file_contents(exfat_fs_t *exfat, uint32_t *start_cluster,  uint8_t *buffer, uint64_t filesize, int *cont, int clear)
{
    uint64_t gotsize = 0;
    uint64_t actsize, gotclust;
    uint32_t bytesperclust = exfat->bytes_per_cluster;
    uint32_t curclust  = *start_cluster;
    uint32_t last_startclust = 0, last_endclust = 0, needed_cluster = 0;
    int search_first = 0;
    *cont=0;

    while (filesize > 0) {
        gotclust = 0;
        actsize = 0;
        needed_cluster = (filesize + bytesperclust-1) / bytesperclust;
        /* search for consecutive clusters */
        if (needed_cluster > 0)	{
            /* For existing file over-writting, *start_cluster is always available. */
            search_first = ((*start_cluster == 0) || (curclust != *start_cluster)) ? 1 : 0;
            gotclust = get_available_clusters(exfat, &curclust, needed_cluster, search_first);
            if (gotclust == 0) {
                dprintf(INFO, "No free cluster for use. Left size = %ld, written size = %ld\n", filesize, gotsize);
                goto out;
            }
            if (*start_cluster == 0) {
                *start_cluster = curclust;
            }
            actsize = gotclust * bytesperclust;
        }
        if (actsize > filesize) {  /* Process tail data of file */
            actsize = filesize;
        }

        if (actsize > 0) {
            if (clear) {
                if (clear_clusters(exfat, curclust, buffer, actsize) != 0) {
                    dprintf(INFO, "error: writing cluster\n");
                    goto out;
                }
            }
            else if (write_clusters(exfat, curclust, buffer, actsize) != 0) {
                dprintf(INFO, "error: writing cluster\n");
                goto out;
            }

            /*  update clusters chain */
            if (*cont > 0) {
                for ( ; last_startclust < last_endclust; last_startclust++) {
                    set_exfatent_value(exfat, last_startclust, last_startclust+1);
                }
                set_exfatent_value(exfat, last_endclust, curclust);
            }

            last_startclust = curclust;
            last_endclust = curclust + gotclust - 1;
            curclust = last_endclust + 2;
            gotsize +=actsize;
            filesize -= actsize;
            buffer += actsize;
            *cont = *cont +1;
        }
    }
out:
    if (*cont > 1 || gotsize <= bytesperclust) {
        for ( ; last_startclust < last_endclust; last_startclust++) {
            set_exfatent_value(exfat, last_startclust,  last_startclust+1);
        }
        /* set file end */
        curclust =  0xffffffff;
        set_exfatent_value(exfat, last_endclust,  curclust);
    }

    return gotsize;
}

static exfat_file_entry  *
find_dir_entry(exfat_fs_t *exfat,  exfat_whole_dentry *p_ent,  exfat_whole_dentry *p_parent,
                    char *filename, int is_dir)
{
    uint32_t curclust, startsect, sec_all;
    uint32_t rd_sec, rd_sec_cl = 0, rd_sec_all = 0;
    int pending_strext = 0;
    int pending_fnext = 0;
    exfat_file_entry *ret_dent = NULL;
    exfat_file_entry *prim_entry = NULL;
    exfat_strext_entry *strext_entry = NULL;
    char l_name[FNAME_MAXLEN_BYTES];
    int i, j, fn_len;
    int idx = 0, cont = 0;
    int fname_dent_idx = 0, strend = 0;

    if (p_parent->strext == NULL) {// root dir
        sec_all = 0xffffffff;
        startsect = exfat->rootdir_sect;
        curclust = (startsect - exfat->data_begin) / exfat->sectors_per_cluster;
        cont = 0;
    } else {
        sec_all = 0xffffffff;
        curclust = EXFAT2CPU32(p_parent->strext->first_cluster);
        startsect = exfat->data_begin + curclust * exfat->sectors_per_cluster;
        cont = (p_parent->strext->secondary_flags==0x03) ? 1 : 0;
    }

    fn_len = strlen(filename);

    exfat_dentry_clear(exfat, p_ent);

    while (1) {
        exfat_dentry *dentptr;
        uint8_t *temp = p_ent->bf0.pbuf;
        p_ent->bf0 = p_ent->bf1;
        p_ent->bf1.pbuf=temp;

        rd_sec = 1;
        memset(p_ent->bf1.pbuf, 0, exfat->bytes_per_sector);
        if (get_exfat_blocks(exfat, startsect, p_ent->bf1.pbuf, rd_sec) != 0) {
            dprintf(INFO, "Error: reading directory block......Return -1\n");
            return  (exfat_file_entry  *)-1;
        }

        p_ent->bf1.sectors = rd_sec;
        p_ent->bf1.cluster = curclust;
        p_ent->bf1.start_sec = startsect;
        startsect += rd_sec;
        rd_sec_cl += rd_sec;
        rd_sec_all += rd_sec;

        dentptr = (exfat_dentry *)p_ent->bf1.pbuf;
        for (i = 0; i < (p_ent->bf1.sectors * exfat->bytes_per_sector /sizeof(exfat_dentry));  i++, dentptr++) {
            if (pending_strext) {
                if ((dentptr->entry_type & EXFAT_DTYPE_USED) &&
                    ((dentptr->entry_type & EXFAT_DTYPE_MASK) == EXFAT_DTYPE_STREXT) ) {
                    strext_entry = (exfat_strext_entry *)dentptr;
                    pending_strext = 0;
                    if (fn_len != (int)strext_entry->name_len) {
                        pending_strext = 0;
                        pending_fnext = 0;
                        continue;
                    }
                    memset(l_name, '\0', sizeof(l_name));
                    idx = strend = 0;
                    fname_dent_idx = 0;
                    continue;
                }
                else {
                    pending_strext = 0;
                    pending_fnext = 0;
                }
            }
            else if (pending_fnext) {
                if ((dentptr->entry_type&EXFAT_DTYPE_USED) &&
                    ((dentptr->entry_type & EXFAT_DTYPE_MASK)== EXFAT_DTYPE_FNEXT) ) {
                    pending_fnext--;
                    p_ent->fname_dent[fname_dent_idx++] = dentptr;
                    for (j=0; j < 30; j+=2) {
                        if((dentptr->bytes[j] != 0 || dentptr->bytes[j+1] !=0) && idx <(int)strext_entry->name_len)
                            l_name[idx++] = dentptr->bytes[j];
                        else {
                            l_name[idx] = '\0';
                            strend = 1;
                        }
                    }
                    if (pending_fnext==0 || strend==1) {
                        exfat_downcase(l_name);
                        if (!strcmp(filename, l_name)) {
                            dprintf(INFO, "Existing file %s is found. It may be overwritten.\n",  filename);
                            ret_dent = prim_entry;
                            ret_dent->str_dent = strext_entry;
                            p_ent->prim = prim_entry;
                            p_ent->strext = strext_entry;
                            return ret_dent;
                        }
                        else {
                            pending_fnext = 0;
                        }
                    }
                    continue;
                }
                else {
                    dprintf(INFO, "Warning: exFAT stream extension entry should be here.");
                    pending_strext = 0;
                    pending_fnext = 0;
                }
            }

            if ((dentptr->entry_type&EXFAT_DTYPE_USED) &&
                    ((dentptr->entry_type & EXFAT_DTYPE_MASK)== EXFAT_DTYPE_FILE) ) {
                prim_entry = (exfat_file_entry *)dentptr;
                uint16_t attr = EXFAT2CPU16(prim_entry->attr);
                //is_dir = -1 : ignore the file type
                if (is_dir != -1){
                    if (is_dir == 0 && attr != EXFAT_ATTR_FILE)
                        continue;
                    if (is_dir == 1 && attr != EXFAT_ATTR_DIR)
                        continue;
                }
                if (prim_entry->secondary_cnt  > 18 || prim_entry->secondary_cnt < 2) {
                    dprintf(INFO, "exFAT secondary entries %d out of range (2,18)\n", prim_entry->secondary_cnt);
                    continue;
                }
                pending_strext = 1;
                pending_fnext = prim_entry->secondary_cnt - 1;
            }
            else if (dentptr->entry_type == 0)  {
                p_ent->empty = dentptr;
                return NULL;
            }
            else {
                // unknown entry, skip it.
            }
        }

        if (rd_sec_all >= sec_all) {
            dprintf(INFO, "Parse to end of current directory. Read: %u,  parent size: %u (sectors).\n", rd_sec_all, sec_all);
            break;
        }

        if (rd_sec_cl >= exfat->sectors_per_cluster) {
            rd_sec_cl = 0;
            if (cont)
                curclust++;
            else
                curclust = get_next_cluster(exfat, curclust);
            if (curclust < 2 || curclust > exfat->cluster_cnt) {
                dprintf(INFO, "Get end or error for current directory, next cluster %08x\n", curclust);
                return  NULL;
            }
            startsect = exfat->data_begin + curclust * exfat->sectors_per_cluster;
        }
    }

    return NULL;
}

static exfat_file_entry *
update_dir_entry(exfat_fs_t *exfat, exfat_whole_dentry *p_ent, uint32_t start_cluster,  uint64_t  written_size, int cont)
{
    exfat_file_entry *prim_entry = p_ent->prim;
    exfat_strext_entry * strext_entry = p_ent->strext;
    struct rtc_time tm;
    uint32_t  mod_time;

    tm = get_time_by_sec();
    mod_time = ((tm.tm_year - 1980) << 9) | (tm.tm_mon << 5) | tm.tm_mday;
    mod_time <<= 16;
    mod_time |= (tm.tm_hour << 11) | (tm.tm_min << 5) |  (tm.tm_sec / 2);

    prim_entry->str_dent = NULL;
    prim_entry->mod_time  = cpu_to_le32(mod_time);
    prim_entry->access_time = cpu_to_le32(mod_time);
    strext_entry->secondary_flags = (cont > 1) ? 0x01 : 0x03;
    strext_entry->first_cluster = cpu_to_le32(start_cluster);
    strext_entry->data_len = strext_entry->val_data_len = cpu_to_le64(written_size);

    cal_checksum(p_ent);
    return prim_entry;
}

static exfat_file_entry *
new_dir_entry(exfat_fs_t *exfat, exfat_whole_dentry *p_ent, const char *filename,
                uint32_t start_cluster, uint64_t written_size, int cont, int is_dir)
{
    exfat_dentry  *dentptr;
    exfat_file_entry *prim_entry = p_ent->empty;
    exfat_strext_entry *strext_entry = NULL;
    exfat_fnext_entry *fname_entry;
    char fname[FNAME_MAXLEN_BYTES];
    uint32_t needed_clust, entry_num;
    uint32_t newclust = 0;
    uint32_t clust_start_sec;
    uint32_t cl_size, rem_bytes;
    size_t  fname_len, cur_len;
    int  i, j, idx;
    struct rtc_time tm;
    uint32_t mod_time = 0;
    uint16_t name_hash, val;
    uint16_t attr;


    fname_len = strlen(filename);
    memcpy(fname, filename, fname_len);
    fname[fname_len] = '\0';

    needed_clust = 0;
    cl_size = exfat->bytes_per_sector * exfat->sectors_per_cluster;
    entry_num = 2 + ((fname_len+14)/15);

    if (p_ent->empty==NULL) {
        needed_clust = (entry_num * 32 + cl_size - 1)/cl_size;
    } else {
        clust_start_sec = exfat->data_begin +  p_ent->bf1.cluster * exfat->sectors_per_cluster;
        rem_bytes = cl_size - (p_ent->bf1.start_sec-clust_start_sec)*exfat->bytes_per_sector;
        rem_bytes -= ((uint8_t *)p_ent->empty - p_ent->bf1.pbuf);
        if ((entry_num*32) > rem_bytes) {
            needed_clust = (entry_num * 32 - rem_bytes + cl_size - 1)/cl_size;
        }
    }

    if (needed_clust) {
        exfat_whole_dentry * parent = (exfat_whole_dentry *)p_ent->parent;
        unsigned long wr_size = (unsigned long)cl_size;
        int temp;
        newclust = 0;
        wr_size = write_file_contents(exfat, &newclust, NULL, wr_size, &temp, 1);
        if (wr_size == 0) {
            debugf("Error: get new cluster for parent.\n");
            return NULL;
        }

        if (parent->prim==NULL) { // parent is root dir
            set_exfatent_value(exfat, p_ent->bf1.cluster,  newclust);
            set_exfatent_value(exfat, newclust,  0xffffffff);
        } else {
            // update parent dir.
            uint64_t data_len = EXFAT2CPU64(parent->strext->data_len);
            uint32_t cl_num;
            cl_num = data_len/cl_size;
            data_len += cl_size;
            parent->strext->data_len = parent->strext->val_data_len = cpu_to_le64(data_len);

            if (parent->strext->secondary_flags==0x03 && newclust != (p_ent->bf1.cluster+1) ) {
                uint32_t  prev;
                parent->strext->secondary_flags=0x01;
                prev = EXFAT2CPU32(parent->strext->first_cluster);

                while(--cl_num > 0){
                    set_exfatent_value(exfat, prev,  prev+1);
                    prev++;
                }
            }
            set_exfatent_value(exfat, p_ent->bf1.cluster,  newclust);
            set_exfatent_value(exfat, newclust,  0xffffffff);

            parent->prim->str_dent = NULL;
            cal_checksum(parent);

            if (write_back_entry(exfat, parent) < 0) {
                debugf("Error writing parent data.\n");
                return NULL;
            }
        }

        if (prim_entry == NULL) {
            uint8_t *tmp = p_ent->bf0.pbuf;
            p_ent->bf0 = p_ent->bf1;
            p_ent->bf1.pbuf = tmp;
            if (p_ent->bf1.pbuf == NULL) {
                p_ent->bf1.pbuf = exfat_malloc(exfat->bytes_per_sector+64);
                if (p_ent->bf1.pbuf == NULL) {
                    debugf("Error: allocate memory failed.\n");
                    return NULL;
                }
            }
            p_ent->bf1.cluster = newclust;
            p_ent->bf1.sectors = 1;
            p_ent->bf1.start_sec = exfat->data_begin + newclust * exfat->sectors_per_cluster;
            memset(p_ent->bf1.pbuf, 0, exfat->bytes_per_sector);
            prim_entry = (exfat_file_entry *)p_ent->bf1.pbuf;
        }
    }

    if (prim_entry==NULL) {
        debugf("Error:  prim_entry is NULL, this should never happen.\n");
        return NULL;
    }

    memset(prim_entry, 0, sizeof(exfat_file_entry));

    tm = get_time_by_sec();
    mod_time = ((tm.tm_year - 1980) << 9) | (tm.tm_mon << 5) | tm.tm_mday;
    mod_time <<= 16;
    mod_time |= (tm.tm_hour << 11) | (tm.tm_min << 5) |  (tm.tm_sec / 2);

    attr = is_dir ? EXFAT_ATTR_DIR : EXFAT_ATTR_FILE;
    prim_entry->entry_type = EXFAT_DTYPE_USED | EXFAT_DTYPE_FILE;
    prim_entry->secondary_cnt = (uint8_t)((entry_num-1)&0xff);
    prim_entry->attr = cpu_to_le16(attr);
    prim_entry->creat_time = cpu_to_le32(mod_time);
    prim_entry->mod_time  = cpu_to_le32(mod_time);
    prim_entry->access_time = cpu_to_le32(mod_time);

    entry_num--;
    dentptr = (exfat_dentry *)prim_entry;
    j = idx = 0;
    name_hash = 0;
    while (entry_num-- >0) {
        uint8_t *bufend = NULL;
        dentptr++;
        bufend = p_ent->bf1.pbuf + p_ent->bf1.sectors * exfat->bytes_per_sector;
        if  ((uint8_t *)dentptr >=bufend) {
            uint8_t *tmp = p_ent->bf0.pbuf;
            p_ent->bf0 = p_ent->bf1;
            p_ent->bf1.pbuf = tmp;
            if (p_ent->bf1.pbuf == NULL) {
                p_ent->bf1.pbuf = exfat_malloc(exfat->bytes_per_sector+64);
                if (p_ent->bf1.pbuf == NULL) {
                    debugf("Error: allocate memory failed.\n");
                    return NULL;
                }
            }

            if (needed_clust) {
                p_ent->bf1.cluster = newclust;
                p_ent->bf1.start_sec = exfat->data_begin + newclust * exfat->sectors_per_cluster;
            } else {
                p_ent->bf1.cluster = p_ent->bf0.cluster;
                p_ent->bf1.start_sec = p_ent->bf0.start_sec + p_ent->bf0.sectors;
            }
            p_ent->bf1.sectors = 1;
            memset(p_ent->bf1.pbuf, 0, exfat->bytes_per_sector);
            dentptr = (exfat_dentry *)p_ent->bf1.pbuf;
        }

        if (strext_entry==NULL) {
            strext_entry = (exfat_strext_entry *)dentptr;
            strext_entry->entry_type = EXFAT_DTYPE_USED | EXFAT_DTYPE_STREXT;
            strext_entry->secondary_flags = (cont > 1) ? 0x01 : 0x03;
            strext_entry->name_len = (uint8_t)(fname_len&0xff);
            strext_entry->name_hash = cpu_to_le16(0x2345);
            strext_entry->first_cluster = cpu_to_le32(start_cluster);
            strext_entry->data_len = strext_entry->val_data_len = cpu_to_le64(written_size);
        } else {
            p_ent->fname_dent[j++] =  fname_entry = (exfat_fnext_entry *)dentptr;
            memset(fname_entry, 0, sizeof(exfat_fnext_entry));
            cur_len = (fname_len) > 15 ? 15 :  fname_len;
            fname_len -= cur_len;
            fname_entry->entry_type = EXFAT_DTYPE_USED | EXFAT_DTYPE_FNEXT;
            fname_entry->secondary_flags = 0x00;
            for (i=0; i < cur_len; i++) {
                if (fname[idx] > 0x80 || fname[idx] <0x20)
                    debugf("Error: unsupported non-ascii code: %02x", fname[idx]);
                fname_entry->uniname[i] = (uint16_t)fname[idx++];
            }
            uint16_t upcase_name[15];
            for (i=0; i < cur_len; i++) {
                upcase_name[i] = exfat->uptab_buf[fname_entry->uniname[i]];
            }

            uint8_t *ptr;
            ptr = (uint8_t *)upcase_name;
            cur_len <<= 1;
            for (i=0; i<cur_len; i++) {
                val = (uint16_t)ptr[i];
                name_hash = ((name_hash&1) ? 0x8000 : 0) + (name_hash >>1) + (val&0xff);
            }
        }
    }
    strext_entry->name_hash = cpu_to_le16(name_hash);
    p_ent->prim = prim_entry;
    p_ent->strext = strext_entry;
    cal_checksum(p_ent);

    if (write_back_entry(exfat, p_ent) < 0) {
        debugf("Error to write_back_entry.\n");
        return NULL;
    }
    prim_entry->str_dent = strext_entry;
    return prim_entry;
}


static int exfat_read_entry(exfat_fs_t *exfat, const char *path, exfat_whole_dentry *p_ent, int target_type)
{
    exfat_file_entry *retdent;
    const char *cur, *start, *end;
    int name_len, is_last = 0, is_dir = 0;
    int result = NO_ERROR;
    char l_filename[FNAME_MAXLEN_BYTES] = {0};
    exfat_whole_dentry parent, current;

    memset(&parent, 0, sizeof(exfat_whole_dentry));
    memset(&current, 0, sizeof(exfat_whole_dentry));

    if (exfat_dentry_init(exfat, &parent) < 0) {
        result = ERR_NO_MEMORY;
        dprintf(INFO, "Error: buffer init failed!\n");
        goto exit;
    }

    if (exfat_dentry_init(exfat, &current) < 0) {
        result = ERR_NO_MEMORY;
        dprintf(INFO, "Error: buffer init failed!\n");
        goto exit;
    }

    cur = start = path;
    while (*cur != 0) {
        while (*cur == '/')
            cur++;
        start = cur;
        while (*cur != '/' && *cur != 0)
            cur++;
        end = cur;
        name_len = end - start;
        if (*cur == 0)
            is_last = 1;

        if (name_len >= FNAME_MAXLEN_BYTES)
            name_len = FNAME_MAXLEN_BYTES - 1;

        if (name_len > 210) {
            dprintf(INFO, "Warning: too long sysdump file name(len=%d) may cause buffer overflow, truncate it to 210.\n", name_len);
            name_len = 210;
        }
        memcpy(l_filename, start, name_len);
        l_filename[name_len] = 0;
        exfat_downcase(l_filename);

        if (is_last && target_type == EXFAT_ATTR_FILE)
            is_dir = 0;
        else if (is_last && target_type == EXFAT_ATTR_IGNORE)
            is_dir = -1;
        else
            is_dir = 1;

        retdent = find_dir_entry(exfat, &current, &parent, l_filename, is_dir);

        if (retdent == (exfat_file_entry *)(-1)) {
            dprintf(INFO, "Get error when find file entry\n");
            result = ERR_IO;
            goto exit;
        }
        current.parent = (void *)&parent;
        if (retdent == NULL) {
            result = ERR_NOT_FOUND;
            goto exit;
        } else {
            if (is_last != 1) {
                exfat_dentry_exchange(&parent, &current);
                exfat_dentry_clear(exfat, &current);
            } else {
                /* just to check whether the file exists. */
                if (p_ent == NULL) {
                    goto exit;
                }
                exfat_dentry_exchange(p_ent, &current);
                goto exit;
            }
        }
    }
exit:
    if(parent.bf0.pbuf)
        free_malloc(parent.bf0.pbuf);
    if(parent.bf1.pbuf)
        free_malloc(parent.bf1.pbuf);
    if(current.bf0.pbuf)
        free_malloc(current.bf0.pbuf);
    if(current.bf1.pbuf)
        free_malloc(current.bf1.pbuf);
    return result;
}

static int universal_file_make(exfat_fs_t *exfat, const char* path, int file_type, uint64_t f_len)
{
    const char *dn_start, *dn_end, *cur;
    char l_filename[FNAME_MAXLEN_BYTES];
    exfat_file_entry  *retdent;
    exfat_whole_dentry parent, current;
    uint32_t start_cl, start_sec, cur_cl;
    uint32_t cl_size = exfat->bytes_per_cluster;
    uint64_t size64, written_size;
    int name_len, cont, is_dir = 1, is_last = 0;
    int result = NO_ERROR;

    memset(&parent, 0, sizeof(exfat_whole_dentry));
    memset(&current, 0, sizeof(exfat_whole_dentry));

    if (exfat_dentry_init(exfat, &parent) < 0) {
        result = ERR_NO_MEMORY;
        dprintf(INFO, "Error: buffer init failed!\n");
        goto exit;
    }

    if (exfat_dentry_init(exfat, &current) < 0) {
        result = ERR_NO_MEMORY;
        dprintf(INFO, "Error: buffer init failed!\n");
        goto exit;
    }

    //check whether the file already exists.
    if(exfat_read_entry(exfat, path, NULL, EXFAT_ATTR_IGNORE) != ERR_NOT_FOUND){
        dprintf(INFO, "Error: %s already exists, skip rename\n", path);
        result = ERR_ALREADY_EXISTS;
        goto exit;
    }

    cur = dn_start = path;
    while (*cur != 0) {
        while(*cur == '/') {
            cur++;
        }
        dn_start = cur;

        while (*cur != '/' && *cur != 0) {
            cur++;
        }
        dn_end = cur;
        name_len = dn_end - dn_start;
        /* This is the last component */
        if (*cur == 0)
            is_last = 1;

        if (name_len >= FNAME_MAXLEN_BYTES)
            name_len = FNAME_MAXLEN_BYTES - 1;

        /* Too long name length may cause one dir entry cross 3 sector, which we can't process. */
        if (name_len > 210) {
            dprintf(INFO, "Warning: too long sysdump file name(len=%d) may cause buffer overflow, truncate it to 210.\n", name_len);
            name_len = 210;
        }
        memcpy(l_filename, dn_start, name_len);
        l_filename[name_len] = 0;
        exfat_downcase(l_filename);

        retdent = find_dir_entry(exfat, &current, &parent, l_filename, is_dir);

        if (retdent == (exfat_file_entry *)(-1)) {
            result = ERR_GENERIC;
            goto exit;
        }

        current.parent = (void *)&parent;

        if (is_last && file_type == EXFAT_ATTR_FILE)
            is_dir = 0;

        if (is_dir) {
            if (retdent == NULL) {
                // Create new dir
                cur_cl = 0;
                written_size = (uint64_t)cl_size;
                written_size = write_file_contents(exfat, &cur_cl, NULL, written_size, &cont, 1);
                if (written_size != cl_size) {
                    dprintf(INFO, "Error: writing contents\n");
                    result = ERR_IO;
                    goto exit;
                }
                retdent = new_dir_entry(exfat, &current, l_filename, cur_cl, written_size, cont, 1);
                if (retdent==NULL) {
                    dprintf(INFO, "Error to create a new file entry.\n");
                    result = ERR_IO;
                    goto exit;
                }
                start_cl = cur_cl;
            } else {
                start_cl = EXFAT2CPU32(retdent->str_dent->first_cluster);
            }
            start_sec  = exfat->data_begin + start_cl * exfat->sectors_per_cluster;

            exfat_dentry_exchange(&parent, &current);
            exfat_dentry_clear(exfat, &current);
        } else if (retdent == NULL){
            // Create new file
            cur_cl = 0;
            written_size = f_len;
            if (written_size != 0)
                written_size = write_file_contents(exfat, &cur_cl, NULL, written_size, &cont, 1);
            if (written_size != f_len) {
                debugf("Error: writing contents\n");
                result = ERR_IO;
                goto exit;
            }
            size64 = written_size;
            retdent = new_dir_entry(exfat, &current, l_filename, cur_cl, size64, cont, 0);
            if (retdent == NULL) {
                debugf("Error to create a new file entry.\n");
                result = ERR_IO;
                goto exit;
            }
            goto exit;
        }
    }

exit:
    if(parent.bf0.pbuf)
        free_malloc(parent.bf0.pbuf);
    if(parent.bf1.pbuf)
        free_malloc(parent.bf1.pbuf);
    if(current.bf0.pbuf)
        free_malloc(current.bf0.pbuf);
    if(current.bf1.pbuf)
        free_malloc(current.bf1.pbuf);
    return result;
}

/* Delete subfolders recursively */
static int sub_files_remove(exfat_fs_t *exfat, uint32_t start_cluster)
{
    exfat_buf_info buffer;
    exfat_dentry *dentptr;
    exfat_file_entry *prim;
    exfat_strext_entry *strext;
    exfat_fnext_entry *fnext;
    int search_time, i, clr_fatent, rd_sec = 1, result = 0, modify_flag = 0, is_dir = 0;
    uint32_t startsect, cl_size, cl_num, cl_start;
    uint16_t attr;

    startsect = exfat->data_begin + start_cluster * exfat->sectors_per_cluster;
    buffer.cluster = start_cluster;
    buffer.sectors = rd_sec;
    search_time = exfat->sectors_per_cluster;

    buffer.pbuf = exfat_malloc(exfat->bytes_per_sector+64);
    if (buffer.pbuf == NULL) {
        dprintf(INFO, "Error alloc buffer.\n");
        return -1;
    }
    while (search_time-- > 0) {
        if (get_exfat_blocks(exfat, startsect, buffer.pbuf, rd_sec) != 0) {
            dprintf(INFO, "Error: reading directory block......Return -1\n");
            result = -1;
            goto exit;
        }
        buffer.start_sec = startsect;
        startsect += rd_sec;

        dentptr = (exfat_dentry *)buffer.pbuf;
        for (i = 0; i < (buffer.sectors * exfat->bytes_per_sector / sizeof(exfat_dentry)); i++, dentptr++) {
            switch (dentptr->entry_type) {
                case (EXFAT_DTYPE_USED | EXFAT_DTYPE_FILE):
                    prim = (exfat_file_entry *)dentptr;
                    attr = EXFAT2CPU16(prim->attr);
                    is_dir = (attr == EXFAT_ATTR_DIR) ? 1 : 0;
                    prim->entry_type ^= EXFAT_DTYPE_USED;
                    modify_flag += 1;
                    break;
                case (EXFAT_DTYPE_USED | EXFAT_DTYPE_STREXT):
                    strext = (exfat_file_entry *)dentptr;
                    cl_size = exfat->bytes_per_sector * exfat->sectors_per_cluster;
                    cl_num = (EXFAT2CPU64(strext->data_len) + cl_size -1) / cl_size;
                    cl_start = EXFAT2CPU32(strext->first_cluster);
                    if (is_dir) {
                        result = sub_files_remove(exfat, cl_start);
                        if (result < 0) {
                            dprintf(INFO, "Error: sub files remove failed!\n");
                            goto exit;
                        }
                    }
                    clr_fatent = strext->secondary_flags == 0x03 ? 0 : 1;
                    if (clear_exfatent_bitmap(exfat, cl_start, cl_num, clr_fatent) < 0) {
                        dprintf(INFO, "Error: clear_exfatent_bitmap\n");
                        result = -1;
                        goto exit;
                    }
                    strext->entry_type ^= EXFAT_DTYPE_USED;
                    modify_flag += 1;
                    break;
                case (EXFAT_DTYPE_USED | EXFAT_DTYPE_FNEXT):
                    fnext = (exfat_file_entry *)dentptr;
                    fnext->entry_type ^= EXFAT_DTYPE_USED;
                    modify_flag += 1;
                    break;
                default:
                    continue;
            }
        }
        if (modify_flag > 0) {
            if (exfat_disk_write(exfat, buffer.start_sec, buffer.sectors, buffer.pbuf) < 0) {
                dprintf(INFO, "Error: subfiles remove\n");
                result = -1;
        }
            modify_flag = 0;
        }
    }

exit:
    if (buffer.pbuf)
        free_malloc(buffer.pbuf);
    return result;
}

static int universal_open_file(exfat_fs_t *exfat, const char *path, filecookie **fcookie, int type)
{
    exfat_file_t *file = NULL;
    exfat_whole_dentry tmp_dent;
    exfat_whole_dentry *p_ent = &tmp_dent;
    int result;

    memset(p_ent, 0, sizeof(exfat_whole_dentry));

    if (exfat_dentry_init(exfat, p_ent) < 0) {
        result = ERR_NO_MEMORY;
        dprintf(INFO, "Error: buffer init failed!\n");
        goto exit;
    }

    result = exfat_read_entry(exfat, path, p_ent, type);
    if(result < 0) {
        dprintf(INFO, "Error: can't find target file!");
        goto exit;
    }

    file = exfat_malloc(sizeof(exfat_file_t));
    if (file == NULL) {
        dprintf(INFO, "Error: malloc file memory!");
        result = ERR_NO_MEMORY;
        goto exit;
    }
    file->exfat_fs = exfat;
    file->start_cluster = p_ent->strext->first_cluster;
    file->length = p_ent->strext->data_len;
    file->attributes = p_ent->prim->attr;
    file->cont_flag = (p_ent->strext->secondary_flags==0x03) ? 1 : 0;
    memcpy(&(file->file_ent), p_ent, sizeof(exfat_whole_dentry));
    result = NO_ERROR;

    *fcookie = (filecookie *)file;
    return result;

exit:
    if (p_ent->bf0.pbuf)
        free_malloc(p_ent->bf0.pbuf);
    if (p_ent->bf1.pbuf)
        free_malloc(p_ent->bf1.pbuf);
    return result;
}
status_t exfat_open_file(fscookie *cookie, const char *path, filecookie **fcookie)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    status_t result;

    dprintf(INFO, "exfat_open_file path is %s\n", path);
    result = universal_open_file(exfat, path, fcookie, EXFAT_ATTR_FILE);
    if (result < 0) {
        dprintf(INFO, "open %s failed\n", path);
    }
    return result;
}

status_t exfat_close_file(filecookie **fcookie)
{
    exfat_file_t *file = (exfat_file_t *)fcookie;
    if (file == NULL) {
        dprintf(INFO, "file invalid\n!");
        return ERR_NOT_VALID;
    }
    if(file->file_ent.bf0.pbuf) {
        free_malloc(file->file_ent.bf0.pbuf);
    }
    if(file->file_ent.bf1.pbuf) {
        free_malloc(file->file_ent.bf1.pbuf);
    }
    free_malloc(file);
    return NO_ERROR;
}

status_t exfat_open_dir(fscookie *cookie, const char *path, dircookie **dcookie)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    status_t result;

    result = universal_open_file(exfat, path, dcookie, EXFAT_ATTR_DIR);
    if (result < 0)
        dprintf(INFO, "open %s failed\n", path);
    return result;
}

status_t exfat_close_dir(dircookie **dcookie)
{
    exfat_file_t *file = (exfat_file_t *)dcookie;
    if (file == NULL) {
        dprintf(INFO, "dir invalid\n!");
        return ERR_NOT_VALID;
    }

    if(file->file_ent.bf0.pbuf) {
        free_malloc(file->file_ent.bf0.pbuf);
    }

    if(file->file_ent.bf1.pbuf) {
        free_malloc(file->file_ent.bf1.pbuf);
    }
    free_malloc(file);
    return NO_ERROR;
}

status_t exfat_make_dir(fscookie *cookie, const char *path)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    status_t result;

    result = universal_file_make(exfat, path, EXFAT_ATTR_DIR, 0);
    if (result < 0)
        dprintf(INFO,"make dir failed\n");

    return result;
}

status_t exfat_make_file(fscookie *cookie, const char *path, filecookie **fcookie, uint64_t f_len)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    status_t result;

    result = universal_file_make(exfat, path, EXFAT_ATTR_FILE, f_len);
    if(result < 0){
        dprintf(INFO,"make file failed\n");
        return result;
    }
    result = exfat_open_file(exfat, path, fcookie);

    return result;
}

ssize_t exfat_write_file(filecookie *fscookie, void *buf, off_t offset, size_t data_len)
{
    exfat_file_t *file = (exfat_file_t *)fscookie;
    exfat_fs_t *exfat = file->exfat_fs;
    ssize_t result;
    uint32_t start_cluster, cl_num, cl_size;
    uint64_t written_size;
    int cont;

    cl_size = exfat->bytes_per_cluster;
    cont = file->cont_flag;
    start_cluster = file->start_cluster;

    //clear previous centent of existing file (fat table & bitmap).

    if (file->start_cluster) {
        cl_num = (EXFAT2CPU64(file->length) + cl_size - 1) / cl_size;
        if (clear_exfatent_bitmap(exfat, start_cluster, cl_num, cont) < 0) {
            dprintf(INFO, "Error: clear_exfatent_bitmap\n");
            return ERR_IO;
        }
    }

    if (check_available_size(exfat, (uint64_t)data_len) < 0) {
        dprintf(INFO, "Error: clear_exfatent_bitmap\n");
        return ERR_IO;
    }

    cont = 0;
    written_size = write_file_contents(exfat, &start_cluster, buf, data_len, &cont, 0);
    if (written_size == 0) {
        dprintf(INFO, "Error: writting contents\n");
        return ERR_IO;
    }

    update_dir_entry(exfat, &(file->file_ent), start_cluster, written_size, cont);

    if (write_back_entry(exfat, &(file->file_ent)) < 0) {
        dprintf(INFO, "Error to write_back_entry.\n");
        return ERR_IO;
    }

    result = (ssize_t)written_size;

    if (flush_exfatent(exfat) < 0) {
        dprintf(INFO, "Error to flush exfatent.\n");
        return ERR_IO;
    }
    if (flush_bitmap(exfat) < 0 ) {
        dprintf(INFO, "Error to flush bitmap.\n");
        return ERR_IO;
    }

    return result;
}

status_t exfat_remove(fscookie *cookie, const char *path)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    exfat_whole_dentry tmp_dent;
    exfat_whole_dentry *p_ent = &tmp_dent;
    status_t result = NO_ERROR;
    uint16_t attr;
    uint32_t sub_cluster = 0;
    uint32_t cl_size, cl_num;
    int clr_fatent;

    memset(p_ent, 0, sizeof(exfat_whole_dentry));

    if (exfat_dentry_init(exfat, p_ent) < 0) {
        result = ERR_NO_MEMORY;
        dprintf(INFO, "Error: buffer init failed!\n");
        goto exit;
    }

    result = exfat_read_entry(exfat, path, p_ent, EXFAT_ATTR_IGNORE);
    if(result < 0) {
        dprintf(INFO, "Error: can't find target file!\n");
        goto exit;
    }

    attr = EXFAT2CPU16(p_ent->prim->attr);

    if(attr == EXFAT_ATTR_DIR) {
        sub_cluster = p_ent->strext->first_cluster;
        if(sub_files_remove(exfat, sub_cluster) < 0) {
            dprintf(INFO, "sub files remove failed, please check!\n");
            result = ERR_IO;
            goto exit;
        }
    }

    p_ent->prim->entry_type ^= EXFAT_DTYPE_USED;
    p_ent->strext->entry_type ^= EXFAT_DTYPE_USED;
    p_ent->fname_dent[0]->entry_type ^= EXFAT_DTYPE_USED;

    if (write_back_entry(exfat, p_ent) < 0) {
        dprintf(INFO, "clean entry failed, please check!\n");
        result = ERR_IO;
        goto exit;
    }

    cl_size = exfat->bytes_per_cluster;
    cl_num = (EXFAT2CPU64(p_ent->strext->data_len) + cl_size -1) / cl_size;
    clr_fatent = p_ent->strext->secondary_flags == 0x03 ? 0 : 1;
    clear_exfatent_bitmap(exfat, sub_cluster, 1, clr_fatent);

    dprintf(INFO, "remove %s succeed!\n", path);

exit:
    if (p_ent->bf0.pbuf)
        free_malloc(p_ent->bf0.pbuf);
    if (p_ent->bf1.pbuf)
        free_malloc(p_ent->bf1.pbuf);
    return result;
}

status_t exfat_rename(fscookie *cookie, const char *src, const char *dest)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    exfat_whole_dentry tmp_dent;
    exfat_whole_dentry *p_ent = &tmp_dent;
    status_t result;
    size_t fname_len;
    char dest_name[FNAME_MAXLEN_BYTES] = {0};
    uint16_t name_hash = 0;
    uint16_t val = 0;
    int i, j, idx;
    int fname_num, fname_dent_idx = 0;
    uint16_t upcase_name[15];
    uint8_t *ptr;
    char *tmp_path;

    /* Used to save the parent path of src, and the full name of dest path */
    tmp_path = exfat_malloc(strlen(src) + FNAME_MAXLEN_BYTES);
    if (tmp_path == NULL) {
        dprintf(INFO, "Error alloc buffer.\n");
        return ERR_NO_MEMORY;
    }

    if (split_parent_path(src, tmp_path) < 0) {
        dprintf(INFO, "Error: split parent filename failed!\n");
        result = ERR_BAD_PATH;
        goto exit;
    }
    memcpy(tmp_path + strlen(tmp_path), dest, strlen(dest));

    memset(p_ent, 0, sizeof(exfat_whole_dentry));

    if (exfat_dentry_init(exfat, p_ent) < 0) {
        result = ERR_NO_MEMORY;
        dprintf(INFO, "Error: buffer init failed!\n");
        goto exit;
    }

    result = exfat_read_entry(exfat, tmp_path, NULL, EXFAT_ATTR_IGNORE);
    if (result != ERR_NOT_FOUND){
        if (result != ERR_IO && result != ERR_NO_MEMORY)
            dprintf(INFO, "Error: %s already exists, skip rename\n", dest);
        goto exit;
    }

    result = exfat_read_entry(exfat, src, p_ent, EXFAT_ATTR_IGNORE);
    if (result < 0){
        dprintf(INFO, "Error: do not find the target file's entry\n");
        goto exit;
    }

    if (split_last_name(dest, dest_name) < 0) {
        dprintf(INFO, "Error: split last filename failed!\n");
        goto exit;
    }

    fname_len = strlen(dest_name);
    if (fname_len > 15) {
        dprintf(INFO, "Error: dir name is too long, should be shoter than 15, please check!\n");
        goto exit;
    }

    fname_num = p_ent->prim->secondary_cnt - 1;
    while (fname_dent_idx < fname_num) {
        if (fname_dent_idx > 0) {
            memset(p_ent->fname_dent[fname_dent_idx], 0, sizeof(exfat_fnext_entry));
        } else {
            idx = 0;
            memset(p_ent->fname_dent[fname_dent_idx]->uniname, 0, sizeof(uint16_t) * 15);
            for (i = 0; i < fname_len; i++) {
                p_ent->fname_dent[fname_dent_idx]->uniname[i] = (uint16_t)dest_name[idx++];
                upcase_name[i] = exfat->uptab_buf[p_ent->fname_dent[fname_dent_idx]->uniname[i]];
            }
        }
        fname_dent_idx++;
    }
    p_ent->strext->name_hash = cpu_to_le16(0x2345);
    p_ent->strext->name_len = (uint8_t)(fname_len&0xff);
    p_ent->prim->secondary_cnt = 2;
    ptr = (uint8_t *) upcase_name;
    fname_len <<= 1;
    for (j = 0; j < fname_len; j++) {
        val = (uint16_t)ptr[j];
        name_hash = ((name_hash&1) ? 0x8000 : 0) + (name_hash >> 1) + (val&0xff);
    }
    p_ent->strext->name_hash = cpu_to_le16(name_hash);
    cal_checksum(p_ent);
    if(write_back_entry(exfat, p_ent) < 0) {
        dprintf(INFO, "Write back entry failed!\n");
        goto exit;
    }

    result = NO_ERROR;
    dprintf(INFO, "rename succeed\n");

exit:
    if (p_ent->bf0.pbuf)
        free_malloc(p_ent->bf0.pbuf);
    if (p_ent->bf1.pbuf)
        free_malloc(p_ent->bf1.pbuf);
    free_malloc(tmp_path);
    return result;
}

status_t exfat_stat_fs(fscookie *cookie, struct fs_stat *stat)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    uint64_t cl_avail = 0;

    cl_avail = get_available_size(exfat);

    stat->free_space = cl_avail * exfat->bytes_per_cluster;
    stat->total_space = (uint64_t)exfat->total_sectors * (uint64_t)exfat->bytes_per_sector;
    stat->free_inodes = cl_avail;
    stat->total_inodes = exfat->total_sectors / exfat->sectors_per_cluster;
    return 0;
}

status_t exfat_mount(block_dev_desc_t *dev, fscookie **cookie, disk_partition_t *part_info)
{
    uint32_t cl_size;
    uint32_t startsect;
    exfat_buf_array buf_array;
    exfat_boot_sector bs;
    status_t result = ERR_GENERIC;

    if (!dev) {
        dprintf(INFO, "Error: invalid device\n");
        return ERR_NOT_VALID;
    }

    exfat_bits_table_init();

    exfat_fs_t *exfat = exfat_malloc(sizeof(exfat_fs_t));
    if (exfat == NULL) {
        dprintf(INFO, "Error: allocating memory\n");
        result = ERR_NO_MEMORY;
        goto exit;
    }
    exfat->dev = dev;
    exfat->total_part_size = (uint64_t)part_info->blk_cnt;
    exfat->part_start = (uint64_t)part_info->start_blk;
    exfat->part_block_size = (uint64_t)part_info->blksz;
    exfat->fatbuf = NULL;
    exfat->bitmap = NULL;
    exfat->uptab_buf = NULL;
    exfat->bitmap_fatbuf = NULL;

    dprintf(INFO, "get_mmc total_part_size = %llu\n",exfat->total_part_size);
    dprintf(INFO, "get_mmc part_start = 0x%llu\n",exfat->part_start);
    dprintf(INFO, "get_mmc part_block_size = %llu\n",exfat->part_block_size);

    if (exfat_valid_check(exfat) < 0) {
        dprintf(INFO, "Error: exfat_valid_check failed.\n");
        result = ERR_GENERIC;
        goto exit;
    }

    if (read_exfat_bootsec(exfat, &bs)) {
        dprintf(INFO, "Error: reading boot sector\n");
        result = ERR_GENERIC;
        goto exit;
    }

    exfat->bytes_per_sector = 1 << bs.sector_size_bits;
    exfat->sectors_per_cluster = 1 << bs.sectors_per_clu_bits;
    cl_size = exfat->bytes_per_sector * exfat->sectors_per_cluster;

    exfat->fat_num = bs.num_fats;
    exfat->fatlength = bs.fat_len;
    exfat->bitmaplen = (bs.cluster_cnt + 7) / 8;
    exfat->cluster_cnt = bs.cluster_cnt;
    exfat->fat_sect = bs.fat_off;
    exfat->root_cluster = bs.root_cluster;
    exfat->rootdir_sect = bs.cluster_heap_off + (bs.root_cluster-2) * exfat->sectors_per_cluster;
    exfat->data_begin = bs.cluster_heap_off - (exfat->sectors_per_cluster * 2);
    exfat->rd_sectors = (cl_size > MAX_RD_BLK_SIZE) ? (MAX_RD_BLK_SIZE /exfat->bytes_per_sector) : exfat->sectors_per_cluster;
    exfat->bytes_per_cluster = exfat->sectors_per_cluster * exfat->bytes_per_sector;
    exfat->vol_len = bs.vol_len;

    exfat->fatbuf_idx = 0;
    exfat->fatbuf = exfat_malloc(exfat->bytes_per_sector);
    if (exfat->fatbuf == NULL) {
        dprintf(INFO, "Error: allocating memory\n");
        result = ERR_NO_MEMORY;
        goto exit;
    }

    exfat->bitmap_idx = 0;
    exfat->bitmap = exfat_malloc(exfat->bytes_per_sector);
    if (exfat->bitmap == NULL) {
        dprintf(INFO, "Error: allocating memory\n");
        result = ERR_NO_MEMORY;
        goto exit;
    }

    memset(&buf_array, 0, sizeof(buf_array));
    startsect = exfat->rootdir_sect;

    if (exfat_path_init(exfat, &buf_array, startsect) < 0) {
        dprintf(INFO, "Error: exfat_path_init failed.\n");
        result = ERR_GENERIC;
        goto exit;
    }

    if (exfat->bitmap_sect == 0) {
        dprintf(INFO, "Error: exfat bitmap is not found\n");
        result = ERR_NOT_FOUND;
        goto exit;
    }

    if (exfat->uptab_buf == NULL) {
        dprintf(INFO, "Error: exfat upcase table is not found\n");
        result = ERR_NOT_FOUND;
        goto exit;
    }

    exfat->bitmap_fatbuf_size = EXFAT_FAT_ENTRY_SIZE * (exfat->bitmaplen + cl_size - 1) / cl_size;
    exfat->bitmap_fatbuf_size = (exfat->bitmap_fatbuf_size + exfat->bytes_per_sector -1) / exfat->bytes_per_sector;
    exfat->bitmap_fatbuf = exfat_malloc(exfat->bitmap_fatbuf_size * exfat->bytes_per_sector);
    if (exfat->bitmap_fatbuf == NULL) {
        dprintf(INFO, "Error: allocating memory\n");
        result = ERR_GENERIC;
        goto exit;
    }

    if (check_bitmap_fatbuf(exfat) < 0) {
        dprintf(INFO, "Error: read and check fatbuf for bitmap failed.\n");
        result = ERR_GENERIC;
        goto exit;
    }

    *cookie = (fscookie *)exfat;

    return NO_ERROR;
exit:
    free_exfat(exfat);
    return result;
}

status_t exfat_unmount(fscookie *cookie)
{
    exfat_fs_t *exfat = (exfat_fs_t *)cookie;
    free_exfat(exfat);
    return NO_ERROR;
}
static const struct fs_api exfat_api = {
    .mount = exfat_mount,
    .unmount = exfat_unmount,
    .fs_stat = exfat_stat_fs,
    .open = exfat_open_file,
    .write = exfat_write_file,
    .create = exfat_make_file,
    .mkdir = exfat_make_dir,
    .opendir = exfat_open_dir,
    .closedir = exfat_close_dir,
    .remove = exfat_remove,
    .rename = exfat_rename,
    .close = exfat_close_file,
    .format = exfat_format,
};

STATIC_FS_IMPL(exfat, &exfat_api);


