/*
 * USB Communications Device Class (CDC) definitions
 *
 */

/* Line Coding Structure from CDC spec 6.2.13 */
struct usb_cdc_line_coding {
	__le32	dwDTERate;
	__u8	bCharFormat;
#define USB_CDC_1_STOP_BITS			0
#define USB_CDC_1_5_STOP_BITS			1
#define USB_CDC_2_STOP_BITS			2

	__u8	bParityType;
#define USB_CDC_NO_PARITY			0
#define USB_CDC_ODD_PARITY			1
#define USB_CDC_EVEN_PARITY			2
#define USB_CDC_MARK_PARITY			3
#define USB_CDC_SPACE_PARITY			4

	__u8	bDataBits;
} __attribute__ ((packed));

