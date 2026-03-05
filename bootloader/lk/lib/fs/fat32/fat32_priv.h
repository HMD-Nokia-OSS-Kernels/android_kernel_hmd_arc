/*
 * Copyright (c) 2015 Steve White
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

//#include <lib/bio.h>
#include <part.h>
#include <lib/fs.h>
#include "fat_fs.h"

typedef void *fsfilecookie;

status_t fat32_mount(block_dev_desc_t *dev, fscookie **cookie, disk_partition_t *part_info);
status_t fat32_unmount(fscookie *cookie);
int disk_read(fat_fs_t *fat, uint32_t block, uint32_t nr_blocks, void *buf);
int disk_write(fat_fs_t *fat, uint32_t block, uint32_t nr_blocks, void *buf);

/* file api */
status_t fat32_open_file(fscookie *cookie, const char *path, filecookie **fcookie);
ssize_t fat32_read_file(filecookie *fcookie, void *buf, off_t offset, size_t len);
ssize_t fat32_write_file(filecookie *fcookie, const void *buf, off_t offset, size_t len);
status_t fat32_close_file(filecookie *fcookie);
status_t fat32_stat_file(filecookie *fcookie, struct file_stat *stat);
status_t fat32_create_file(fscookie *cookie, const char *path, filecookie **fcookie, uint64_t len);
status_t fat32_rename(fscookie *cookie, const char *scr, const char *dest);
status_t fat_remove(fscookie *cookie, const char *path);
status_t fat_open_dir(fscookie *cookie, const char *path, dircookie **dcookie);
status_t fat_make_dir(fscookie *cookie, const char *path);
status_t fat_read_dir(dircookie *dircookie, struct dirent *dirinfo);
status_t fat_close_dir(dircookie *dircookie);
status_t fat32_stat_fs(fscookie *cookie, struct fs_stat *stat);
status_t fat_format(block_dev_desc_t *dev, const void *args, disk_partition_t *part_info);



