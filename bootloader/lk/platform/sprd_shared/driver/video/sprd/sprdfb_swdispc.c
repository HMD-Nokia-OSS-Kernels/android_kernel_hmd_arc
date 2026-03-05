#include "sprdfb.h"
#include "sprdfb_spi_panel.h"

extern int32_t sprdfb_spi_refresh(struct sprdfb_device *dev);

struct display_ctrl sprdfb_swdispc_ctrl = {
	.name		= "swdispc",
	.early_init	= NULL,
	.init		= NULL,
	.uninit		= NULL,
	.refresh	= sprdfb_spi_refresh,
	.update_clk     = NULL,
};

