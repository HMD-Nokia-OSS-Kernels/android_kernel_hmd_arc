/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#define MUSB_DMA_PAUSE	0x1000
#define MUSB_DMA_FRAG_WAIT	0x1004
#define MUSB_DMA_INTR_RAW_STATUS	0x1008
#define MUSB_DMA_INTR_MASK_STATUS	0x100C
#define MUSB_DMA_REQ_STATUS	0x1010
#define MUSB_DMA_EN_STATUS	0x1014
#define MUSB_DMA_DEBUG_STATUS	0x1018

#define MUSB_DMA_CHN_PAUSE(n)		(0x1C00 + (n - 1) * 0x20)
#define MUSB_DMA_CHN_CFG(n)		(0x1C04 + (n - 1) * 0x20)
#define MUSB_DMA_CHN_INTR(n)		(0x1C08 + (n - 1) * 0x20)
#define MUSB_DMA_CHN_ADDR(n)		(0x1C0C + (n - 1) * 0x20)
#define MUSB_DMA_CHN_LEN(n)		(0x1C10 + (n - 1) * 0x20)
#define MUSB_DMA_CHN_LLIST_PTR(n)	(0x1C14 + (n - 1) * 0x20)
#define MUSB_DMA_CHN_BYTE_CNT(n)	(0x1C18 + (n - 1) * 0x20)
#define MUSB_DMA_CHN_REQ(n)		(0x1C1C + (n - 1) * 0x20)

#define musb_read_dma_addr(mbase, bchannel)	\
	musb_readl(mbase,	\
		   MUSB_DMA_CHN_ADDR(bchannel))

#define musb_write_dma_addr(mbase, bchannel, addr) \
	musb_writel(mbase, \
		    MUSB_DMA_CHN_ADDR(bchannel), \
		    addr)

#define CHN_EN			0x01
#define CHN_LLIST_INT_EN	0x04
#define CHN_START_INT_EN	0x08
#define CHN_USBRX_INT_EN	0x10

#define CHN_LLIST_INT_MASK_STATUS	0x040000
#define CHN_START_INT_MASK_STATUS	0x080000
#define CHN_USBRX_INT_MASK_STATUS	0x100000

#define CHN_LLIST_INT_CLR	0x04000000
#define CHN_START_INT_CLR	0x08000000
#define CHN_USBRX_LAST_INT_CLR	0x10000000


#define MUSB_DMA_CHANNELS	30
#define MUSB_MAX_BLOCK_LEN	0xfc00
#define MUSB_LINKLIST_NODES	64

struct sprd_musb_dma_controller;

struct sprd_musb_dma_channel {
	struct sprd_dma_channel	channel;
	struct sprd_musb_dma_controller	*controller;
	u16	max_packet_sz;
	u8	idx;
	u8	transmit;
	dma_addr_t	addr;
	unsigned short	blk_len;
	unsigned short	frag_len;
	u8		ep_num;
	u8		ioc;
	u8		sp;
	u8		list_end;
};

struct sprd_musb_dma_controller {
	struct dma_controller	controller;
	struct sprd_musb_dma_channel	channel[MUSB_DMA_CHANNELS];
	void	*private_data;
	void __iomem	*base;
	u32	used_channels;
};

typedef struct linklist_node_s {
  	unsigned int 	addr;
  	unsigned short  frag_len;
  	unsigned short  blk_len;
 	unsigned int	list_end :1;
 	unsigned int	sp :1;
  	unsigned int	ioc :1;
 	unsigned int	reserved:5;
 	unsigned int	data_addr :4;
 	unsigned int	pad :20;
#ifndef CONFIG_USB_SPRD_DMA_V0
  	unsigned int reserved2;
#endif
} linklist_node_t;

irqreturn_t sprd_dma_interrupt(struct musb *musb, u32 int_hsdma);
struct dma_controller *dma_controller_create(struct musb *musb,
							void __iomem *base);
void dma_controller_destroy(struct dma_controller *c);
