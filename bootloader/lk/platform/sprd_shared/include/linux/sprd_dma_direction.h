#ifndef _LINUX_SPRD_DMA_DIRECTION_H
#define _LINUX_SPRD_DMA_DIRECTION_H
/*
 These definitions reflect the definitions in pci.h
 so they can be used interchangeably with the corresponding PCI_ counterparts.
 */
enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};
#endif
