/*
 * usb/gadget/config.c --config descriptors
 */

#include <sprd_common.h>
#include <errno.h>
#include <lk/list.h>
#include <string.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

int sprd_usb_descriptor_fillbuf(void *buffer, unsigned buflen,
		const struct usb_descriptor_header **src)
{
	u8 *dest = buffer;

	if (!src)
		return -EINVAL;

	for (; *src != NULL; ++src) {
		unsigned length = (*src)->bLength;

		if (length > buflen)
			return -EINVAL;
		memcpy(dest, *src, length);
		buflen -= length;
		dest += length;
	}
	return dest - (u8 *)buffer;
}

int sprd_usb_gadget_config_buf(const struct usb_config_descriptor *config,void *buffer,
		unsigned length, const struct usb_descriptor_header **desc)
{
	struct usb_config_descriptor *cfg_p = buffer;
	int len;

	/* config descriptor first */
	if (length < SPRD_USB_DT_CONFIG_SIZE || !desc)
		return -EINVAL;
	*cfg_p = *config;

	len = sprd_usb_descriptor_fillbuf(SPRD_USB_DT_CONFIG_SIZE + (u8 *)buffer,
			length - SPRD_USB_DT_CONFIG_SIZE, desc);
	if (len < 0)
		return len;
	len += SPRD_USB_DT_CONFIG_SIZE;
	if (len > 0xffff)
		return -EINVAL;

	cfg_p->bLength = SPRD_USB_DT_CONFIG_SIZE;
	cfg_p->bDescriptorType = SPRD_USB_DT_CONFIG;
	cfg_p->wTotalLength = cpu_to_le16(len);
	cfg_p->bmAttributes |= SPRD_USB_CONFIG_ATT_ONE;
	return len;
}

struct usb_descriptor_header **__init
sprd_usb_copy_descriptors(struct usb_descriptor_header **source)
{
	struct usb_descriptor_header **temp;
	void *memory;
	unsigned bytes, n_desc;
	struct usb_descriptor_header **usb_ret;

	/* count usb descriptors and their sizes */
	for (bytes = 0, n_desc = 0, temp = source; *temp; temp++, n_desc++)
		bytes += (*temp)->bLength;
	bytes += (n_desc + 1) * sizeof(*temp);

	memory = kmalloc(bytes, GFP_KERNEL);
	if (!memory)
		return NULL;

	temp = memory;
	usb_ret = memory;
	memory += (n_desc + 1) * sizeof(*temp);
	while (*source) {
		memcpy(memory, *source, (*source)->bLength);
		*temp = memory;
		temp++;
		memory += (*source)->bLength;
		source++;
	}
	*temp = NULL;

	return usb_ret;
}

struct usb_endpoint_descriptor *__init
sprd_usb_find_endpoint(
	struct usb_descriptor_header **source,
	struct usb_descriptor_header **copy,
	struct usb_endpoint_descriptor *match
)
{
	while (*source) {
		if (*source == (void *) match)
			return (void *)*copy;
		source++;
		copy++;
	}
	return NULL;
}

