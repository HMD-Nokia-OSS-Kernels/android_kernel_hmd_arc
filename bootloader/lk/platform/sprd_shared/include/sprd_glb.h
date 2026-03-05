/*
 *  sprd_glb.h - Support register operation interface
 *
 *  Copyright (C) 2021 Unisoc Communications Inc.
 *  History:
 *      2021/7/29 porter.xu@unisoc.com
 *      Add register operation interface
 */

#ifndef __SPRD_GLB_H__
#define __SPRD_GLB_H__

#include <asm/types.h>
/**
 * sci_glb_read - read value from d-die global register
 * @reg: global register address
 * @msk:
 *
 * If read all bits, set msk = -1
 *
 * Return read value and mask.
 */
u32 sci_glb_read(u32 reg, u32 msk);

/**
 * sci_glb_write - safely write value to d-die global register
 * @reg: global register address
 * @val:
 * @msk:
 *
 * If write all bits, set msk = -1
 *
 * Returns success (0) or negative errno.
 */
int sci_glb_write(u32 reg, u32 val, u32 msk);

/**
 * sci_glb_set - force set bit to d-die global register
 * @reg: global register address
 * @bit: commonly, only set one bit
 *
 *
 * tiger use stand-alone xxx_set/xxx_clr address for all global register
 *
 * Returns success (0) or negative errno.
 */
int sci_glb_set(u32 reg, u32 bit);

/**
 * sci_glb_clr - force clr bit to d-die gglobal register
 * @reg: global register address
 * @bit: commonly, only clear one bit
 *
 *
 * tiger use stand-alone xxx_set/xxx_clr address for all d-die global register
 *
 * Returns success (0) or negative errno.
 */
int sci_glb_clr(u32 reg, u32 bit);

#endif
