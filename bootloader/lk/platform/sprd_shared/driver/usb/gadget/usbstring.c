#include <sprd_common.h>
#include <errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/kernel.h>
#include <string.h>

#include <sprd_unaligned.h>


static int utf8_to_utf16le(const char *str, __le16 *cp, unsigned length)
{
	int	utf_count = 0;
	u8	character;
	u16	uchar;

	while (length != 0 && (character = (u8) *str++) != 0) {
		if ((character & 0x80)) {
			if ((character & 0xe0) == 0xc0) {
				uchar = (character & 0x1f) << 6;

				character = (u8) *str++;
				if ((character & 0xc0) != 0x80)
					goto fail;
				character &= 0x3f;
				uchar |= character;
			} else if ((character & 0xf0) == 0xe0) {
				uchar = (character & 0x0f) << 12;

				character = (u8) *str++;
				if ((character & 0xc0) != 0x80)
					goto fail;
				character &= 0x3f;
				uchar |= character << 6;

				character = (u8) *str++;
				if ((character & 0xc0) != 0x80)
					goto fail;
				character &= 0x3f;
				uchar |= character;

				if (0xd800 <= uchar && uchar <= 0xdfff)
					goto fail;
			} else
				goto fail;
		} else
			uchar = character;
		put_unaligned_le16(uchar, cp++);
		utf_count++;
		length--;
	}
	return utf_count;
fail:
	return -1;
}

int
sprd_usb_gadget_get_string(struct usb_gadget_strings *g_table, int id, u8 *buffer)
{
	struct usb_string	*str;
	int			length;

	/* descriptor 0 has the language id */
	if (id == 0) {
		buffer[0] = 4;
		buffer[1] = USB_DT_STRING;
		buffer[2] = (u8) g_table->language;
		buffer[3] = (u8) (g_table->language >> 8);
		return 4;
	}
	for (str = g_table->strings; str && str->s; str++)
		if (str->id == id)
			break;

	/* unrecognized: stall. */
	if (!str || !str->s)
		return -EINVAL;

	/* string descriptors have length, tag, then UTF16-LE text */
	length = min((size_t) 126, strlen(str->s));
	memset(buffer + 2, 0, 2 * length);	/* zero all the bytes */
	length = utf8_to_utf16le(str->s, (__le16 *)&buffer[2], length);
	if (length < 0)
		return -EINVAL;
	buffer[0] = (length + 1) * 2;
	buffer[1] = USB_DT_STRING;
	return buffer[0];
}
