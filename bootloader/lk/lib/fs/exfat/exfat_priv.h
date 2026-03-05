#pragma once

#include "exfat.h"

typedef void *fsfilecookie;

//status_t exfat_format();
status_t exfat_mount(block_dev_desc_t *dev, fscookie **cookie, disk_partition_t *part_info);
status_t exfat_unmount(fscookie *cookie);
status_t exfat_stat_fs(fscookie *cookie, struct fs_stat *stat);

/* file api */
ssize_t exfat_read_file(filecookie *fcookie, void *buf, off_t offset, size_t len);
ssize_t exfat_write_file(filecookie *fscookie, void *buf, off_t offset, size_t data_len);
status_t exfat_open_file(fscookie *cookie, const char *path, filecookie **fcookie);
status_t exfat_stat_file(filecookie *fcookie, struct file_stat *stat);
status_t exfat_make_file(fscookie *cookie, const char *path, filecookie **fcookie, uint64_t f_len);
status_t exfat_mkdir(fscookie *cookie, const char *path_name);
status_t exfat_open_dir(fscookie *cookie, const char *path, dircookie **dcookie);
status_t exfat_close_dir(dircookie **dircookie);
status_t exfat_remove(fscookie *cookie, const char *path);
status_t exfat_rename(fscookie *cookie, const char *scr, const char *dest);
status_t exfat_close_file(filecookie **fcookie);
status_t exfat_format(block_dev_desc_t *dev, const void *args, disk_partition_t *part_info);