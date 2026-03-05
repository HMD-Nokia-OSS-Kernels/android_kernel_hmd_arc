/******************************************************************************
 ** File Name:    sprdfb_round_corner.h                                           *
 ** Author:                                                           *
 ** DATE:                                                           *
 ** Copyright:    2005 Spreatrum, Incoporated. All Rights Reserved.           *
 ** Description:                                                            *
 ******************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------*
 ** DATE          NAME            DESCRIPTION                                 *
 **
 ******************************************************************************/

#ifndef __SPRD_ROUND_CORNER_H_
#define __SPRD_ROUND_CORNER_H_

#include <stdlib.h>
#include "sprd_dispc.h"
#include "sprd_panel.h"
#include "malloc.h"

#define STEP (256)

#define CORNER_ERR     -1
#define CORNER_DONE    0

void dispc_logo_show_corner(int height, int width, uint32_t addr);

#endif
