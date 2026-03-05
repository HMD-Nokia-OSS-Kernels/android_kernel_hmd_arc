/**
 * core.h - DesignWare USB3 DRD Core Header
 */

#ifndef __DRIVERS_USB_SPRD_DWC3_CORE_H__
#define __DRIVERS_USB_SPRD_DWC3_CORE_H__

#include <linux/usb/ch9.h>
#include <sprd_types.h>

#define dev_info(x,...)
#define ERR_PTR(x) ((void *)x)
#define PTR_ERR(x) ((long)x)
#define IS_ERR(x)  (unlikely(((unsigned long) (void *) x) >= (unsigned long)-MAX_ERRNO))
#define PTR_ALIGN(p, a)        ((typeof(p))ALIGN((unsigned long)(p), (a)))

enum usb_phy_interface {
        USBPHY_INTERFACE_MODE_UNKNOWN,
        USBPHY_INTERFACE_MODE_UTMI,
        USBPHY_INTERFACE_MODE_UTMIW,
};

struct resource {
        resource_size_t start;
        resource_size_t end;
        const char *name;
        unsigned long flags;
        struct resource *parent, *sibling, *child;
};

#define SPRD_DWC3_MSG_MAX 500

/* Global constants */
#define SPRD_DWC3_EP0_BOUNCE_SIZE       512
#define SPRD_DWC3_ENDPOINTS_NUM         32
#define SPRD_DWC3_XHCI_RESOURCES_NUM    2

#define SPRD_DWC3_SCRATCHBUF_SIZE       4096     /* each buffer is assumed to be 4KiB */
#define SPRD_DWC3_EVENT_SIZE            4        /* bytes */
#define SPRD_DWC3_EVENT_MAX_NUM         64       /* 2 events/endpoint */
#define SPRD_DWC3_EVENT_BUFFERS_SIZE    (SPRD_DWC3_EVENT_SIZE * SPRD_DWC3_EVENT_MAX_NUM)
#define SPRD_DWC3_EVENT_TYPE_MASK       0xfe

#define SPRD_DWC3_EVENT_TYPE_DEV        0
#define SPRD_DWC3_EVENT_TYPE_CARKIT     3
#define SPRD_DWC3_EVENT_TYPE_I2C        4

#define SPRD_DWC3_DEVICE_EVENT_DISCONNECT          0
#define SPRD_DWC3_DEVICE_EVENT_RESET               1
#define SPRD_DWC3_DEVICE_EVENT_CONNECT_DONE        2
#define SPRD_DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE  3
#define SPRD_DWC3_DEVICE_EVENT_WAKEUP              4
#define SPRD_DWC3_DEVICE_EVENT_HIBER_REQ           5
#define SPRD_DWC3_DEVICE_EVENT_EOPF                6
#define SPRD_DWC3_DEVICE_EVENT_SOF                 7
#define SPRD_DWC3_DEVICE_EVENT_ERRATIC_ERROR       9
#define SPRD_DWC3_DEVICE_EVENT_CMD_CMPL            10
#define SPRD_DWC3_DEVICE_EVENT_OVERFLOW            11

#define SPRD_DWC3_GEVNTCOUNT_MASK       0xfffc
#define SPRD_DWC3_GSNPSID_MASK          0xffff0000
#define SPRD_DWC3_GSNPSREV_MASK         0xffff

/* SPRD_DWC3 registers memory space boundries */
#define SPRD_DWC3_XHCI_REGS_START       0x0
#define SPRD_DWC3_XHCI_REGS_END         0x7fff
#define SPRD_DWC3_GLOBALS_REGS_START    0xc100
#define SPRD_DWC3_GLOBALS_REGS_END      0xc6ff
#define SPRD_DWC3_DEVICE_REGS_START     0xc700
#define SPRD_DWC3_DEVICE_REGS_END       0xcbff
#define SPRD_DWC3_OTG_REGS_START        0xcc00
#define SPRD_DWC3_OTG_REGS_END          0xccff

/* Global Registers */
#define SPRD_DWC3_GSBUSCFG0             0xc100
#define SPRD_DWC3_GSBUSCFG1             0xc104
#define SPRD_DWC3_GTXTHRCFG             0xc108
#define SPRD_DWC3_GRXTHRCFG             0xc10c
#define SPRD_DWC3_GCTL                  0xc110
#define SPRD_DWC3_GEVTEN                0xc114
#define SPRD_DWC3_GSTS                  0xc118
#define SPRD_DWC3_GUCTL1                0xc11c
#define SPRD_DWC3_GSNPSID               0xc120
#define SPRD_DWC3_GGPIO                 0xc124
#define SPRD_DWC3_GUID                  0xc128
#define SPRD_DWC3_GUCTL                 0xc12c
#define SPRD_DWC3_GBUSERRADDR0          0xc130
#define SPRD_DWC3_GBUSERRADDR1          0xc134
#define SPRD_DWC3_GPRTBIMAP0            0xc138
#define SPRD_DWC3_GPRTBIMAP1            0xc13c
#define SPRD_DWC3_GHWPARAMS0            0xc140
#define SPRD_DWC3_GHWPARAMS1            0xc144
#define SPRD_DWC3_GHWPARAMS2            0xc148
#define SPRD_DWC3_GHWPARAMS3            0xc14c
#define SPRD_DWC3_GHWPARAMS4            0xc150
#define SPRD_DWC3_GHWPARAMS5            0xc154
#define SPRD_DWC3_GHWPARAMS6            0xc158
#define SPRD_DWC3_GHWPARAMS7            0xc15c
#define SPRD_DWC3_GDBGFIFOSPACE         0xc160
#define SPRD_DWC3_GDBGLTSSM             0xc164
#define SPRD_DWC3_GPRTBIMAP_HS0         0xc180
#define SPRD_DWC3_GPRTBIMAP_HS1         0xc184
#define SPRD_DWC3_GPRTBIMAP_FS0         0xc188
#define SPRD_DWC3_GPRTBIMAP_FS1         0xc18c
#define SPRD_DWC3_GUCTL2                0xc19c

#define SPRD_DWC3_VER_NUMBER            0xc1a0
#define SPRD_DWC3_VER_TYPE              0xc1a4

#define SPRD_DWC3_GUSB2PHYCFG(n)        (0xc200 + (n * 0x04))
#define SPRD_DWC3_GUSB2I2CCTL(n)        (0xc240 + (n * 0x04))

#define SPRD_DWC3_GUSB2PHYACC(n)        (0xc280 + (n * 0x04))

#define SPRD_DWC3_GUSB3PIPECTL(n)       (0xc2c0 + (n * 0x04))

#define SPRD_DWC3_GTXFIFOSIZ(n)         (0xc300 + (n * 0x04))
#define SPRD_DWC3_GRXFIFOSIZ(n)         (0xc380 + (n * 0x04))

#define SPRD_DWC3_GEVNTADRLO(n)         (0xc400 + (n * 0x10))
#define SPRD_DWC3_GEVNTADRHI(n)         (0xc404 + (n * 0x10))
#define SPRD_DWC3_GEVNTSIZ(n)           (0xc408 + (n * 0x10))
#define SPRD_DWC3_GEVNTCOUNT(n)         (0xc40c + (n * 0x10))

#define SPRD_DWC3_GHWPARAMS8            0xc600

/* Device Registers */
#define SPRD_DWC3_DCFG            0xc700
#define SPRD_DWC3_DCTL            0xc704
#define SPRD_DWC3_DEVTEN          0xc708
#define SPRD_DWC3_DSTS            0xc70c
#define SPRD_DWC3_DGCMDPAR        0xc710
#define SPRD_DWC3_DGCMD           0xc714
#define SPRD_DWC3_DALEPENA        0xc720
#define SPRD_DWC3_DEPCMDPAR2(n)   (0xc800 + (n * 0x10))
#define SPRD_DWC3_DEPCMDPAR1(n)   (0xc804 + (n * 0x10))
#define SPRD_DWC3_DEPCMDPAR0(n)   (0xc808 + (n * 0x10))
#define SPRD_DWC3_DEPCMD(n)       (0xc80c + (n * 0x10))

/* OTG Registers */
#define SPRD_DWC3_OCFG     0xcc00
#define SPRD_DWC3_OCTL     0xcc04
#define SPRD_DWC3_OEVT     0xcc08
#define SPRD_DWC3_OEVTEN   0xcc0C
#define SPRD_DWC3_OSTS     0xcc10

/* Bit fields */

/* Global Configuration Register */
#define SPRD_DWC3_GCTL_PWRDNSCALE(n)       ((n) << 19)
#define SPRD_DWC3_GCTL_PWRDNSCALE_MASK     (0x1fff << 19)
#define SPRD_DWC3_GCTL_U2RSTECN            (1 << 16)
#define SPRD_DWC3_GCTL_RAMCLKSEL(x)        (((x) & SPRD_DWC3_GCTL_CLK_MASK) << 6)
#define SPRD_DWC3_GCTL_CLK_BUS         (0)
#define SPRD_DWC3_GCTL_CLK_PIPE        (1)
#define SPRD_DWC3_GCTL_CLK_PIPEHALF    (2)
#define SPRD_DWC3_GCTL_CLK_MASK        (3)

#define SPRD_DWC3_GCTL_PRTCAP(n)       (((n) & (3 << 12)) >> 12)
#define SPRD_DWC3_GCTL_PRTCAPDIR(n)    ((n) << 12)
#define SPRD_DWC3_GCTL_PRTCAP_HOST     1
#define SPRD_DWC3_GCTL_PRTCAP_DEVICE   2
#define SPRD_DWC3_GCTL_PRTCAP_OTG      3

#define SPRD_DWC3_GCTL_CORESOFTRESET       (1 << 11)
#define SPRD_DWC3_GCTL_SOFITPSYNC          (1 << 10)
#define SPRD_DWC3_GCTL_SCALEDOWN(n)        ((n) << 4)
#define SPRD_DWC3_GCTL_SCALEDOWN_MASK      SPRD_DWC3_GCTL_SCALEDOWN(3)
#define SPRD_DWC3_GCTL_DISSCRAMBLE         (1 << 3)
#define SPRD_DWC3_GCTL_U2EXIT_LFPS         (1 << 2)
#define SPRD_DWC3_GCTL_GBLHIBERNATIONEN    (1 << 1)
#define SPRD_DWC3_GCTL_DSBLCLKGTNG         (1 << 0)

/* GUCTL1 Register */
#define SPRD_DWC3_GUCTL1_DEV_FOR_30_CLK    (1 << 26)

/* USB2 PHY Configuration Register */
#define SPRD_DWC3_GUSB2PHYCFG_PHYSOFTRST      (1 << 31)
#define SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM(n)    ((n) << 10)
#define SPRD_DWC3_GUSB2PHYCFG_PHYIF(n)        ((n) << 3)
#define SPRD_DWC3_GUSB2PHYCFG_PHYIF_MASK      (1 << 3)
#define SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM_MASK  SPRD_DWC3_GUSB2PHYCFG_USBTRDTIM(0xf)
#define SPRD_DWC3_GUSB2PHYCFG_ENBLSLPM        (1 << 8)
#define SPRD_DWC3_GUSB2PHYCFG_SUSPHY          (1 << 6)
//#define SPRD_DWC3_GUSB2PHYCFG_PHYIF         (1 << 3)
#define USBTRDTIM_UTMI_8_BIT        9
#define USBTRDTIM_UTMI_16_BIT       5
#define UTMI_PHYIF_16_BIT           1
#define UTMI_PHYIF_8_BIT            0

/* USB3 PIPE Control Register */
#define SPRD_DWC3_GUSB3PIPECTL_PHYSOFTRST         (1 << 31)
#define SPRD_DWC3_GUSB3PIPECTL_U2SSINP3OK         (1 << 29)
#define SPRD_DWC3_GUSB3PIPECTL_REQP1P2P3          (1 << 24)
#define SPRD_DWC3_GUSB3PIPECTL_DEP1P2P3(n)        ((n) << 19)
#define SPRD_DWC3_GUSB3PIPECTL_DEP1P2P3_MASK      SPRD_DWC3_GUSB3PIPECTL_DEP1P2P3(7)
#define SPRD_DWC3_GUSB3PIPECTL_DEP1P2P3_EN        SPRD_DWC3_GUSB3PIPECTL_DEP1P2P3(1)
#define SPRD_DWC3_GUSB3PIPECTL_DEPOCHANGE         (1 << 18)
#define SPRD_DWC3_GUSB3PIPECTL_SUSPHY             (1 << 17)
#define SPRD_DWC3_GUSB3PIPECTL_LFPSFILT           (1 << 9)
#define SPRD_DWC3_GUSB3PIPECTL_RX_DETOPOLL        (1 << 8)
#define SPRD_DWC3_GUSB3PIPECTL_TX_DEEPH_MASK      SPRD_DWC3_GUSB3PIPECTL_TX_DEEPH(3)
#define SPRD_DWC3_GUSB3PIPECTL_TX_DEEPH(n)        ((n) << 1)

/* TX Fifo Size Register */
#define SPRD_DWC3_GTXFIFOSIZ_TXFDEF(n)            ((n) & 0xffff)
#define SPRD_DWC3_GTXFIFOSIZ_TXFSTADDR(n)         ((n) & 0xffff0000)

/* Event Size Registers */
#define SPRD_DWC3_GEVNTSIZ_INTMASK                (1 << 31)
#define SPRD_DWC3_GEVNTSIZ_SIZE(n)                ((n) & 0xffff)

/* HWPARAMS1 Register */
#define SPRD_DWC3_GHWPARAMS1_EN_PWROPT(n)         (((n) & (3 << 24)) >> 24)
#define SPRD_DWC3_GHWPARAMS1_EN_PWROPT_NO         0
#define SPRD_DWC3_GHWPARAMS1_EN_PWROPT_CLK        1
#define SPRD_DWC3_GHWPARAMS1_EN_PWROPT_HIB        2
#define SPRD_DWC3_GHWPARAMS1_PWROPT(n)            ((n) << 24)
#define SPRD_DWC3_GHWPARAMS1_PWROPT_MASK           SPRD_DWC3_GHWPARAMS1_PWROPT(3)

/* HWPARAMS3 Register */
#define SPRD_DWC3_GHWPARAMS3_SSPHY_IFC(n)         ((n) & 3)
#define SPRD_DWC3_GHWPARAMS3_SSPHY_IFC_DIS        0
#define SPRD_DWC3_GHWPARAMS3_SSPHY_IFC_ENA        1
#define SPRD_DWC3_GHWPARAMS3_HSPHY_IFC(n)         (((n) & (3 << 2)) >> 2)
#define SPRD_DWC3_GHWPARAMS3_HSPHY_IFC_DIS        0
#define SPRD_DWC3_GHWPARAMS3_HSPHY_IFC_UTMI       1
#define SPRD_DWC3_GHWPARAMS3_HSPHY_IFC_ULPI       2
#define SPRD_DWC3_GHWPARAMS3_HSPHY_IFC_UTMI_ULPI  3
#define SPRD_DWC3_GHWPARAMS3_FSPHY_IFC(n)         (((n) & (3 << 4)) >> 4)
#define SPRD_DWC3_GHWPARAMS3_FSPHY_IFC_DIS        0
#define SPRD_DWC3_GHWPARAMS3_FSPHY_IFC_ENA        1

/* HWPARAMS4 Register */
#define SPRD_DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(n) (((n) & (0x0f << 13)) >> 13)
#define SPRD_DWC3_MAX_HIBER_SCRATCHBUFS           15

/* HWPARAMS6 Register */
#define SPRD_DWC3_GHWPARAMS6_EN_FPGA     (1 << 7)

/* Configuration Register */
#define SPRD_DWC3_DCFG_DEVADDR(addr)     ((addr) << 3)
#define SPRD_DWC3_DCFG_DEVADDR_MASK      SPRD_DWC3_DCFG_DEVADDR(0x7f)

#define SPRD_DWC3_DCFG_SPEED_MASK        (7 << 0)
#define SPRD_DWC3_DCFG_SUPERSPEED        (4 << 0)
#define SPRD_DWC3_DCFG_HIGHSPEED         (0 << 0)
#define SPRD_DWC3_DCFG_FULLSPEED2        (1 << 0)
#define SPRD_DWC3_DCFG_LOWSPEED          (2 << 0)
#define SPRD_DWC3_DCFG_FULLSPEED1        (3 << 0)

#define SPRD_DWC3_DCFG_LPM_CAP           (1 << 22)

/* Device Control Register */
#define SPRD_DWC3_DCTL_RUN_STOP          (1 << 31)
#define SPRD_DWC3_DCTL_CSFTRST           (1 << 30)
#define SPRD_DWC3_DCTL_LSFTRST           (1 << 29)
#define SPRD_DWC3_DCTL_HIRD_THRES_MASK   (0x1f << 24)
#define SPRD_DWC3_DCTL_HIRD_THRES(n)     ((n) << 24)
#define SPRD_DWC3_DCTL_APPL1RES          (1 << 23)

/* for core versions 1.87a and earlier */
#define SPRD_DWC3_DCTL_TRGTULST_MASK     (0x0f << 17)
#define SPRD_DWC3_DCTL_TRGTULST(n)       ((n) << 17)
#define SPRD_DWC3_DCTL_TRGTULST_U2       (SPRD_DWC3_DCTL_TRGTULST(2))
#define SPRD_DWC3_DCTL_TRGTULST_U3       (SPRD_DWC3_DCTL_TRGTULST(3))
#define SPRD_DWC3_DCTL_TRGTULST_SS_DIS   (SPRD_DWC3_DCTL_TRGTULST(4))
#define SPRD_DWC3_DCTL_TRGTULST_RX_DET   (SPRD_DWC3_DCTL_TRGTULST(5))
#define SPRD_DWC3_DCTL_TRGTULST_SS_INACT (SPRD_DWC3_DCTL_TRGTULST(6))

/* for core versions 1.94a and later */
#define SPRD_DWC3_DCTL_LPM_ERRATA_MASK   SPRD_DWC3_DCTL_LPM_ERRATA(0xf)
#define SPRD_DWC3_DCTL_LPM_ERRATA(n)     ((n) << 20)

#define SPRD_DWC3_DCTL_KEEP_CONNECT      (1 << 19)
#define SPRD_DWC3_DCTL_L1_HIBER_EN       (1 << 18)
#define SPRD_DWC3_DCTL_CRS               (1 << 17)
#define SPRD_DWC3_DCTL_CSS               (1 << 16)

#define SPRD_DWC3_DCTL_INITU2ENA         (1 << 12)
#define SPRD_DWC3_DCTL_ACCEPTU2ENA       (1 << 11)
#define SPRD_DWC3_DCTL_INITU1ENA         (1 << 10)
#define SPRD_DWC3_DCTL_ACCEPTU1ENA       (1 << 9)
#define SPRD_DWC3_DCTL_TSTCTRL_MASK      (0xf << 1)

#define SPRD_DWC3_DCTL_ULSTCHNGREQ_MASK          (0x0f << 5)
#define SPRD_DWC3_DCTL_ULSTCHNGREQ(n)            (((n) << 5) & SPRD_DWC3_DCTL_ULSTCHNGREQ_MASK)

#define SPRD_DWC3_DCTL_ULSTCHNG_NO_ACTION        (SPRD_DWC3_DCTL_ULSTCHNGREQ(0))
#define SPRD_DWC3_DCTL_ULSTCHNG_SS_DISABLED      (SPRD_DWC3_DCTL_ULSTCHNGREQ(4))
#define SPRD_DWC3_DCTL_ULSTCHNG_RX_DETECT        (SPRD_DWC3_DCTL_ULSTCHNGREQ(5))
#define SPRD_DWC3_DCTL_ULSTCHNG_SS_INACTIVE      (SPRD_DWC3_DCTL_ULSTCHNGREQ(6))
#define SPRD_DWC3_DCTL_ULSTCHNG_RECOVERY         (SPRD_DWC3_DCTL_ULSTCHNGREQ(8))
#define SPRD_DWC3_DCTL_ULSTCHNG_COMPLIANCE       (SPRD_DWC3_DCTL_ULSTCHNGREQ(10))
#define SPRD_DWC3_DCTL_ULSTCHNG_LOOPBACK         (SPRD_DWC3_DCTL_ULSTCHNGREQ(11))

/* Device-Event Enable Register */
#define SPRD_DWC3_DEVTEN_VNDRDEVTSTRCVEDEN       (1 << 12)
#define SPRD_DWC3_DEVTEN_EVNTOVERFLOWEN          (1 << 11)
#define SPRD_DWC3_DEVTEN_CMDCMPLTEN              (1 << 10)
#define SPRD_DWC3_DEVTEN_ERRTICERREN             (1 << 9)
#define SPRD_DWC3_DEVTEN_SOFEN                   (1 << 7)
#define SPRD_DWC3_DEVTEN_EOPFEN                  (1 << 6)
#define SPRD_DWC3_DEVTEN_HIBERNATIONREQEVTEN     (1 << 5)
#define SPRD_DWC3_DEVTEN_WKUPEVTEN               (1 << 4)
#define SPRD_DWC3_DEVTEN_ULSTCNGEN               (1 << 3)
#define SPRD_DWC3_DEVTEN_CONNECTDONEEN           (1 << 2)
#define SPRD_DWC3_DEVTEN_USBRSTEN                (1 << 1)
#define SPRD_DWC3_DEVTEN_DISCONNEVTEN            (1 << 0)

/* Device-Status Register */
#define SPRD_DWC3_DSTS_DCNRD              (1 << 29)

/* for core versions 1.87a and earlier */
#define SPRD_DWC3_DSTS_PWRUPREQ           (1 << 24)

/* for core versions 1.94a and later */
#define SPRD_DWC3_DSTS_RSS                (1 << 25)
#define SPRD_DWC3_DSTS_SSS                (1 << 24)

#define SPRD_DWC3_DSTS_COREIDLE           (1 << 23)
#define SPRD_DWC3_DSTS_DEVCTRLHLT         (1 << 22)

#define SPRD_DWC3_DSTS_USBLNKST_MASK      (0x0f << 18)
#define SPRD_DWC3_DSTS_USBLNKST(n)        (((n) & SPRD_DWC3_DSTS_USBLNKST_MASK) >> 18)

#define SPRD_DWC3_DSTS_RXFIFOEMPTY        (1 << 17)

#define SPRD_DWC3_DSTS_SOFFN_MASK         (0x3fff << 3)
#define SPRD_DWC3_DSTS_SOFFN(n)           (((n) & SPRD_DWC3_DSTS_SOFFN_MASK) >> 3)

#define SPRD_DWC3_DSTS_CONNECTSPD         (7 << 0)

#define SPRD_DWC3_DSTS_SUPERSPEED         (4 << 0)
#define SPRD_DWC3_DSTS_HIGHSPEED          (0 << 0)
#define SPRD_DWC3_DSTS_FULLSPEED2         (1 << 0)
#define SPRD_DWC3_DSTS_LOWSPEED           (2 << 0)
#define SPRD_DWC3_DSTS_FULLSPEED1         (3 << 0)

/* Device-Generic Command Register */
#define SPRD_DWC3_DGCMD_SET_LMP                 0x01
#define SPRD_DWC3_DGCMD_SET_PERIODIC_PAR        0x02
#define SPRD_DWC3_DGCMD_XMIT_FUNCTION           0x03

/* for core versions 1.94a and later */
#define SPRD_DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO  0x04
#define SPRD_DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI  0x05

#define SPRD_DWC3_DGCMD_SELECTED_FIFO_FLUSH     0x09
#define SPRD_DWC3_DGCMD_ALL_FIFO_FLUSH          0x0a
#define SPRD_DWC3_DGCMD_SET_ENDPOINT_NRDY       0x0c
#define SPRD_DWC3_DGCMD_RUN_SOC_BUS_LOOPBACK    0x10

#define SPRD_DWC3_DGCMD_STATUS(n)             (((n) >> 15) & 1)
#define SPRD_DWC3_DGCMD_CMDACT                (1 << 10)
#define SPRD_DWC3_DGCMD_CMDIOC                (1 << 8)

/* Device-Generic Command Parameter Register */
#define SPRD_DWC3_DGCMDPAR_FORCE_LINKPM_ACCEPT  (1 << 0)
#define SPRD_DWC3_DGCMDPAR_FIFO_NUM(n)          ((n) << 0)
#define SPRD_DWC3_DGCMDPAR_RX_FIFO              (0 << 5)
#define SPRD_DWC3_DGCMDPAR_TX_FIFO              (1 << 5)
#define SPRD_DWC3_DGCMDPAR_LOOPBACK_DIS         (0 << 0)
#define SPRD_DWC3_DGCMDPAR_LOOPBACK_ENA         (1 << 0)

/* Device-Endpoint Command Register */
#define SPRD_DWC3_DEPCMD_PARAM_SHIFT        16
#define SPRD_DWC3_DEPCMD_PARAM(x)           ((x) << SPRD_DWC3_DEPCMD_PARAM_SHIFT)
#define SPRD_DWC3_DEPCMD_GET_RSC_IDX(x)     (((x) >> SPRD_DWC3_DEPCMD_PARAM_SHIFT) & 0x7f)
#define SPRD_DWC3_DEPCMD_STATUS(x)          (((x) >> 15) & 1)
#define SPRD_DWC3_DEPCMD_HIPRI_FORCERM      (1 << 11)
#define SPRD_DWC3_DEPCMD_CMDACT             (1 << 10)
#define SPRD_DWC3_DEPCMD_CMDIOC             (1 << 8)

#define SPRD_DWC3_DEPCMD_DEPSTARTCFG        (0x09 << 0)
#define SPRD_DWC3_DEPCMD_ENDTRANSFER        (0x08 << 0)
#define SPRD_DWC3_DEPCMD_UPDATETRANSFER     (0x07 << 0)
#define SPRD_DWC3_DEPCMD_STARTTRANSFER      (0x06 << 0)
#define SPRD_DWC3_DEPCMD_CLEARSTALL         (0x05 << 0)
#define SPRD_DWC3_DEPCMD_SETSTALL           (0x04 << 0)
/* for core versions 1.90a and earlier */
#define SPRD_DWC3_DEPCMD_GETSEQNUMBER       (0x03 << 0)
/* for core versions 1.94a and later */
#define SPRD_DWC3_DEPCMD_GETEPSTATE         (0x03 << 0)
#define SPRD_DWC3_DEPCMD_SETTRANSFRESOURCE  (0x02 << 0)
#define SPRD_DWC3_DEPCMD_SETEPCONFIG        (0x01 << 0)

/* EP number goes 0..31 so ep0 is always out and ep1 is always in */
#define SPRD_DWC3_DALEPENA_EP(n)          (1 << n)

#define SPRD_DWC3_DEPCMD_TYPE_CONTROL     0
#define SPRD_DWC3_DEPCMD_TYPE_ISOC        1
#define SPRD_DWC3_DEPCMD_TYPE_BULK        2
#define SPRD_DWC3_DEPCMD_TYPE_INTR        3

struct dwc3_trb;

/*
 * Software event buffer representation
 */
struct dwc3_event_buffer {
        void           *buf;      /* buffer */
        unsigned        length;   /* size of this buffer */
        unsigned int    lpos;     /* event offset */
        unsigned int    count;    /* cache of last read event count register */
        unsigned int    flags;    /* flags related to this event buffer */
        dma_addr_t      dma;      /* dma_addr_t */
        struct dwc3    *dwc;      /* pointer to DWC controller */
};

/* Related to dwc3_event_buffer->flags */
#define SPRD_DWC3_EVENT_PENDING         (1UL << 0)

#define SPRD_DWC3_EP_FLAG_STALLED       (1 << 0)
#define SPRD_DWC3_EP_FLAG_WEDGED        (1 << 1)

#define SPRD_DWC3_EP_DIRECTION_TX       true
#define SPRD_DWC3_EP_DIRECTION_RX       false

#define SPRD_DWC3_TRB_NUM               32
#define SPRD_DWC3_TRB_MASK              (SPRD_DWC3_TRB_NUM - 1)

/**
 * struct dwc3_ep - device side endpoint representation
 */
struct dwc3_ep {
        struct usb_ep     endpoint;     /* usb endpoint */
        struct list_head  request_list; /* list of requests for this endpoint */
        struct list_head  req_queued;   /* list of requests on this ep which have TRBs setup */

        struct dwc3_trb  *trb_pool;     /* array of transaction buffers */
        dma_addr_t        trb_pool_dma; /* dma address of @trb_pool */
        u32               free_slot;    /* next slot which is going to be used */
        u32               busy_slot;    /* first slot which is owned by HW */

        /* usb_endpoint_descriptor pointer */
        const struct usb_ss_ep_comp_descriptor *comp_desc;

        struct dwc3      *dwc;          /* pointer to DWC controller */
        u32               saved_state;  /* ep state saved during hibernation */
        unsigned          flags;        /* endpoint flags (wedged, stalled, ...) */
#define SPRD_DWC3_EP_ENABLED             (1 << 0)
#define SPRD_DWC3_EP_STALL               (1 << 1)
#define SPRD_DWC3_EP_WEDGE               (1 << 2)
#define SPRD_DWC3_EP_BUSY                (1 << 4)
#define SPRD_DWC3_EP_PENDING_REQUEST     (1 << 5)
#define SPRD_DWC3_EP_MISSED_ISOC         (1 << 6)
/* This is specific to EP0 */
#define SPRD_DWC3_EP0_DIR_IN             (1 << 31)

        unsigned         current_trb;  /* index of current used trb */
        u8               number;       /* endpoint number (1-15) */
        u8               type;         /* set to bmAttributes & USB_ENDPOINT_XFERTYPE_MASK */
        u8               resource_index; /* Resource transfer index */
        u32              interval;     /* the interval on which the ISOC transfer is started */
        char             name[20];     /* a human readable name e.g. ep1out-bulk */
        unsigned         direction:1;  /* true for TX, false for RX */
        unsigned         stream_capable:1; /* true when streams are enabled */
};

enum dwc3_phy {
        SPRD_DWC3_PHY_UNKNOWN = 0,
        SPRD_DWC3_PHY_USB3,
        SPRD_DWC3_PHY_USB2,
};

enum dwc3_ep0_next {
        SPRD_DWC3_EP0_UNKNOWN = 0,
        SPRD_DWC3_EP0_COMPLETE,
        SPRD_DWC3_EP0_NRDY_DATA,
        SPRD_DWC3_EP0_NRDY_STATUS,
};

enum dwc3_ep0_state {
        SPRD_DWC3_EP0_STATE_UNCONNECTED = 0,
        SPRD_DWC3_EP0_STATE_SETUP_PHASE,
        SPRD_DWC3_EP0_STATE_DATA_PHASE,
        SPRD_DWC3_EP0_STATE_STATUS_PHASE,
};

enum dwc3_link_state {
        /* In SuperSpeed */
        SPRD_DWC3_LINK_STATE_U0       = 0x00, /* in HS, means ON */
        SPRD_DWC3_LINK_STATE_U1       = 0x01,
        SPRD_DWC3_LINK_STATE_U2       = 0x02, /* in HS, means SLEEP */
        SPRD_DWC3_LINK_STATE_U3       = 0x03, /* in HS, means SUSPEND */
        SPRD_DWC3_LINK_STATE_SS_DIS   = 0x04,
        SPRD_DWC3_LINK_STATE_RX_DET   = 0x05, /* in HS, means Early Suspend */
        SPRD_DWC3_LINK_STATE_SS_INACT = 0x06,
        SPRD_DWC3_LINK_STATE_POLL     = 0x07,
        SPRD_DWC3_LINK_STATE_RECOV    = 0x08,
        SPRD_DWC3_LINK_STATE_HRESET   = 0x09,
        SPRD_DWC3_LINK_STATE_CMPLY    = 0x0a,
        SPRD_DWC3_LINK_STATE_LPBK     = 0x0b,
        SPRD_DWC3_LINK_STATE_RESET    = 0x0e,
        SPRD_DWC3_LINK_STATE_RESUME   = 0x0f,
        SPRD_DWC3_LINK_STATE_MASK     = 0x0f,
};

/* TRB: Length, PCM and Status */
#define SPRD_DWC3_TRB_SIZE_MASK        (0x00ffffff)
#define SPRD_DWC3_TRB_SIZE_LENGTH(n)   ((n) & SPRD_DWC3_TRB_SIZE_MASK)
#define SPRD_DWC3_TRB_SIZE_PCM1(n)     (((n) & 0x03) << 24)
#define SPRD_DWC3_TRB_SIZE_TRBSTS(n)   (((n) & (0x0f << 28)) >> 28)

#define SPRD_DWC3_TRBSTS_OK                   0
#define SPRD_DWC3_TRBSTS_MISSED_ISOC          1
#define SPRD_DWC3_TRBSTS_SETUP_PENDING        2
#define SPRD_DWC3_TRB_STS_XFER_IN_PROG        4

/* TRB: Control */
#define SPRD_DWC3_TRB_CTRL_HWO                (1 << 0)
#define SPRD_DWC3_TRB_CTRL_LST                (1 << 1)
#define SPRD_DWC3_TRB_CTRL_CHN                (1 << 2)
#define SPRD_DWC3_TRB_CTRL_CSP                (1 << 3)
#define SPRD_DWC3_TRB_CTRL_TRBCTL(n)          (((n) & 0x3f) << 4)
#define SPRD_DWC3_TRB_CTRL_ISP_IMI            (1 << 10)
#define SPRD_DWC3_TRB_CTRL_IOC                (1 << 11)
#define SPRD_DWC3_TRB_CTRL_SID_SOFN(n)        (((n) & 0xffff) << 14)

#define SPRD_DWC3_TRBCTL_NORMAL               SPRD_DWC3_TRB_CTRL_TRBCTL(1)
#define SPRD_DWC3_TRBCTL_CONTROL_SETUP        SPRD_DWC3_TRB_CTRL_TRBCTL(2)
#define SPRD_DWC3_TRBCTL_CONTROL_STATUS2      SPRD_DWC3_TRB_CTRL_TRBCTL(3)
#define SPRD_DWC3_TRBCTL_CONTROL_STATUS3      SPRD_DWC3_TRB_CTRL_TRBCTL(4)
#define SPRD_DWC3_TRBCTL_CONTROL_DATA         SPRD_DWC3_TRB_CTRL_TRBCTL(5)
#define SPRD_DWC3_TRBCTL_ISOCHRONOUS_FIRST    SPRD_DWC3_TRB_CTRL_TRBCTL(6)
#define SPRD_DWC3_TRBCTL_ISOCHRONOUS          SPRD_DWC3_TRB_CTRL_TRBCTL(7)
#define SPRD_DWC3_TRBCTL_LINK_TRB             SPRD_DWC3_TRB_CTRL_TRBCTL(8)

/*
 * transfer request block (hw format)
 */
struct dwc3_trb {
        u32        bpl;  /* DW0-3 */
        u32        bph;  /* DW4-7 */
        u32        size; /* DW8-B */
        u32        ctrl; /* DWC-F */
} __packed;

/*
 * copy of HWPARAMS registers
 */
struct dwc3_hwparams {
        u32        hwparams0;  /* GHWPARAMS0 */
        u32        hwparams1;  /* GHWPARAMS1 */
        u32        hwparams2;  /* GHWPARAMS2 */
        u32        hwparams3;  /* GHWPARAMS3 */
        u32        hwparams4;  /* GHWPARAMS4 */
        u32        hwparams5;  /* GHWPARAMS5 */
        u32        hwparams6;  /* GHWPARAMS6 */
        u32        hwparams7;  /* GHWPARAMS7 */
        u32        hwparams8;  /* GHWPARAMS8 */
};

/* HWPARAMS0 */
#define SPRD_DWC3_MODE(n)            ((n) & 0x7)

#define SPRD_DWC3_MDWIDTH(n)         (((n) & 0xff00) >> 8)

/* HWPARAMS1 */
#define SPRD_DWC3_NUM_INT(n)         (((n) & (0x3f << 15)) >> 15)

/* HWPARAMS3 */
#define SPRD_DWC3_NUM_IN_EPS_MASK    (0x1f << 18)
#define SPRD_DWC3_NUM_EPS_MASK       (0x3f << 12)
#define SPRD_DWC3_NUM_EPS(p)         (((p)->hwparams3 & (SPRD_DWC3_NUM_EPS_MASK)) >> 12)
#define SPRD_DWC3_NUM_IN_EPS(p)      (((p)->hwparams3 & (SPRD_DWC3_NUM_IN_EPS_MASK)) >> 18)

/* HWPARAMS7 */
#define SPRD_DWC3_RAM1_DEPTH(n)      ((n) & 0xffff)


struct dwc3_request {
        struct usb_request      request;
        struct list_head        list;
        struct dwc3_ep         *dep;
        u32                     start_slot;

        u8                      epnum;
        struct dwc3_trb        *trb;
        dma_addr_t              trb_dma;

        unsigned                direction:1;
        unsigned                mapped:1;
        unsigned                queued:1;
};

/*
 * hibernation scratchpad array
 */
struct dwc3_scratchpad_array {
        __le64        dma_adr[SPRD_DWC3_MAX_HIBER_SCRATCHBUFS];
};

/*
 * representation of our dwc3 controller
 */
struct dwc3 {
        /* usb control request which is used for ep0 */
        struct usb_ctrlrequest    *ctrl_req;
        /* trb which is used for the ctrl_req */
        struct dwc3_trb           *ep0_trb;
        /* bounce buffer for ep0 */
        void                      *ep0_bounce;
        void                      *scratchbuf;
        /* used while precessing STD USB requests */
        u8                        *setup_buf;
        /* dma address of ctrl_req */
        dma_addr_t                 ctrl_req_addr;
        /* dma address of ep0_trb */
        dma_addr_t                 ep0_trb_addr;
        /* dma address of ep0_bounce */
        dma_addr_t                 ep0_bounce_addr;
        /* dma address of scratchbuf */
        dma_addr_t                 scratch_addr;
        /* dummy req used while handling STD USB requests */
        struct dwc3_request        ep0_usb_req;

        /* for synchronizing */
        spinlock_t                 lock;
        /* pointer to our struct device */
//#if defined(__UBOOT__) && CONFIG_IS_ENABLED(DM_USB)
#if 0
        struct udevice            *dev;
#else
        struct device             *dev;
#endif

        /* pointer to our xHCI child */
        struct platform_device    *xhci;
        /* struct resources for our child */
        struct resource            xhci_resources[SPRD_DWC3_XHCI_RESOURCES_NUM];

        /* struct dwc3_event_buffer pointer */
        struct dwc3_event_buffer **ev_buffs;
        /* endpoint array */
        struct dwc3_ep            *eps[SPRD_DWC3_ENDPOINTS_NUM];

        /* device side representation of the peripheral controller */
        struct usb_gadget          gadget;
        /* pointer to the gadget driver */
        struct usb_gadget_driver  *gadget_driver;

        /* base address for our registers */
        void __iomem              *regs;
        /* address space size */
        size_t                     regs_size;

        /* requested mode of operation */
        u32                        dr_mode;
        /* UTMI phy mode, one of following: */
        /*                - USBPHY_INTERFACE_MODE_UTMI */
        /*                - USBPHY_INTERFACE_MODE_UTMIW */
        enum usb_phy_interface     hsphy_mode;
        /* used for suspend/resume */
        /* saved contents of DCFG register */
        u32                        dcfg;
        /* saved contents of GCTL register */
        u32                        gctl;

        /* number of scratch buffers */
        u32                        nr_scratch;
        /* calculated number of event buffers */
        u32                        num_event_buffers;
        /* only used on revisions <1.83a for workaround */
        u32                        u1u2;
        /* maximum speed requested (mainly for testing purposes) */
        u32                        maximum_speed;
        /* revision register contents */
        u32                        revision;

#define SPRD_DWC3_REVISION_173A        0x5533173a
#define SPRD_DWC3_REVISION_175A        0x5533175a
#define SPRD_DWC3_REVISION_180A        0x5533180a
#define SPRD_DWC3_REVISION_183A        0x5533183a
#define SPRD_DWC3_REVISION_185A        0x5533185a
#define SPRD_DWC3_REVISION_187A        0x5533187a
#define SPRD_DWC3_REVISION_188A        0x5533188a
#define SPRD_DWC3_REVISION_190A        0x5533190a
#define SPRD_DWC3_REVISION_194A        0x5533194a
#define SPRD_DWC3_REVISION_200A        0x5533200a
#define SPRD_DWC3_REVISION_202A        0x5533202a
#define SPRD_DWC3_REVISION_210A        0x5533210a
#define SPRD_DWC3_REVISION_220A        0x5533220a
#define SPRD_DWC3_REVISION_230A        0x5533230a
#define SPRD_DWC3_REVISION_240A        0x5533240a
#define SPRD_DWC3_REVISION_250A        0x5533250a
#define SPRD_DWC3_REVISION_260A        0x5533260a
#define SPRD_DWC3_REVISION_270A        0x5533270a
#define SPRD_DWC3_REVISION_280A        0x5533280a
#define SPRD_DWC3_REVISION_290A        0x5533290a
#define SPRD_DWC3_REVISION_300A        0x5533300a
#define SPRD_DWC3_REVISION_310A        0x5533310a

/*
 * NOTICE: we're using bit 31 as a "is usb 3.1" flag. This is really
 * just so dwc31 revisions are always larger than dwc3.
 */
#define SPRD_DWC3_REVISION_IS_SPRD_DWC31     0x80000000
#define SPRD_DWC3_USB31_REVISION_110A        (0x3131302a | SPRD_DWC3_REVISION_IS_SPRD_DWC31)
#define SPRD_DWC3_USB31_REVISION_120A        (0x3132302a | SPRD_DWC3_REVISION_IS_SPRD_DWC31)

        /* hold the next expected event */
        enum dwc3_ep0_next       ep0_next_event;
        /* state of endpoint zero */
        enum dwc3_ep0_state      ep0state;
        /* link state */
        enum dwc3_link_state     link_state;

        /* wValue from Set Isochronous Delay request; */
        u16                      isoch_delay;
        /* parameter from Set SEL request. */
        u16                      u2sel;
        /* parameter from Set SEL request. */
        u16                      u2pel;
        /* parameter from Set SEL request. */
        u8                       u1sel;
        /* parameter from Set SEL request. */
        u8                       u1pel;

        /* device speed (super, high, full, low) */
        u8                       speed;

        /* number of out endpoints */
        u8                       num_out_eps;
        /* number of in endpoints */
        u8                       num_in_eps;

        /* points to start of memory which is used for this struct. */
        void                    *mem;

        /* copy of hwparams registers */
        struct dwc3_hwparams     hwparams;
        /* debugfs root folder pointer */
        struct dentry           *root;
        /* debugfs pointer to regdump file */
        struct debugfs_regset32 *regset;

        /* true when we're entering a USB test mode */
        u8                      test_mode;
        /* test feature selector */
        u8                      test_mode_nr;
        /* LPM NYET response threshold */
        u8                      lpm_nyet_threshold;
        /* HIRD threshold */
        u8                      hird_threshold;

        /* true when gadget driver asks for delayed status */
        unsigned                delayed_status:1;
        /* true when we used bounce buffer */
        unsigned                ep0_bounced:1;
        /* true when we expect a DATA IN transfer */
        unsigned                ep0_expect_in:1;
        /* true when dwc3 was configured with Hibernation */
        unsigned                has_hibernation:1;
        /* true when core was configured with LPM Erratum. Note that */
        /*                        there's now way for software to detect this in runtime. */
        unsigned                has_lpm_erratum:1;
        /* the core asserts output signal */
        /*         0        - utmi_sleep_n */
        /*         1        - utmi_l1_suspend_n */
        unsigned                is_utmi_l1_suspend:1;
        /* true when we are selfpowered */
        unsigned                is_selfpowered:1;
        /* true when we are using the FPGA board */
        unsigned                is_fpga:1;
        /* not all users might want fifo resizing, flag it */
        unsigned                needs_fifo_resize:1;
        /* true when Run/Stop bit is set */
        unsigned                pullups_connected:1;
        /* tells us it's ok to reconfigure our TxFIFO sizes. */
        unsigned                resize_fifos:1;
        /* true when there's a Setup Packet in FIFO. Workaround */
        unsigned                setup_packet_pending:1;
        /* true when StartConfig command has been issued */
        unsigned                start_config_issued:1;
        /* set if we perform a three phase setup */
        unsigned                three_stage_setup:1;

        /* set if we enable the disable scramble quirk */
        unsigned                disable_scramble_quirk:1;
        /* set if we enable u2exit lfps quirk */
        unsigned                u2exit_lfps_quirk:1;
        /* set if we enable P3 OK for U2/SS Inactive quirk */
        unsigned                u2ss_inp3_quirk:1;
        /* set if we enable request p1p2p3 quirk */
        unsigned                req_p1p2p3_quirk:1;
        /* set if we enable delay p1p2p3 quirk */
        unsigned                del_p1p2p3_quirk:1;
        /* set if we enable delay phy power change quirk */
        unsigned                del_phy_power_chg_quirk:1;
        /* set if we enable LFPS filter quirk */
        unsigned                lfps_filter_quirk:1;
        /* set if we enable rx_detect to polling lfps quirk */
        unsigned                rx_detect_poll_quirk:1;
        /* set if we disable usb3 suspend phy */
        unsigned                dis_u3_susphy_quirk:1;
        /* set if we disable usb2 suspend phy */
        unsigned                dis_u2_susphy_quirk:1;

        /* set if we disable delay phy power */
        /*			change quirk. */
        unsigned                dis_del_phy_power_chg_quirk:1;
        /* set if we disable u2mac linestate */
        /*			check during HS transmit */
        unsigned                dis_tx_ipgap_linecheck_quirk:1;
        /* set if we clear enblslpm in GUSB2PHYCFG, */
        /*                      disabling the suspend signal to the PHY. */
        unsigned                dis_enblslpm_quirk:1;
        /* : set if we clear u2_freeclk_exists */
        /*			in GUSB2PHYCFG, specify that USB2 PHY doesn't */
        /*			provide a free-running PHY clock. */
        unsigned                dis_u2_freeclk_exists_quirk:1;

        /* set if we enable Tx de-emphasis quirk */
        unsigned                tx_de_emphasis_quirk:1;

        /* Tx de-emphasis value */
        /*         0        - -6dB de-emphasis */
        /*         1        - -3.5dB de-emphasis */
        /*         2        - No de-emphasis */
        /*         3        - Reserved */
        unsigned                tx_de_emphasis:2;
        /* index of _this_ controller */
        int                     index;
        /* to maintain the list of dwc3 controllers */
        struct list_head        list;
};

/* -------------------------------------------------------------------------- */

struct dwc3_event_type {
        u32        is_devspec:1;
        u32        type:7;
        u32        reserved8_31:24;
} __packed;

#define SPRD_DWC3_DEPEVT_XFERCOMPLETE        0x01
#define SPRD_DWC3_DEPEVT_XFERINPROGRESS      0x02
#define SPRD_DWC3_DEPEVT_XFERNOTREADY        0x03
#define SPRD_DWC3_DEPEVT_RXTXFIFOEVT         0x04
#define SPRD_DWC3_DEPEVT_STREAMEVT           0x06
#define SPRD_DWC3_DEPEVT_EPCMDCMPLT          0x07

/*
 * returns event name
 */
static inline const char *dwc3_get_ep_event_string(u8 event)
{
        switch (event) {
        case SPRD_DWC3_DEPEVT_XFERCOMPLETE:
                return "Transfer Complete";
        case SPRD_DWC3_DEPEVT_XFERINPROGRESS:
                return "Transfer In-Progress";
        case SPRD_DWC3_DEPEVT_XFERNOTREADY:
                return "Transfer Not Ready";
        case SPRD_DWC3_DEPEVT_RXTXFIFOEVT:
                return "FIFO";
        case SPRD_DWC3_DEPEVT_STREAMEVT:
                return "Stream";
        case SPRD_DWC3_DEPEVT_EPCMDCMPLT:
                return "Endpoint Command Complete";
        }

        return "UNKNOWN";
}

/*
 * Device Endpoint Events
 */
struct dwc3_event_depevt {
        /* indicates this is an endpoint event (not used) */
        u32        one_bit:1;
        /* number of the endpoint */
        u32        endpoint_number:5;

        /* The event we have:
         *  0x00 - Reserved
         *  0x01 - XferComplete
         *  0x02 - XferInProgress
         *  0x03 - XferNotReady
         *  0x04 - RxTxFifoEvt (IN->Underrun, OUT->Overrun)
         *  0x05 - Reserved
         *  0x06 - StreamEvt
         *  0x07 - EPCmdCmplt
         */
        u32        endpoint_event:4;
        /* Reserved, don't use. */
        u32        reserved11_10:2;
        /* Indicates the status of the event. */
        u32        status:4;

/* Status: Within XferNotReady */
#define DEPEVT_STATUS_TRANSFER_ACTIVE        (1 << 3)

/* Status: Within XferComplete */
#define DEPEVT_STATUS_BUSERR     (1 << 0)
#define DEPEVT_STATUS_SHORT      (1 << 1)
#define DEPEVT_STATUS_IOC        (1 << 2)
#define DEPEVT_STATUS_LST        (1 << 3)

/* For Stream event only */
#define DEPEVT_STREAMEVT_FOUND           1
#define DEPEVT_STREAMEVT_NOTFOUND        2

/* For Control-only Status */
#define DEPEVT_STATUS_CONTROL_DATA       1
#define DEPEVT_STATUS_CONTROL_STATUS     2

        /* Parameters of the current event. */
        u32        parameters:16;
} __packed;

/*
 * Device Events
 */
struct dwc3_event_devt {
        u32        one_bit:1;        /* indicates this is a non-endpoint event (not used) */
        u32        device_event:7;   /* indicates it's a device event. Should read as 0x00 */

       /* the type of device event.
        *  0  - DisconnEvt
        *  1  - USBRst
        *  2  - ConnectDone
        *  3  - ULStChng
        *  4  - WkUpEvt
        *  5  - Reserved
        *  6  - EOPF
        *  7  - SOF
        *  8  - Reserved
        *  9  - ErrticErr
        *  10 - CmdCmplt
        *  11 - EvntOverflow
        *  12 - VndrDevTstRcved
        */
        u32        type:4;
        u32        reserved15_12:4;  /* Reserved, not used */
        u32        event_info:9;     /* Information about this event */
        u32        reserved31_25:7;  /* Reserved, not used */
} __packed;

/*
 * Other Core Events
 */
struct dwc3_event_gevt {
        u32        one_bit:1;          /* indicates this is a non-endpoint event (not used)*/
        u32        device_event:7;     /* indicates it's (0x03) Carkit or (0x04) I2C event. */
        u32        phy_port_number:4;  /* self-explanatory */
        u32        reserved31_12:20;   /* Reserved, not used */
} __packed;

/*
 * representation of Event Buffer contents
 */
union dwc3_event {
        u32                         raw;    /* raw 32-bit event */
        struct dwc3_event_type      type;   /* the type of the event */
        struct dwc3_event_depevt    depevt; /* Device Endpoint Event */
        struct dwc3_event_devt      devt;   /* Device Event */
        struct dwc3_event_gevt      gevt;   /* Global Event */
};

/*
 * endpoint command parameters
 */
typedef struct dwc3_gadget_ep_cmd_params {
        u32        param2;
        u32        param1;
        u32        param0;
} dwc3_cmd_params_t;

/*
 * DWC3 Device
 */
struct dwc3_device {
        struct dwc3 *dwc;
        int base;
        /* requested mode of operation */
        u32 dr_mode;
        /* UTMI phy mode, one of following: */
        /*                - USBPHY_INTERFACE_MODE_UTMI */
        /*                - USBPHY_INTERFACE_MODE_UTMIW */
        enum usb_phy_interface hsphy_mode;
        /* maximum speed requested (mainly for testing purposes) */
        u32 maximum_speed;
        unsigned tx_fifo_resize:1;
        /* true when core was configured with LPM Erratum. Note that */
        /*                        there's now way for software to detect this in runtime. */
        unsigned has_lpm_erratum;
        /* LPM NYET response threshold */
        u8 lpm_nyet_threshold;
        /* the core asserts output signal */
        /*         0        - utmi_sleep_n */
        /*         1        - utmi_l1_suspend_n */
        unsigned is_utmi_l1_suspend;
        /* HIRD threshold */
        u8 hird_threshold;
        /* set if we enable the disable scramble quirk */
        unsigned disable_scramble_quirk;
        /* set if we enable u2exit lfps quirk */
        unsigned u2exit_lfps_quirk;
        /* set if we enable P3 OK for U2/SS Inactive quirk */
        unsigned u2ss_inp3_quirk;
        /* set if we enable request p1p2p3 quirk */
        unsigned req_p1p2p3_quirk;
        /* set if we enable delay p1p2p3 quirk */
        unsigned del_p1p2p3_quirk;
        /* set if we enable delay phy power change quirk */
        unsigned del_phy_power_chg_quirk;
        /* set if we enable LFPS filter quirk */
        unsigned lfps_filter_quirk;
        /* set if we enable rx_detect to polling lfps quirk */
        unsigned rx_detect_poll_quirk;
        /* set if we disable usb3 suspend phy */
        unsigned dis_u3_susphy_quirk;
        /* set if we disable usb2 suspend phy */
        unsigned dis_u2_susphy_quirk;
        /* set if we enable Tx de-emphasis quirk */
        unsigned tx_de_emphasis_quirk;
        /* Tx de-emphasis value */
        /*  0 - -6dB de-emphasis */
        /*  1 - -3.5dB de-emphasis */
        /*  2 - No de-emphasis */
        /*  3 - Reserved */
        unsigned tx_de_emphasis;
};

/* -------------------------------------------------------------------------- */
int dwc3_gadget_resize_tx_fifos(struct dwc3 *dwc);
int dwc3_gadget_init(struct dwc3 *dwc);
void dwc3_gadget_exit(struct dwc3 *dwc);
int dwc3_gadget_set_test_mode(struct dwc3 *dwc, int mode);
int dwc3_gadget_get_link_state(struct dwc3 *dwc);
int dwc3_gadget_set_link_state(struct dwc3 *dwc, enum dwc3_link_state state);
int dwc3_send_gadget_ep_cmd(struct dwc3 *dwc, unsigned ep,
                unsigned cmd, struct dwc3_gadget_ep_cmd_params *params);
int dwc3_send_gadget_generic_command(struct dwc3 *dwc, unsigned cmd, u32 param);
int dwc3_uboot_init(struct dwc3_device *dev);
void dwc3_uboot_exit(struct dwc3_device *dev);
void dwc3_uboot_handle_interrupt(struct dwc3_device *dev);

#endif /* __DRIVERS_USB_SPRD_DWC3_CORE_H__ */
