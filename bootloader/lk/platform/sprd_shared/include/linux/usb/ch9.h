
#ifndef __LINUX_USB_CH9_H
#define __LINUX_USB_CH9_H

#include <linux/types.h>	/* __u8 etc */
#include <asm/byteorder.h>	/* le16_to_cpu */
#include <sprd_unaligned.h>	/* get_unaligned() */

/*-------------------------------------------------------------------------*/
/*
 * USB recipients, the third of three bRequestType fields
 */
#define USB_RECIP_OTHER			0x03
#define USB_RECIP_ENDPOINT		0x02
#define USB_RECIP_INTERFACE		0x01
#define USB_RECIP_DEVICE		0x00
#define USB_RECIP_MASK			0x1f

/* From Wireless USB 1.0 */
#define USB_RECIP_RPIPE		0x05
#define USB_RECIP_PORT			0x04

/*
 * USB types, the second of three bRequestType fields
 */
#define USB_TYPE_RESERVED		(0x03 << 5)
#define USB_TYPE_VENDOR			(0x02 << 5)
#define USB_TYPE_CLASS			(0x01 << 5)
#define USB_TYPE_STANDARD		(0x00 << 5)
#define USB_TYPE_MASK			(0x03 << 5)

/*
 * This bit flag is used in endpoint descriptors' bEndpointAddress field.
 * It's also one of three fields in control requests bRequestType.
 */
#define USB_DIR_IN			0x80		/* to host */
#define USB_DIR_OUT			0		/* to device */

/*
 * Standard requests, for the bRequest field of a SETUP packet.
 */
#define USB_REQ_SET_ISOCH_DELAY		0x31
#define USB_REQ_SET_SEL			0x30
#define USB_REQ_SYNCH_FRAME		0x0C
#define USB_REQ_SET_INTERFACE		0x0B
#define USB_REQ_GET_INTERFACE		0x0A
#define USB_REQ_SET_CONFIGURATION	0x09
#define USB_REQ_GET_CONFIGURATION	0x08
#define USB_REQ_SET_DESCRIPTOR		0x07
#define USB_REQ_GET_DESCRIPTOR		0x06
#define USB_REQ_SET_ADDRESS		0x05
#define USB_REQ_SET_FEATURE		0x03
#define USB_REQ_CLEAR_FEATURE		0x01
#define USB_REQ_GET_STATUS		0x00

/* Wireless USB */
#define USB_REQ_SET_INTERFACE_DS	0x17
#define USB_REQ_LOOPBACK_DATA_READ	0x16
#define USB_REQ_LOOPBACK_DATA_WRITE	0x15
#define USB_REQ_SET_WUSB_DATA		0x14
#define USB_REQ_GET_SECURITY_DATA	0x13
#define USB_REQ_SET_SECURITY_DATA	0x12
#define USB_REQ_SET_CONNECTION		0x11
#define USB_REQ_GET_HANDSHAKE		0x10
#define USB_REQ_RPIPE_RESET		0x0F
#define USB_REQ_SET_HANDSHAKE		0x0F
#define USB_REQ_RPIPE_ABORT		0x0E
#define USB_REQ_GET_ENCRYPTION		0x0E
#define USB_REQ_SET_ENCRYPTION		0x0D

/* the link Powerer Management (LPM) ECN defines USB_REQ_TEST_AND_SET command,
 * used by hubs to put ports into a new L1 suspend states, except that it
 * forget to define its numbers ...
 */

/*
 * Test Mode Selectors
 * See USB 2.0 spec Table 9-7
 */
#define	TEST_FORCE_EN	5
#define	TEST_PACKET	4
#define	TEST_SE0_NAK	3
#define	TEST_K		2
#define	TEST_J		1

/*
 * USB features flags are written using USB_REQ_{CLEAR,SET}_FEATURE, and
 * are readed as a bit array returned by USB_REQ_GET_STATUS.  (So there
 * are at most sixteen features of each type)  Hubs may also support
 * a new USB_REQ_TEST_AND_SET_FEATURE to put port into L1 suspend.
 */
#define USB_DEVICE_DEBUG_MODE		6	/* (special devices only) */
#define USB_DEVICE_A_ALT_HNP_SUPPORT	5	/* (otg) other RH port does */
#define USB_DEVICE_A_HNP_SUPPORT	4	/* (otg) RH port supports HNP */
#define USB_DEVICE_WUSB_DEVICE		3	/* (wireless)*/
#define USB_DEVICE_B_HNP_ENABLE		3	/* (otg) dev may initiate HNP */
#define USB_DEVICE_BATTERY		2	/* (wireless) */
#define USB_DEVICE_TEST_MODE		2	/* (wired high speed only) */
#define USB_DEVICE_REMOTE_WAKEUP	1	/* dev may initiate wakeup */
#define USB_DEVICE_SELF_POWERED		0	/* (read only) */

/*
 * Suspended Options, Table 9-7 USB 3.0 spec
 */
#define USB_INTRF_FUNC_SUSPEND_RW	(1 << (8 + 1))
#define USB_INTRF_FUNC_SUSPEND_LP	(1 << (8 + 0))

/*
 * New Feature Selectors  added by USB 3.0
 * See USB 3.0 spec Tables 9-6
 */
#define USB_INTRF_FUNC_SUSPEND	0	/* function suspend */
#define USB_DEVICE_LTM_ENABLE	50	/* dev may send LTM */
#define USB_DEVICE_U2_ENABLE	49	/* dev may initiate U2 transition */
#define USB_DEVICE_U1_ENABLE	48	/* dev may initiate U1 transition */

#define USB_ENDPOINT_HALT		0	/* IN/OUT will STALL */

#define USB_INTR_FUNC_SUSPEND_OPT_MASK	0xFF00

/**
 * bRequestType: matches the USB bmRequestType field
 * bRequest: matches the USB bRequest field
 * wValue: matches the USB wValue field (le16 byte order)
 * wIndex: matches the USB wIndex field (le16 byte order)
 * wLength: matches the USB wLength field (le16 byte order)
 * structure is used to send control requests to a USB device.  It matches
 * different fields of the USB 2.0 Spec section 9.3, table 9-2.  See the
 * USB spec for a fuller description of the different fields, and what they are
 * used for
 * that the driver for any interface can issue control requests.
 * For most devices, interfaces don't coordinate with each other,
 */
struct usb_ctrlrequest {
	__u8 bRequestType;
	__u8 bRequest;
	__le16 wValue;
	__le16 wIndex;
	__le16 wLength;
} __attribute__ ((packed));

/* Bit array elements as returned by the USB_REQ_GET_STATUS request. */
#define USB_DEV_STAT_LTM_ENABLED	4	/* Latency tolerance messages */
#define USB_DEV_STAT_U2_ENABLED		3	/* transition into U2 state */
#define USB_DEV_STAT_U1_ENABLED		2	/* transition into U1 state */

/*-------------------------------------------------------------------------*/

/*
 * Descriptor types ... USB 2.0 spec table 9.5
 */
#define USB_DT_INTERFACE_POWER		0x08
#define USB_DT_OTHER_SPEED_CONFIG	0x07
#define USB_DT_DEVICE_QUALIFIER		0x06
#define USB_DT_ENDPOINT			0x05
#define USB_DT_INTERFACE		0x04
#define USB_DT_STRING			0x03
#define SPRD_USB_DT_CONFIG			0x02
#define SPRD_USB_DT_DEVICE			0x01
/* these are from a minor usb 2.0 revision (ECN) */
#define USB_DT_INTERFACE_ASSOCIATION	0x0b
#define USB_DT_DEBUG			0x0a
#define USB_DT_OTG			0x09
/* these are from the Wireless USB spec */
#define USB_DT_CS_RADIO_CONTROL		0x23
#define USB_DT_RPIPE			0x22
#define USB_DT_WIRE_ADAPTER		0x21
#define USB_DT_WIRELESS_ENDPOINT_COMP	0x11
#define USB_DT_DEVICE_CAPABILITY	0x10
#define USB_DT_BOS			0x0f
#define USB_DT_ENCRYPTION_TYPE		0x0e
#define USB_DT_KEY			0x0d
#define USB_DT_SECURITY			0x0c

/* From the T10 UAS specification */
#define USB_DT_PIPE_USAGE		0x24
/* From the USB 3.0 spec */
#define	USB_DT_SS_ENDPOINT_COMP		0x30

/* All standard descriptors have these 2 fields at the beginning */
struct usb_descriptor_header {
	__u8  bLength;
	__u8  bDescriptorType;
} __attribute__ ((packed));

/* Conventional code for class-specific descriptors.   convention is
 * define in  USB "Common Class" Spec (3.11).  Individual class specs
 * are authoritative for usage, not  "common class" writeup.
 */
#define USB_DT_CS_ENDPOINT		(USB_TYPE_CLASS | USB_DT_ENDPOINT)
#define USB_DT_CS_INTERFACE		(USB_TYPE_CLASS | USB_DT_INTERFACE)
#define USB_DT_CS_STRING		(USB_TYPE_CLASS | USB_DT_STRING)
#define USB_DT_CS_CONFIG		(USB_TYPE_CLASS | USB_DT_CONFIG)
#define USB_DT_CS_DEVICE		(USB_TYPE_CLASS | USB_DT_DEVICE)

/*-------------------------------------------------------------------------*/

#define SPRD_USB_DT_DEVICE_SIZE		18

/*
 * Device and/or Interface Class code
 *  found in bDeviceClass or bInterfaceClass
 * and define in www.usb.org documents
 */
#define USB_CLASS_VENDOR_SPEC		0xff
#define USB_CLASS_APP_SPEC		0xfe
#define USB_CLASS_MISC			0xef
#define USB_CLASS_WIRELESS_CONTROLLER	0xe0
#define USB_CLASS_VIDEO			0x0e
#define USB_CLASS_CONTENT_SEC		0x0d	/* content security */
#define USB_CLASS_CSCID			0x0b	/* chip+ smart card */
#define USB_CLASS_CDC_DATA		0x0a
#define USB_CLASS_HUB			9
#define USB_CLASS_MASS_STORAGE		8
#define USB_CLASS_PRINTER		7
#define USB_CLASS_STILL_IMAGE		6
#define USB_CLASS_PHYSICAL		5
#define USB_CLASS_HID			3
#define USB_CLASS_COMM			2
#define USB_CLASS_AUDIO			1
#define USB_CLASS_PER_INTERFACE		0	/* for DeviceClass */

#define USB_SUBCLASS_VENDOR_SPEC	0xff

struct usb_device_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__le16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__le16 idVendor;
	__le16 idProduct;
	__le16 bcdDevice;
	__u8  iManufacturer;
	__u8  iProduct;
	__u8  iSerialNumber;
	__u8  bNumConfigurations;
} __attribute__ ((packed));

/*-------------------------------------------------------------------------*/

/* from config descriptor bmAttributes */
#define USB_CONFIG_ATT_BATTERY		(1 << 4)	/* battery powered */
#define USB_CONFIG_ATT_WAKEUP		(1 << 5)	/* can wakeup */
#define SPRD_USB_CONFIG_ATT_SELFPOWER	(1 << 6)	/* self powered */
#define SPRD_USB_CONFIG_ATT_ONE		(1 << 7)	/* must be set */

#define SPRD_USB_DT_CONFIG_SIZE		9

/* Configuration descriptor information.
 * USB_DT_OTHER_SPEED_CONFIG is same descriptor, except that the
 * descriptor type is different.  Highspeed-capable devices can looks
 * different depending on what speed they are currently running.  Only
 * devices with  USB_DT_DEVICE_QUALIFIER have any OTHER_SPEED_CONFIG descriptors.
 */
struct usb_config_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__le16 wTotalLength;
	__u8  bNumInterfaces;
	__u8  bConfigurationValue;
	__u8  iConfiguration;
	__u8  bmAttributes;
	__u8  bMaxPower;
} __attribute__ ((packed));

#define USB_DT_INTERFACE_SIZE		9

/* USB_DT_INTERFACE: Interface descriptor */
struct usb_interface_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bInterfaceNumber;
	__u8  bAlternateSetting;
	__u8  bNumEndpoints;
	__u8  bInterfaceClass;
	__u8  bInterfaceSubClass;
	__u8  bInterfaceProtocol;
	__u8  iInterface;
} __attribute__ ((packed));

/* USB_DT_STRING: String descriptor */
struct usb_string_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__le16 wData[1];		/* UTF-16LE encoded */
} __attribute__ ((packed));

/*-------------------------------------------------------------------------*/
/* Used to access common field */
struct usb_generic_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
};

#define USB_DT_ENDPOINT_AUDIO_SIZE	9	/* Audio extension */
#define USB_DT_ENDPOINT_SIZE		7

/* Endpoint descriptor */
struct usb_endpoint_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bEndpointAddress;
	__u8  bmAttributes;
	__le16 wMaxPacketSize;
	__u8  bInterval;

	__u8  bRefresh;
	__u8  bSynchAddress;
} __attribute__ ((packed));


/* The USB 3.0 spec redefines bits 5:4 of bmAttributes as interrupt ep type. */
#define USB_ENDPOINT_INTR_NOTIFICATION	(1 << 4)
#define USB_ENDPOINT_INTR_PERIODIC	(0 << 4)
#define USB_ENDPOINT_INTRTYPE		0x30

#define USB_ENDPOINT_SYNC_SYNC		(3 << 2)
#define USB_ENDPOINT_SYNC_ADAPTIVE	(2 << 2)
#define USB_ENDPOINT_SYNC_ASYNC		(1 << 2)
#define USB_ENDPOINT_SYNC_NONE		(0 << 2)
#define USB_ENDPOINT_SYNCTYPE		0x0c

#define USB_ENDPOINT_USAGE_IMPLICIT_FB	0x20	/* Implicit feedback Data endpoint */
#define USB_ENDPOINT_USAGE_FEEDBACK	0x10
#define USB_ENDPOINT_USAGE_DATA		0x00
#define USB_ENDPOINT_USAGE_MASK		0x30

/*
 * Endpoints
 */
#define USB_ENDPOINT_DIR_MASK		0x80
#define USB_ENDPOINT_NUMBER_MASK	0x0f	/* in bEndpointAddress */

#define USB_ENDPOINT_MAX_ADJUSTABLE	0x80
#define USB_ENDPOINT_XFER_INT		3
#define USB_ENDPOINT_XFER_BULK		2
#define USB_ENDPOINT_XFER_ISOC		1
#define USB_ENDPOINT_XFER_CONTROL	0
#define USB_ENDPOINT_XFERTYPE_MASK	0x03	/* in bmAttributes */

/*-------------------------------------------------------------------------*/
static inline int usb_endpoint_interrupt_type(
		const struct usb_endpoint_descriptor *epd)
{
	return epd->bmAttributes & USB_ENDPOINT_INTRTYPE;
}

static inline int usb_endpoint_maxp(const struct usb_endpoint_descriptor *epd)
{
	return __le16_to_cpu(get_unaligned(&epd->wMaxPacketSize));
}

static inline int usb_endpoint_xfer_bulk(const struct usb_endpoint_descriptor *uepd)
{
	return (USB_ENDPOINT_XFER_BULK == (uepd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK));
}



static inline int usb_endpoint_dir_out(const struct usb_endpoint_descriptor *uepd)
{
	return (USB_DIR_OUT == (uepd->bEndpointAddress & USB_ENDPOINT_DIR_MASK));
}

/* SuperSpeed Endpoint Companion descriptor */
struct usb_ss_ep_comp_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bMaxBurst;
	__u8  bmAttributes;
	__le16 wBytesPerInterval;
} __attribute__ ((packed));

static inline int usb_endpoint_dir_in(const struct usb_endpoint_descriptor *uepd)
{
	return (USB_DIR_IN == (uepd->bEndpointAddress & USB_ENDPOINT_DIR_MASK));
}

static inline int usb_endpoint_is_bulk_out(const struct usb_endpoint_descriptor *uepd)
{
	return usb_endpoint_xfer_bulk(uepd) && usb_endpoint_dir_out(uepd);
}

/* USB_DT_INTERFACE_ASSOCIATION: groups interfaces */
struct usb_interface_assoc_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bFirstInterface;
	__u8  bInterfaceCount;
	__u8  bFunctionClass;
	__u8  bFunctionSubClass;
	__u8  bFunctionProtocol;
	__u8  iFunction;
} __attribute__ ((packed));

static inline int usb_endpoint_type(const struct usb_endpoint_descriptor *uepd)
{
	return uepd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
}

/* group of wireless security descriptors, including
 * encryption types available for setting up a CC/association.
 */
struct usb_security_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__le16 wTotalLength;
	__u8  bNumEncryptionTypes;
} __attribute__((packed));

static inline int usb_endpoint_is_bulk_in(const struct usb_endpoint_descriptor *uepd)
{
	return usb_endpoint_xfer_bulk(uepd) && usb_endpoint_dir_in(uepd);
}


static inline int usb_endpoint_xfer_control(const struct usb_endpoint_descriptor *uepd)
{
	return (USB_ENDPOINT_XFER_CONTROL == (uepd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK));
}

#define USB_DT_SS_EP_COMP_SIZE		6
/* Device Qualifier descriptor */
struct usb_qualifier_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__le16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__u8  bNumConfigurations;
	__u8  bRESERVED;
} __attribute__ ((packed));


static inline int usb_endpoint_num(const struct usb_endpoint_descriptor *uepd)
{
	return uepd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}

/* modifies or revokes  connection context (CC)
 * CC may also be set up using non-wireless secure channels (including
 * wired USB), and some devices may support CCs with multiple host
 */
struct usb_connection_context {
	__u8 CHID[16];		/* persistent host id */
	__u8 CDID[16];		/* device id (unique w/in host context) */
	__u8 CK[16];		/* connection key */
} __attribute__((packed));

static inline int usb_endpoint_xfer_int(const struct usb_endpoint_descriptor *uepd)
{
	return (USB_ENDPOINT_XFER_INT == (uepd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK));
}

/* for special highspeed devices, replacing serial console */
struct usb_debug_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	/* bulk endpoints with 8 byte maxpacket */
	__u8  bDebugInEndpoint;
	__u8  bDebugOutEndpoint;
} __attribute__((packed));

static inline int usb_endpoint_is_int_in(const struct usb_endpoint_descriptor *uepd)
{
	return usb_endpoint_xfer_int(uepd) && usb_endpoint_dir_in(uepd);
}

/* is a four-way handshake used between  wireless
 * host and  device for connection set up, mutual authentication, and
 * exchanging short lived session key.  The handshake depends on  CC.
 */
struct usb_handshake {
	__u8 bMessageNumber;
	__u8 bStatus;
	__u8 tTKID[3];
	__u8 bReserved;
	__u8 CDID[16];
	__u8 nonce[16];
	__u8 MIC[8];
} __attribute__((packed));

static inline int usb_endpoint_xfer_isoc(const struct usb_endpoint_descriptor *uepd)
{
	return (USB_ENDPOINT_XFER_ISOC == (uepd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK));
}

/* used with {GET,SET}_SECURITY_DATA; only public keys
 * may be retrieved.
 */
struct usb_key_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  tTKID[3];
	__u8  bReserved;
	__u8  bKeyData[0];
} __attribute__((packed));

static inline int usb_endpoint_is_isoc_in(const struct usb_endpoint_descriptor *uepd)
{
	return usb_endpoint_xfer_isoc(uepd) && usb_endpoint_dir_in(uepd);
}

static inline int usb_endpoint_is_isoc_out(const struct usb_endpoint_descriptor *uepd)
{
	return usb_endpoint_xfer_isoc(uepd) && usb_endpoint_dir_out(uepd);
}

static inline int usb_endpoint_is_int_out(const struct usb_endpoint_descriptor *uepd)
{
	return usb_endpoint_xfer_int(uepd) && usb_endpoint_dir_out(uepd);
}

/*-------------------------------------------------------------------------*/



#define USB_SS_MULT(p)			(1 + ((p) & 0x3))

/* Bits 4:0 of bmAttributes if this is a bulk endpoint */
static inline int usb_ss_max_streams(const struct usb_ss_ep_comp_descriptor *comp)
{
	int		max_stream;

	if (!comp)
		return 0;

	max_stream = comp->bmAttributes & 0x1f;

	if (!max_stream)
		return 0;

	max_stream = 1 << max_stream;

	return max_stream;
}

/* from usb_otg_descriptor.bmAttributes */
#define USB_OTG_HNP		(1 << 1)	/* swap host/device roles */
#define USB_OTG_SRP		(1 << 0)


/* (from OTG 1.0a supplement) */
struct usb_otg_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bmAttributes;	/* support for HNP, SRP, etc */
} __attribute__ ((packed));




/* bundled in DT_SECURITY groups */
struct usb_encryption_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bEncryptionType;
#define	USB_ENC_TYPE_RSA_1		3	/* rsa3072/sha1 auth */
#define	USB_ENC_TYPE_CCM_1		2	/* aes128/cbc session */
#define	USB_ENC_TYPE_WIRED		1	/* non-wireless mode */
#define	USB_ENC_TYPE_UNSECURE		0
	__u8  bEncryptionValue;		/* use in SET_ENCRYPTION */
	__u8  bAuthKeyIndex;
} __attribute__((packed));

struct usb_ext_cap_descriptor {		/* Link Power Management */
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDevCapabilityType;
	__le32 bmAttributes;
#define USB_GET_BESL_DEEP(p)		(((p) & (0xf << 12)) >> 12)
#define USB_GET_BESL_BASELINE(p)	(((p) & (0xf << 8)) >> 8)
#define USB_BESL_DEEP_VALID		(1 << 4)	/* Deep BESL valid */
#define USB_BESL_BASELINE_VALID		(1 << 3)	/* Baseline BESL valid*/
#define USB_BESL_SUPPORT		(1 << 2)	/* supports BESL */
#define USB_LPM_SUPPORT			(1 << 1)	/* supports LPM */
} __attribute__((packed));

#define USB_DT_USB_EXT_CAP_SIZE	7

/*  grouped with BOS */
struct usb_dev_cap_header {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDevCapabilityType;
} __attribute__((packed));

struct usb_set_sel_req {
	__u8	u1_sel;
	__u8	u1_pel;
	__le16	u2_sel;
	__le16	u2_pel;
} __attribute__ ((packed));

#define USB3_LPM_DEVICE_INITIATED	0xFF
#define USB3_LPM_U2_MAX_TIMEOUT		0xFE
#define USB3_LPM_U1_MAX_TIMEOUT		0x7F
#define USB3_LPM_DISABLED		0x0

#define	USB_CAP_TYPE_WIRELESS_USB	1

#define USB_DT_BOS_SIZE		5

struct usb_wireless_cap_descriptor {	/* Ultra Wide Band */
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDevCapabilityType;

	__u8  bmAttributes;
#define	USB_WIRELESS_BEACON_NONE	(3 << 2)
#define	USB_WIRELESS_BEACON_DIRECTED	(2 << 2)
#define	USB_WIRELESS_BEACON_SELF	(1 << 2)
#define	USB_WIRELESS_BEACON_MASK	(3 << 2)
#define	USB_WIRELESS_P2P_DRD		(1 << 1)
	__le16 wPHYRates;	/* bit rates, Mbps */
#define	USB_WIRELESS_PHY_480		(1 << 7)
#define	USB_WIRELESS_PHY_400		(1 << 6)
#define	USB_WIRELESS_PHY_320		(1 << 5)
#define	USB_WIRELESS_PHY_200		(1 << 4)	/* always set */
#define	USB_WIRELESS_PHY_160		(1 << 3)
#define	USB_WIRELESS_PHY_107		(1 << 2)	/* always set */
#define	USB_WIRELESS_PHY_80		(1 << 1)
#define	USB_WIRELESS_PHY_53		(1 << 0)	/* always set */
	__u8  bmTFITXPowerInfo;	/* TFI power levels */
	__u8  bmFFITXPowerInfo;	/* FFI power levels */
	__le16 bmBandGroup;
	__u8  bReserved;
} __attribute__((packed));

/* USB 2.0 Extension descriptor */
#define	USB_CAP_TYPE_EXT		2

enum usb3_link_state {
	USB3_LPM_U0 = 0,
	USB3_LPM_U1,
	USB3_LPM_U2,
	USB3_LPM_U3
};

/*
 * Container ID Capability descriptor: Defines the instance unique ID used to
 * identify the instance across all operating modes
 */
#define	CONTAINER_ID_TYPE	4
struct usb_ss_container_id_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDevCapabilityType;
	__u8  bReserved;
	__u8  ContainerID[16]; /* 128-bit number */
} __attribute__((packed));


#define		USB_SS_CAP_TYPE		3
struct usb_ss_cap_descriptor {		/* Link Power Management */
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDevCapabilityType;
	__u8  bmAttributes;
#define USB_LTM_SUPPORT			(1 << 1) /* supports LTM */
	__le16 wSpeedSupported;
#define USB_5GBPS_OPERATION		(1 << 3) /* Operation at 5Gbps */
#define USB_HIGH_SPEED_OPERATION	(1 << 2) /* High speed operation */
#define USB_FULL_SPEED_OPERATION	(1 << 1) /* Full speed operation */
#define USB_LOW_SPEED_OPERATION		(1)	 /* Low speed operation */
	__u8  bFunctionalitySupport;
	__u8  bU1devExitLat;
	__le16 bU2DevExitLat;
} __attribute__((packed));

#define USB_DT_USB_SS_CAP_SIZE	10


#define USB_DT_USB_SS_CONTN_ID_SIZE	20
/*-------------------------------------------------------------------------*/
/*
 * As per USB compliance update,  device that is actively drawing
 * more than 100mA from USB must report itself as bus-powered in the
 *  GetStatus(DEVICE) call
 *please http://compliance.usb.org/index.asp?UpdateFile=Electrical&Format=Standard#34
 */
#define USB_SELF_POWER_VBUS_MAX_DRAW		100

/*  group of device-level capabilities */
struct usb_bos_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__le16 wTotalLength;
	__u8  bNumDeviceCaps;
} __attribute__((packed));

/* USB_DT_WIRELESS_ENDPOINT_COMP:  companion descriptor associated with
 * each endpoint descriptor for a wireless device
 */
struct usb_wireless_ep_comp_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bMaxBurst;
	__u8  bMaxSequence;
	__le16 wMaxStreamDelay;
	__le16 wOverTheAirPacketSize;
	__u8  bOverTheAirInterval;
	__u8  bmCompAttributes;
#define USB_ENDPOINT_SWITCH_MASK	0x03	/* in bmCompAttributes */
#define USB_ENDPOINT_SWITCH_SCALE	2
#define USB_ENDPOINT_SWITCH_SWITCH	1
#define USB_ENDPOINT_SWITCH_NO		0
} __attribute__((packed));

#define USB3_LPM_MAX_U2_SEL_PEL		0xFFFF
#define USB3_LPM_MAX_U1_SEL_PEL		0xFF

enum usb_device_state {
	USB_STATE_NOTATTACHED = 0,

	/* chapter 9 and authentication (wireless) device states */
	USB_STATE_ATTACHED,
	USB_STATE_POWERED,			/* wired */
	USB_STATE_RECONNECTING,			/* auth */
	USB_STATE_UNAUTHENTICATED,		/* auth */
	USB_STATE_DEFAULT,			/* limited function */
	USB_STATE_ADDRESS,
	USB_STATE_CONFIGURED,			/* most functions */

	USB_STATE_SUSPENDED
};

/* USB 2.0 defines three speeds, here is how Linux identifies them */
enum usb_device_speed {
	USB_SPEED_UNKNOWN = 0,			/* enumerating */
	USB_SPEED_LOW, USB_SPEED_FULL,		/* usb 1.1 */
	USB_SPEED_HIGH,				/* usb 2.0 */
	USB_SPEED_WIRELESS,			/* wireless (usb 2.5) */
	USB_SPEED_SUPER,			/* usb 3.0 */
};

/**
 * struct usb_string - wraps a C string and its USB id
 * @id:the (nonzero) ID for this string
 * @s:the string, in UTF-8 encoding
 *
 * If you're using usb_gadget_get_string(), use this to wrap a string
 * together with its ID.
 */
struct usb_string {
	u8 id;
	const char *s;
};

#ifdef __KERNEL__

/**
 *  Returns human readable-name of the speed.
 * @speed:  speed to return human-readable name for.  If it is not
 *   any of  speeds defined in usb_device_speed enum, string for
 *   USB_SPEED_UNKNOWN will be returned.
 */
extern const char *usb_speed_string(enum usb_device_speed speed);

#endif

#endif /* __LINUX_USB_CH9_H */
