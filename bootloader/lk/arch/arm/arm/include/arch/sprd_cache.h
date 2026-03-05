/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef __ARCH_CACHE_H
#define __ARCH_CACHE_H

void	icache_disable(void);
void	dcache_disable(void);
void	enable_caches(void);
void	flush_cache   (unsigned long, unsigned long);
void	flush_dcache_all(void);
void	flush_dcache_range(unsigned long start, unsigned long stop);
void	invalidate_dcache_range(unsigned long start, unsigned long stop);
void	invalidate_dcache_all(void);
void	invalidate_icache_all(void);
void	v7_inval_tlb(void);
void	mmu_page_table_flush(unsigned long start, unsigned long stop);
int mmu_enabled(void);
void mmu_setup(void);
#endif

