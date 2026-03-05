/**
 * core.c - DesignWare USB3 DRD Controller Core file
 */

#include <malloc.h>
#include <asm/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <asm/arch/sprd_reg.h>
#include "core.h"
#include "gadget.h"
#include "io.h"
#include "sprd_usb3_def.h"

#define CONFIG_SYS_CACHELINE_SIZE 64

/*
 * Issues core soft reset and PHY reset
 */
static int dwc3_core_soft_reset(struct dwc3 *dwc)
{
	u32 val;

	/* before Resetting PHY, put Core in Reset */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GCTL);
	val |= SPRD_DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, SPRD_DWC3_GCTL, val);

	/* assert USB3 PHY reset */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GUSB3PIPECTL(0));
	val |= SPRD_DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->regs, SPRD_DWC3_GUSB3PIPECTL(0), val);

	/* assert USB2 PHY reset */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0));
	val |= SPRD_DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0), val);

	mdelay(100);

	/* clear USB3 PHY reset */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GUSB3PIPECTL(0));
	val &= ~SPRD_DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->regs, SPRD_DWC3_GUSB3PIPECTL(0), val);

	/* clear USB2 PHY reset */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0));
	val &= ~SPRD_DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0), val);

	mdelay(100);

	/* after PHYs are stable we can take Core out of reset state */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GCTL);
	val &= ~SPRD_DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, SPRD_DWC3_GCTL, val);

	return 0;
}

/*
 * Allocates one event buffer structure
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
static struct dwc3_event_buffer *dwc3_alloc_one_event_buffer(struct dwc3 *dwc,
		unsigned length)
{
	struct dwc3_event_buffer *evt;

	evt = kzalloc(sizeof(*evt), GFP_KERNEL);
	if (!evt) {
		return ERR_PTR(-ENOMEM);
	}

	evt->length = length;
	evt->dwc = dwc;
#ifdef CONFIG_X86
	evt->buf	= 0xE601E000;
#else
	evt->buf	= EVT_BUF_ADDR; //memalign(length, length);
#endif
	evt->dma = evt->buf;
	if (!evt->buf) {
		return ERR_PTR(-ENOMEM);
	}
	return evt;
}

/*
 * Frees one event buffer
 */
static void dwc3_free_one_event_buffer(struct dwc3 *dwc,
		struct dwc3_event_buffer *deb)
{
	//dma_free_coherent(evt->buf);
}

/*
 * frees all allocated event buffers
 */
static void dwc3_free_event_buffers(struct dwc3 *dwc)
{
	struct dwc3_event_buffer *deb;
	int i;

	for (i = 0; i < dwc->num_event_buffers; i++) {
		deb = dwc->ev_buffs[i];
		if (deb) {
			dwc3_free_one_event_buffer(dwc, deb);
		}
	}
}

/*
 * Allocates @num event buffers of size @length
 *
 * Returns 0 on success otherwise negative errno.
 * In the error case, dwc may contain some buffers allocated
 * but not all which were requested.
 */
static int dwc3_alloc_event_buffers(struct dwc3 *dwc, unsigned length)
{
	int i;
	int num;

	num = SPRD_DWC3_NUM_INT(dwc->hwparams.hwparams1);
	dwc->num_event_buffers = num;

	dwc->ev_buffs = memalign(CONFIG_SYS_CACHELINE_SIZE,
				 sizeof(*dwc->ev_buffs) * num);
	if (!dwc->ev_buffs) {
		return -ENOMEM;
	}

	for (i = 0; i < num; i++) {
		struct dwc3_event_buffer *deb;

		deb = dwc3_alloc_one_event_buffer(dwc, length);
		if (IS_ERR(deb)) {
			dev_err(dwc->dev, "can't alloc event buffer \n");
			return -1;
		}
		dwc->ev_buffs[i] = deb;
	}

	return 0;
}

/*
 * setup our allocated event buffers
 */
static int dwc3_event_buffers_setup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer *deb;
	int i;

	for (i = 0; i < dwc->num_event_buffers; i++) {
		deb = dwc->ev_buffs[i];
		dev_dbg(dwc->dev, "Event buf %p dma %08llx length %d\n",
				deb->buf, (unsigned long long) deb->dma,
				deb->length);

		deb->lpos = 0;

		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTADRLO(i),
				lower_32_bits(deb->dma));
		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTADRHI(i),
				upper_32_bits(deb->dma));
		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTSIZ(i),
				SPRD_DWC3_GEVNTSIZ_SIZE(deb->length));
		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTCOUNT(i), 0);
	}

	return 0;
}

/*
 * dwc3_event_buffers_cleanup
 */
static void dwc3_event_buffers_cleanup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer *deb;
	int i;

	for (i = 0; i < dwc->num_event_buffers; i++) {
		deb = dwc->ev_buffs[i];

		deb->lpos = 0;

		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTADRLO(i), 0);
		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTADRHI(i), 0);
		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTSIZ(i),
				SPRD_DWC3_GEVNTSIZ_INTMASK | SPRD_DWC3_GEVNTSIZ_SIZE(0));
		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTCOUNT(i), 0);
	}
}

/*
 * dwc3_alloc_scratch_buffers
 */
static int dwc3_alloc_scratch_buffers(struct dwc3 *dwc)
{
	if (!dwc->has_hibernation) {
		return 0;
	}

	if (!dwc->nr_scratch) {
		return 0;
	}

	dwc->scratchbuf = kmalloc_array(dwc->nr_scratch,
			SPRD_DWC3_SCRATCHBUF_SIZE, GFP_KERNEL);

	if (!dwc->scratchbuf) {
		return -ENOMEM;
	}

	return 0;
}

static int dwc3_setup_scratch_buffers(struct dwc3 *dwc)
{
	dma_addr_t scratch_adr;
	u32 param;
	int ret;

	if (!dwc->has_hibernation) {
		return 0;
	}

	if (!dwc->nr_scratch) {
		return 0;
	}

	scratch_adr = dma_map_single(dwc->scratchbuf,
				      dwc->nr_scratch * SPRD_DWC3_SCRATCHBUF_SIZE,
				      DMA_BIDIRECTIONAL);

	if (dma_mapping_error(dwc->dev, scratch_adr)) {
		ret = -EFAULT;
		dev_err(dwc->dev, "failed to map scratch buffer\n");
		goto out0;
	}

	dwc->scratch_addr = scratch_adr;

	/* ADDR_LO */
	param = lower_32_bits(scratch_adr);
	ret = dwc3_send_gadget_generic_command(dwc,
			SPRD_DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO, param);
	if (ret < 0) {
		goto out1;
	}

	/* ADDR_HI */
	param = upper_32_bits(scratch_adr);
	ret = dwc3_send_gadget_generic_command(dwc,
			SPRD_DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI, param);
	if (ret < 0) {
		goto out1;
	}

	return 0;

out1:
	dma_unmap_single((void *)dwc->scratch_addr,
			 dwc->nr_scratch * SPRD_DWC3_SCRATCHBUF_SIZE,
			 DMA_BIDIRECTIONAL);
out0:
	return ret;
}

/*
 * dwc3_free_scratch_buffers
 */
static void dwc3_free_scratch_buffers(struct dwc3 *dwc)
{
	if (!dwc->has_hibernation || !dwc->nr_scratch) {
		return;
	}
	dma_unmap_single((void *)dwc->scratch_addr,
			 dwc->nr_scratch * SPRD_DWC3_SCRATCHBUF_SIZE,
			 DMA_BIDIRECTIONAL);
	kfree(dwc->scratchbuf);
}

/*
 * dwc3_core_num_eps
 */
static void dwc3_core_num_eps(struct dwc3 *dwc)
{
	struct dwc3_hwparams *dhparms = &dwc->hwparams;

	dwc->num_in_eps = SPRD_DWC3_NUM_IN_EPS(dhparms);
	dwc->num_out_eps = SPRD_DWC3_NUM_EPS(dhparms) - dwc->num_in_eps;

	dev_vdbg(dwc->dev, "found %d IN and %d OUT endpoints\n",
			dwc->num_in_eps, dwc->num_out_eps);
}

static void dwc3_cache_hwparams(struct dwc3 *dwc)
{
	struct dwc3_hwparams *dhparms = &dwc->hwparams;

	dhparms->hwparams0 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS0);
	dhparms->hwparams1 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS1);
	dhparms->hwparams2 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS2);
	dhparms->hwparams3 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS3);
	dhparms->hwparams4 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS4);
	dhparms->hwparams5 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS5);
	dhparms->hwparams6 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS6);
	dhparms->hwparams7 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS7);
	dhparms->hwparams8 = dwc3_readl(dwc->regs, SPRD_DWC3_GHWPARAMS8);
}

static void dwc3_hsphy_mode_setup(struct dwc3 *dwc)
{
	enum usb_phy_interface mode = dwc->hsphy_mode;
	u32 reg;

	/* set dwc3 usb2 phy config */
	reg = dwc3_readl(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0));

	switch (mode) {
	case USBPHY_INTERFACE_MODE_UTMI:
		debugf("%s 8 bit phy\n", __func__);
		reg &= ~(SPRD_DWC3_GUSB2PHYCFG_PHYIF_MASK |
			SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM_MASK);
		reg |= SPRD_DWC3_GUSB2PHYCFG_PHYIF(UTMI_PHYIF_8_BIT) |
			SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM(USBTRDTIM_UTMI_8_BIT);
		break;
	case USBPHY_INTERFACE_MODE_UTMIW:
		reg &= ~(SPRD_DWC3_GUSB2PHYCFG_PHYIF_MASK |
			SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM_MASK);
		reg |= SPRD_DWC3_GUSB2PHYCFG_PHYIF(UTMI_PHYIF_16_BIT) |
			SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM(USBTRDTIM_UTMI_16_BIT);
		break;
	default:
		break;
	}

	dwc3_writel(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0), reg);
}

/*
 * Configure USB PHY Interface of SPRD_DWC3 Core
 */
static void dwc3_phy_setup(struct dwc3 *dwc)
{
	u32 reg;

	reg = dwc3_readl(dwc->regs, SPRD_DWC3_GUSB3PIPECTL(0));

	/*
	 * Above 1.94a,
	 * it is recommended to set SPRD_DWC3_GUSB3PIPECTL_SUSPHY
	 * to '0' during coreConsultant configuration. So default value
	 * will be '0' when the core is reset. Application needs to set it
	 * to '1' after the core initialization is completed.
	 */
	if (dwc->revision > SPRD_DWC3_REVISION_194A) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_SUSPHY;
	}
	if (dwc->u2ss_inp3_quirk) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_U2SSINP3OK;
	}
	if (dwc->req_p1p2p3_quirk) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_REQP1P2P3;
	}
	if (dwc->del_p1p2p3_quirk) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_DEP1P2P3_EN;
	}
	if (dwc->del_phy_power_chg_quirk) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_DEPOCHANGE;
	}
	if (dwc->lfps_filter_quirk) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_LFPSFILT;
	}
	if (dwc->rx_detect_poll_quirk) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_RX_DETOPOLL;
	}
	if (dwc->tx_de_emphasis_quirk) {
		reg |= SPRD_DWC3_GUSB3PIPECTL_TX_DEEPH(dwc->tx_de_emphasis);
	}
	if (dwc->dis_u3_susphy_quirk) {
		reg &= ~SPRD_DWC3_GUSB3PIPECTL_SUSPHY;
	}

	dwc3_writel(dwc->regs, SPRD_DWC3_GUSB3PIPECTL(0), reg);

	mdelay(100);

	reg = dwc3_readl(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0));

	reg &= ~SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM_MASK;
#ifdef CONFIG_USB_SPRD_DWC3_INTEL
	reg &= ~SPRD_DWC3_GUSB2PHYCFG_PHYIF_MASK;
	reg |= SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM(9);
#else
	reg |= SPRD_DWC3_GUSB2PHYCFG_PHYIF_MASK;
	reg |= SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM(5);
#endif

	/*
	 * Above 1.94a,
	 * it is recommended to set SPRD_DWC3_GUSB2PHYCFG_SUSPHY to
	 * '0' during coreConsultant configuration. So default value will
	 * be '0' when the core is reset. Application needs to set it to
	 * '1' after the core initialization is completed.
	 */
	if (dwc->revision > SPRD_DWC3_REVISION_194A) {
		reg |= SPRD_DWC3_GUSB2PHYCFG_SUSPHY;
	}
	if (dwc->dis_u2_susphy_quirk) {
		reg &= ~SPRD_DWC3_GUSB2PHYCFG_SUSPHY;
	}

	dwc3_writel(dwc->regs, SPRD_DWC3_GUSB2PHYCFG(0), reg);

	dwc3_hsphy_mode_setup(dwc);

	mdelay(100);
}

/*
 * Low-level initialization of SPRD_DWC3 Core
 */
static int dwc3_core_init(struct dwc3 *dwc)
{
	unsigned long timeout;
	u32 hwparams4 = dwc->hwparams.hwparams4;
	u32 reg;
	int ret;

	reg = dwc3_readl(dwc->regs, SPRD_DWC3_GSNPSID);
	/* this should read as U3 followed by revision number */
	if ((reg & SPRD_DWC3_GSNPSID_MASK) == 0x55330000) {
		/* detected DWC_usb3 IP */
		dwc->revision = reg;
	} else if ((reg & SPRD_DWC3_GSNPSID_MASK) == 0x33310000) {
		/* detected DWC_usb31 IP */
		dwc->revision = dwc3_readl(dwc->regs, SPRD_DWC3_VER_NUMBER);
		dwc->revision |= SPRD_DWC3_REVISION_IS_SPRD_DWC31;
		debugf("this is USB31 DRD Core\n");
	} else {
		debugf("this is not a DesignWare USB3 or USB31 DRD Core\n");
		ret = -ENODEV;
		goto err0;
	}

	/* handle USB2.0-only core configuration */
	if (SPRD_DWC3_GHWPARAMS3_SSPHY_IFC(dwc->hwparams.hwparams3) ==
			SPRD_DWC3_GHWPARAMS3_SSPHY_IFC_DIS) {
		if (dwc->maximum_speed == USB_SPEED_SUPER)
			dwc->maximum_speed = USB_SPEED_HIGH;
	}

	/* issue device SoftReset too */
	timeout = 5000;
	dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, SPRD_DWC3_DCTL_CSFTRST);
	while (timeout--) {
		reg = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
		if (!(reg & SPRD_DWC3_DCTL_CSFTRST)) {
			break;
		}
	};

	if (!timeout) {
		dev_err(dwc->dev, "Reset TimeOut\n");
		ret = -ETIMEDOUT;
		goto err0;
	}

	ret = dwc3_core_soft_reset(dwc);
	if (ret) {
		goto err0;
	}

	reg = dwc3_readl(dwc->regs, SPRD_DWC3_GCTL);
	reg &= ~SPRD_DWC3_GCTL_SCALEDOWN_MASK;
	reg &= ~SPRD_DWC3_GCTL_PWRDNSCALE_MASK;
	reg |= SPRD_DWC3_GCTL_PWRDNSCALE(2);

	switch (SPRD_DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1)) {
	case SPRD_DWC3_GHWPARAMS1_EN_PWROPT_HIB:
		dwc->nr_scratch = SPRD_DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(hwparams4);
		reg |= SPRD_DWC3_GCTL_GBLHIBERNATIONEN;
		break;

	case SPRD_DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		reg &= ~SPRD_DWC3_GCTL_DSBLCLKGTNG;
		break;

	default:
		dev_dbg(dwc->dev, "No power optimization available\n");
	}

	/* if simulation board */
	if (dwc->hwparams.hwparams6 & SPRD_DWC3_GHWPARAMS6_EN_FPGA) {
		dev_dbg(dwc->dev, "it is on FPGA board\n");
		dwc->is_fpga = true;
	}

	if(dwc->disable_scramble_quirk && !dwc->is_fpga) {
		WARN(true, "disable_scramble cannot be used on non-FPGA\n");
	}

	if (dwc->disable_scramble_quirk && dwc->is_fpga) {
		reg |= SPRD_DWC3_GCTL_DISSCRAMBLE;
	} else {
		reg &= ~SPRD_DWC3_GCTL_DISSCRAMBLE;
	}

	if (dwc->u2exit_lfps_quirk) {
		reg |= SPRD_DWC3_GCTL_U2EXIT_LFPS;
	}

	/*
	 * WORKAROUND: DWC3 revisions <1.90a
	 */
	if (dwc->revision < SPRD_DWC3_REVISION_190A) {
		reg |= SPRD_DWC3_GCTL_U2RSTECN;
	}

	dwc3_core_num_eps(dwc);
	dwc3_writel(dwc->regs, SPRD_DWC3_GCTL, reg);
	dwc3_phy_setup(dwc);

#ifdef CONFIG_USB_SPRD_DWC3_INTEL
	reg = dwc3_readl(dwc->regs, SPRD_DWC3_GUCTL1);
	reg |= SPRD_DWC3_GUCTL1_DEV_FOR_30_CLK;
	dwc3_writel(dwc->regs, SPRD_DWC3_GUCTL1, reg);
#endif

	ret = dwc3_alloc_scratch_buffers(dwc);
	if (ret) {
		goto err0;
	}
	ret = dwc3_setup_scratch_buffers(dwc);
	if (ret) {
		goto err1;
	}
	return 0;

err1:
	dwc3_free_scratch_buffers(dwc);

err0:
	return ret;
}

static void dwc3_core_exit(struct dwc3 *dwc)
{
	dwc3_free_scratch_buffers(dwc);
}

static void dwc3_set_mode(struct dwc3 *dwc, u32 mode)
{
	u32 val;

	val = dwc3_readl(dwc->regs, SPRD_DWC3_GCTL);
	val &= ~(SPRD_DWC3_GCTL_PRTCAPDIR(SPRD_DWC3_GCTL_PRTCAP_OTG));
	val |= SPRD_DWC3_GCTL_PRTCAPDIR(mode);
	dwc3_writel(dwc->regs, SPRD_DWC3_GCTL, val);
}

static int dwc3_core_init_mode(struct dwc3 *dwc)
{
	int ret;

	dwc3_set_mode(dwc, SPRD_DWC3_GCTL_PRTCAP_DEVICE);
	ret = dwc3_gadget_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize gadget\n");
		return ret;
	}
	return 0;
}

static void dwc3_core_exit_mode(struct dwc3 *dwc)
{
	dwc3_gadget_exit(dwc);
}


static struct dwc3 *dwc3;
uint32_t dwc3_get_evt_lpos(void)
{
	return dwc3->ev_buffs[0]->lpos;
}


#define SPRD_DWC3_ALIGN_MASK		(16 - 1)

/*
 * dwc3 core initialization code
 */
int dwc3_uboot_init(struct dwc3_device *d3dev)
{
	struct device *dev;
	struct dwc3 *dwc;
	u8 hird_threshold;
	u8 tx_de_emphasis;
	u8 lpm_nyet_threshold;
	void *mem;
	int  ret;

	mem = kzalloc(sizeof(*dwc) + SPRD_DWC3_ALIGN_MASK, GFP_KERNEL);
	if (!mem) {
		return -ENOMEM;
	}

	dwc = PTR_ALIGN(mem, SPRD_DWC3_ALIGN_MASK + 1);
	dwc->mem = mem;
	dwc3 = dwc;
	d3dev->dwc = dwc;

	dwc->regs = (void *)(d3dev->base + SPRD_DWC3_GLOBALS_REGS_START);

	/*
	 * default to assert utmi_sleep_n
	 * maximum allowed HIRD threshold value of 0b1100
	 */
	hird_threshold = 12;

	/* default: -3.5dB de-emphasis */
	tx_de_emphasis = 1;

	/* default: highest possible threshold */
	lpm_nyet_threshold = 0xff;

	dwc->has_lpm_erratum = d3dev->has_lpm_erratum;
	dwc->maximum_speed = d3dev->maximum_speed;

	if (d3dev->lpm_nyet_threshold) {
		lpm_nyet_threshold = d3dev->lpm_nyet_threshold;
	}

	dwc->is_utmi_l1_suspend = d3dev->is_utmi_l1_suspend;

	if (d3dev->hird_threshold) {
		hird_threshold = d3dev->hird_threshold;
	}

	dwc->dr_mode = d3dev->dr_mode;
	dwc->needs_fifo_resize = d3dev->tx_fifo_resize;
	dwc->u2ss_inp3_quirk = d3dev->u2ss_inp3_quirk;
	dwc->u2exit_lfps_quirk = d3dev->u2exit_lfps_quirk;
	dwc->disable_scramble_quirk = d3dev->disable_scramble_quirk;

	dwc->del_phy_power_chg_quirk = d3dev->del_phy_power_chg_quirk;
	dwc->del_p1p2p3_quirk = d3dev->del_p1p2p3_quirk;
	dwc->req_p1p2p3_quirk = d3dev->req_p1p2p3_quirk;

	dwc->tx_de_emphasis_quirk = d3dev->tx_de_emphasis_quirk;
	dwc->rx_detect_poll_quirk = d3dev->rx_detect_poll_quirk;
	dwc->lfps_filter_quirk = d3dev->lfps_filter_quirk;
	dwc->dis_u2_susphy_quirk = d3dev->dis_u2_susphy_quirk;
	dwc->dis_u3_susphy_quirk = d3dev->dis_u3_susphy_quirk;

	if (d3dev->tx_de_emphasis) {
		tx_de_emphasis = d3dev->tx_de_emphasis;
	}

	dwc->hird_threshold = hird_threshold | (dwc->is_utmi_l1_suspend << 4);
	dwc->tx_de_emphasis = tx_de_emphasis;
	dwc->lpm_nyet_threshold = lpm_nyet_threshold;
	dwc->hsphy_mode = d3dev->hsphy_mode;

	if (USB_SPEED_UNKNOWN == dwc->maximum_speed) {
		dwc->maximum_speed = USB_SPEED_SUPER;
	}

	dwc3_cache_hwparams(dwc);

	ret = dwc3_alloc_event_buffers(dwc, SPRD_DWC3_EVENT_BUFFERS_SIZE);
	if (ret) {
		dev_err(dwc->dev, "failed to alloc event-buffers\n");
		return -ENOMEM;
	}

	dwc->dr_mode = SPRD_DWC3_GCTL_PRTCAP_DEVICE;

	/* core init */
	ret = dwc3_core_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initial core\n");
		goto out0;
	}

	/* setup event buffers */
	ret = dwc3_event_buffers_setup(dwc);
	if (ret) {
		dev_err(dwc->dev, "failed to setup event-buffers\n");
		goto out1;
	}

	/* init mode */
	ret = dwc3_core_init_mode(dwc);
	if (ret) {
		goto out2;
	}
	return 0;

out2:
	dwc3_event_buffers_cleanup(dwc);

out1:
	dwc3_core_exit(dwc);

out0:
	dwc3_free_event_buffers(dwc);

	return ret;
}

/*
 * dwc3 core cleanup
 */
void dwc3_uboot_exit(struct dwc3_device *d3dev)
{
	struct dwc3 *dwc = d3dev->dwc;

	dwc3_core_exit_mode(dwc);
	dwc3_event_buffers_cleanup(dwc);
	dwc3_free_event_buffers(dwc);
	dwc3_core_exit(dwc);
	free(dwc->mem);
}

/*
 * handle dwc3 core interrupt
 */
void dwc3_uboot_handle_interrupt(struct dwc3_device *d3dev)
{
	struct dwc3 *dwc = d3dev->dwc;

	dwc3_gadget_uboot_handle_interrupt(dwc);
}

