/*
 * MUSB OTG driver DMA controller abstraction
 */

#ifndef __LK_MUSB_DMA_H__
#define __LK_MUSB_DMA_H__

//struct musb_hw_ep;

#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)

#define	is_dma_capable()	(1)

enum dma_channel_status {
	MUSB_DMA_STATUS_UNKNOWN,
	MUSB_DMA_STATUS_FREE,
	MUSB_DMA_STATUS_BUSY,
	MUSB_DMA_STATUS_BUS_ABORT,
	MUSB_DMA_STATUS_CORE_ABORT
};

/*
 * dma channel info struct
 */
struct sprd_dma_channel {
	void			*private_data;
	size_t			max_len;
	size_t			actual_len;
	enum dma_channel_status	status;
	bool			desired_mode;
};

/*
 *  function of DMA Controller.
 */
struct dma_controller {
	int	(*start)(struct dma_controller *dma_ctrl);
	int	(*stop)(struct dma_controller *dma_ctrl);
	struct sprd_dma_channel *(*channel_alloc)(struct dma_controller *dma_ctrl,
					struct musb_hw_ep *hwep, u8 is_tx);
	void	(*channel_release)(struct sprd_dma_channel *dma_ctrl);
	int	(*channel_program)(struct sprd_dma_channel *channel,u16 max_packet,
					u8 mode, dma_addr_t addr_dma, u32 length);
	int	(*channel_abort)(struct sprd_dma_channel *);
	int	(*is_compatible)(struct sprd_dma_channel *channel, u16 maxpacket,
					void *buf, u32 length);
};

static inline enum dma_channel_status dma_channel_status(struct sprd_dma_channel *c)
{
	return (is_dma_capable() && c) ? c->status : MUSB_DMA_STATUS_UNKNOWN;
}

extern void dma_controller_destroy(struct dma_controller *);

extern struct dma_controller *__init dma_controller_create(struct musb *,
	       						void __iomem *);

#endif	/* __LK_MUSB_DMA_H__ */
