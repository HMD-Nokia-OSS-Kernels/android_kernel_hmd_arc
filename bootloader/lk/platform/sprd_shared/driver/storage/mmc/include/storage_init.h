#ifndef __STORAGE_INIT_H
#define __STORAGE_INIT_H
int board_mmc_initialize(void);
int mmc_scan_delay(uint host, uint slave, uint speed_mode, uint32_t start_blk,
			u32 *delay_value);
int mmc_finished_scan_delay(uint host, uint slave);

#endif /* __STORAGE_INIT_H */
