/*
 * Copyright (C) 2006-2008 David Brownell
 */
#undef DEBUG
#include <sprd_bitops.h>

#define USB_BUFSIZ	4096

static struct usb_composite_driver *composite;

static struct usb_descriptor_header**
next_ep_desc(struct usb_descriptor_header **t)
{
	for (; *t; t++) {
		if ((*t)->bDescriptorType == USB_DT_ENDPOINT)
			return t;
	}
	return NULL;
}

#define for_each_ep_desc(start, ep_desc) \
	for (ep_desc = next_ep_desc(start); \
	      ep_desc; ep_desc = next_ep_desc(ep_desc+1))

int usb_function_activate(struct usb_function *function)
{
	int				ret = 0;
	struct usb_composite_dev	*cdev = function->config->cdev;

	if (cdev->deactivations == 0)
		ret = -EINVAL;
	else {
		cdev->deactivations--;
		if (cdev->deactivations == 0)
			ret = usb_gadget_connect(cdev->gadget);
	}

	return ret;
}

int config_ep_by_speed(struct usb_gadget *g,
			struct usb_function *f,
			struct usb_ep *_ep)
{
	struct usb_descriptor_header **d_spd; /* cursor for speed desc */
	int need_comp_desc = 0;
	struct usb_ss_ep_comp_descriptor *pcomp_desc = NULL;
	struct usb_descriptor_header **ppspeed_desc = NULL;
	struct usb_endpoint_descriptor *pchosen_desc = NULL;
	struct usb_composite_dev	*cdev = get_gadget_data(g);

	if (!_ep || !g || !f)
		return -EIO;

	/* select desired speed */
	switch (g->speed) {
	case USB_SPEED_SUPER:
		if (gadget_is_superspeed(g)) {
			ppspeed_desc = f->ss_descriptors;
			need_comp_desc = 1;
			break;
		}
		/* else: Fall trough */
	case USB_SPEED_HIGH:
		if (gadget_is_dualspeed(g)) {
			ppspeed_desc = f->hs_descriptors;
			break;
		}
		/* else: fall through */
	default:
		ppspeed_desc = f->descriptors;
	}
	/* find descriptors */
	for_each_ep_desc(ppspeed_desc, d_spd) {
		pchosen_desc = (struct usb_endpoint_descriptor *)*d_spd;
		if (pchosen_desc->bEndpointAddress == _ep->address)
			goto ep_found;
	}
	return -EIO;

ep_found:
	/* commit results */
	_ep->maxpacket = usb_endpoint_maxp(pchosen_desc);
	_ep->mult = 0;
	_ep->maxburst = 0;
	_ep->comp_desc = NULL;
	_ep->desc = pchosen_desc;

	if (!need_comp_desc)
		return 0;

	pcomp_desc = (struct usb_ss_ep_comp_descriptor *)*(++d_spd);
	if (!pcomp_desc ||
	    (USB_DT_SS_ENDPOINT_COMP != pcomp_desc->bDescriptorType))
		return -EIO;
	_ep->comp_desc = pcomp_desc;
	if (USB_SPEED_SUPER == g->speed) {
		switch (usb_endpoint_type(_ep->desc)) {
		case USB_ENDPOINT_XFER_ISOC:
			/* mult: bits 1:0 of bmAttributes */
			_ep->mult = pcomp_desc->bmAttributes & 0x3;
		case USB_ENDPOINT_XFER_BULK:
		case USB_ENDPOINT_XFER_INT:
			_ep->maxburst = pcomp_desc->bMaxBurst + 1;
			break;
		default:
			if (pcomp_desc->bMaxBurst != 0)
				ERROR(cdev, "ep0 bMaxBurst must be 0\n");
			_ep->maxburst = 1;
			break;
		}
	}
	return 0;
}


int usb_add_function(struct usb_configuration *config,
		struct usb_function *function)
{
	int	ret = -EINVAL;

	debug("adding '%s'/%p to config '%s'/%p\n",
			function->name, function,
			config->label, config);

	if (!function->set_alt || !function->disable)
		goto done;

	function->config = config;
	list_add_tail(&config->functions, &function->list);

	if (function->bind) {
		ret = function->bind(config, function);
		if (ret < 0) {
			list_delete(&function->list);
			function->config = NULL;
		}
	} else
		ret = 0;

	if (!config->fullspeed && function->descriptors)
		config->fullspeed = 1;
	if (!config->highspeed && function->hs_descriptors)
		config->highspeed = 1;

done:
	if (ret)
		debug("adding '%s'/%p --> %d\n",
				function->name, function, ret);
	return ret;
}

int usb_function_deactivate(struct usb_function *function)
{
	int				ret = 0;
	struct usb_composite_dev	*cdev = function->config->cdev;

	if (cdev->deactivations == 0)
		ret = usb_gadget_disconnect(cdev->gadget);
	if (ret == 0)
		cdev->deactivations++;

	return ret;
}


static int config_buf(struct usb_configuration *config,
		enum usb_device_speed speed, void *buf, u8 type)
{
	struct usb_descriptor_header    **descriptors;
	struct usb_function		*f;
	int				ret;
	int				len = USB_BUFSIZ - SPRD_USB_DT_CONFIG_SIZE;
	void				*next = buf + SPRD_USB_DT_CONFIG_SIZE;
	struct usb_config_descriptor	*config_descriptor = buf;

	/* write the config descriptor */
	config_descriptor = buf;
	config_descriptor->bLength = SPRD_USB_DT_CONFIG_SIZE;
	config_descriptor->bDescriptorType = type;

	config_descriptor->bNumInterfaces = config->next_interface_id;
	config_descriptor->bConfigurationValue = config->bConfigurationValue;
	config_descriptor->iConfiguration = config->iConfiguration;
	config_descriptor->bmAttributes = SPRD_USB_CONFIG_ATT_ONE | config->bmAttributes;
	config_descriptor->bMaxPower = config->bMaxPower ? : 1;

	/* There may be e.g. OTG descriptors */
	if (config->descriptors) {
		ret = sprd_usb_descriptor_fillbuf(next, len,
				config->descriptors);
		if (ret < 0)
			return ret;
		len -= ret;
		next += ret;
	}

	/* add each function's descriptors */
	list_for_each_entry(f, &config->functions, list) {
		if (speed == USB_SPEED_HIGH)
			descriptors = f->hs_descriptors;
		else
			descriptors = f->descriptors;
		if (!descriptors)
			continue;
		ret = sprd_usb_descriptor_fillbuf(next, len,
			(const struct usb_descriptor_header **) descriptors);
		if (ret < 0)
			return ret;
		len -= ret;
		next += ret;
	}

	len = next - buf;
	config_descriptor->wTotalLength = cpu_to_le16(len);
	return len;
}

int sprd_usb_interface_id(struct usb_configuration *config,
		struct usb_function *function)
{
	unsigned char id = config->next_interface_id;

	if (id < MAX_CONFIG_INTERFACES) {
		config->interface[id] = function;
		config->next_interface_id = id + 1;
		return id;
	}
	return -ENODEV;
}


static int count_configs(struct usb_composite_dev *cdev, unsigned type)
{
	struct usb_configuration	*configuration;
	int				bhs = 0;
	unsigned			count = 0;
	struct usb_gadget		*pgadget = cdev->gadget;

	if (gadget_is_dualspeed(pgadget)) {
		if (pgadget->speed == USB_SPEED_HIGH)
			bhs = 1;
		if (type == USB_DT_DEVICE_QUALIFIER)
			bhs = !bhs;
	}
	list_for_each_entry(configuration, &cdev->configs, list) {
		/* ignore configs that won't work at this speed */
		if (bhs) {
			if (!configuration->highspeed)
				continue;
		} else {
			if (!configuration->fullspeed)
				continue;
		}
		count++;
	}
	return count;
}

static int config_desc(struct usb_composite_dev *cdev, unsigned w_value)
{
	struct usb_configuration	*configuration;
	int                             bhs = 0;
	u8				type = w_value >> 8;
	struct usb_gadget		*gadget = cdev->gadget;
	enum usb_device_speed		espeed = USB_SPEED_UNKNOWN;

	if (gadget_is_dualspeed(gadget)) {
		if (gadget->speed == USB_SPEED_HIGH)
			bhs = 1;
		if (type == USB_DT_OTHER_SPEED_CONFIG)
			bhs = !bhs;
		if (bhs)
			espeed = USB_SPEED_HIGH;
	}

	w_value &= 0xff;
	list_for_each_entry(configuration, &cdev->configs, list) {
		if (espeed == USB_SPEED_HIGH) {
			if (!configuration->highspeed)
				continue;
		} else {
			if (!configuration->fullspeed)
				continue;
		}
		if (w_value == 0)
			return config_buf(configuration, espeed, cdev->req->buf, type);
		w_value--;
	}
	return -EINVAL;
}

static void reset_config(struct usb_composite_dev *cdev)
{
	struct usb_function		*fun;

	debug("%s\n", __func__);

	list_for_each_entry(fun, &cdev->config->functions, list) {
		if (fun->disable)
			fun->disable(fun);

		bitmap_zero(fun->endpoints, 32);
	}
	cdev->config = NULL;
}

static void device_qual(struct usb_composite_dev *cdev)
{
	struct usb_qualifier_descriptor	*qualifier = cdev->req->buf;

	qualifier->bLength = sizeof(*qualifier);
	qualifier->bDescriptorType = USB_DT_DEVICE_QUALIFIER;
	/* POLICY: same bcdUSB and device type info at both speeds */
	qualifier->bcdUSB = cdev->desc.bcdUSB;
	qualifier->bDeviceClass = cdev->desc.bDeviceClass;
	qualifier->bDeviceSubClass = cdev->desc.bDeviceSubClass;
	qualifier->bDeviceProtocol = cdev->desc.bDeviceProtocol;
	/* ASSUME same EP0 fifo size at both speeds */
	qualifier->bMaxPacketSize0 = cdev->gadget->ep0->maxpacket;
	qualifier->bNumConfigurations = count_configs(cdev, USB_DT_DEVICE_QUALIFIER);
	qualifier->bRESERVED = 0;
}


static int set_config(struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *ctrl, unsigned number)
{
	struct usb_function	*fun;
	int			tmp;
	int                     addr;
	struct usb_configuration *configuration = NULL;
	struct usb_endpoint_descriptor *endpoint;
	int			ret = -EINVAL;
	struct usb_descriptor_header **ppdescriptors;
	struct usb_gadget	*gadget = cdev->gadget;
	unsigned		power = gadget_is_otg(gadget) ? 8 : 100;

	if (cdev->config)
		reset_config(cdev);

	if (number) {
		list_for_each_entry(configuration, &cdev->configs, list) {
			if (configuration->bConfigurationValue == number) {
				ret = 0;
				break;
			}
		}
		if (ret < 0)
			goto done;
	} else
		ret = 0;

	debug("%s: %s speed config #%d: %s\n", __func__,
	     ({ char *speed;
		     switch (gadget->speed) {
			 case USB_SPEED_HIGH:
			     speed = "high";
			     break;
		     case USB_SPEED_LOW:
			     speed = "low";
			     break;
		     case USB_SPEED_FULL:
			     speed = "full";
			     break;
		     default:
			     speed = "?";
			     break;
		     };
		     speed;
	     }), number, configuration ? configuration->label : "unconfigured");

	if (!configuration)
		goto done;

	cdev->config = configuration;

	/* Initialize all interfaces by setting them to altsetting zero. */
	for (tmp = 0; tmp < MAX_CONFIG_INTERFACES; tmp++) {
		fun = configuration->interface[tmp];
		if (!fun)
			break;

		if (gadget->speed == USB_SPEED_HIGH)
			ppdescriptors = fun->hs_descriptors;
		else
			ppdescriptors = fun->descriptors;

		for (; *ppdescriptors; ++ppdescriptors) {
			if ((*ppdescriptors)->bDescriptorType != USB_DT_ENDPOINT)
				continue;

			endpoint = (struct usb_endpoint_descriptor *)*ppdescriptors;
			addr = ((endpoint->bEndpointAddress & 0x80) >> 3)
			     |	(endpoint->bEndpointAddress & 0x0f);
			__set_bit(addr, fun->endpoints);
		}

		ret = fun->set_alt(fun, tmp, 0);
		if (ret < 0) {
			debug("interface %d (%s/%p) alt 0 --> %d\n",
					tmp, fun->name, fun, ret);

			reset_config(cdev);
			goto done;
		}
	}

	/* when we return, be sure our power usage is valid */
	power = configuration->bMaxPower ? (2 * configuration->bMaxPower) : 2;
done:
	usb_gadget_vbus_draw(gadget, power);
	return ret;
}

static int lookup_string(
	struct usb_gadget_strings	**sp,
	void				*buf,
	u16				language,
	int				id
)
{
	int				value;
	struct usb_gadget_strings	*s;

	while (*sp) {
		s = *sp++;
		if (s->language != language)
			continue;
		value = sprd_usb_gadget_get_string(s, id, buf);
		if (value > 0)
			return value;
	}
	return -EINVAL;
}

/*
 * We support strings in multiple languages ... string descriptor zero
 * says which languages are supported.	The typical case will be that
 * only one language (probably English) is used, with I18N handled on
 * the host side.
 */

static void collect_langs(struct usb_gadget_strings **sp, __le16 *buf)
{
	const struct usb_gadget_strings	*s;
	u16				language;
	__le16				*tmp;

	while (*sp) {
		s = *sp;
		language = cpu_to_le16(s->language);
		for (tmp = buf; *tmp && tmp < &buf[126]; tmp++) {
			if (*tmp == language)
				goto repeat;
		}
		*tmp++ = language;
repeat:
		sp++;
	}
}

int usb_add_config(struct usb_composite_dev *cdev,
		struct usb_configuration *config)
{
	unsigned int			j;
	struct usb_function		*fun;
	struct usb_configuration	*configuration;
	int				ret = -EINVAL;

	debug("%s: adding config #%u '%s'/%p\n", __func__,
			config->bConfigurationValue,
			config->label, config);

	if (!config->bind || !config->bConfigurationValue)
		goto done;

	/* Prevent duplicate configuration identifiers */
	list_for_each_entry(configuration, &cdev->configs, list) {
		if (configuration->bConfigurationValue == config->bConfigurationValue) {
			ret = -EBUSY;
			goto done;
		}
	}

	config->cdev = cdev;
	list_add_tail(&cdev->configs, &config->list);

	INIT_LIST_HEAD(&config->functions);
	config->next_interface_id = 0;

	ret = config->bind(config);
	if (ret < 0) {
		list_delete(&config->list);
		config->cdev = NULL;
	} else {
		debug("cfg %d/%p speeds:%s%s\n",
			config->bConfigurationValue, config,
			config->highspeed ? " high" : "",
			config->fullspeed ? (gadget_is_dualspeed(cdev->gadget)
					? " full" : " full/low")
				: "");

		for (j = 0; j < MAX_CONFIG_INTERFACES; j++) {
			fun = config->interface[j];
			if (!fun)
				continue;
			debug("%s: interface %d = %s/%p\n",
			      __func__, j, fun->name, fun);
		}
	}

	usb_ep_autoconfig_reset(cdev->gadget);

done:
	if (ret)
		debug("added config '%s'/%u --> %d\n", config->label,
				config->bConfigurationValue, ret);
	return ret;
}

/**
 * allocate an unused string ID
 *
 * All string identifier should be allocated using this,
 * @usb_string_ids_tab() or @usb_string_ids_n() routine, to ensure
 * that for example different functions don't wrongly assign different
 * meanings to the same identifier.
 */
int sprd_usb_string_id(struct usb_composite_dev *cdev)
{
	if (cdev->next_string_id < 254) {
		/*
		 * string id 0 is reserved by USB spec for list of
		 * supported languages
		 * 255 reserved as well? -- mina86
		 */
		cdev->next_string_id++;
		return cdev->next_string_id;
	}
	return -ENODEV;
}

static int get_string(struct usb_composite_dev *cdev,
		void *buf, u16 language, int id)
{
	struct usb_function		*fun;
	struct usb_configuration	*configuration;
	int				length;
	struct usb_gadget_strings	**spstr;
	struct usb_string_descriptor	*s = buf;

	if (id == 0) {
		memset(s, 0, 256);
		s->bDescriptorType = USB_DT_STRING;

		spstr = composite->strings;
		if (spstr)
			collect_langs(spstr, s->wData);

		list_for_each_entry(configuration, &cdev->configs, list) {
			spstr = configuration->strings;
			if (spstr)
				collect_langs(spstr, s->wData);

			list_for_each_entry(fun, &configuration->functions, list) {
				spstr = fun->strings;
				if (spstr)
					collect_langs(spstr, s->wData);
			}
		}

		for (length = 0; length <= 126 && s->wData[length]; length++)
			continue;
		if (!length)
			return -EINVAL;

		s->bLength = 2 * (length + 1);
		return s->bLength;
	}

	if (composite->strings) {
		length = lookup_string(composite->strings, buf, language, id);
		if (length > 0)
			return length;
	}
	list_for_each_entry(configuration, &cdev->configs, list) {
		if (configuration->strings) {
			length = lookup_string(configuration->strings, buf, language, id);
			if (length > 0)
				return length;
		}
		list_for_each_entry(fun, &configuration->functions, list) {
			if (!fun->strings)
				continue;
			length = lookup_string(fun->strings, buf, language, id);
			if (length > 0)
				return length;
		}
	}
	return -EINVAL;
}


/**
int usb_string_ids_tab(struct usb_composite_dev *cdev, struct usb_string *str)
{
	u8 next = cdev->next_string_id;

	for (; str->s; ++str) {
		if (next >= 254)
			return -ENODEV;
		str->id = ++next;
	}

	cdev->next_string_id = next;

	return 0;
}
*/

/*
int usb_string_ids_n(struct usb_composite_dev *c, unsigned n)
{
	u8 next = c->next_string_id;

	if (n > 254 || next + n > 254)
		return -ENODEV;

	c->next_string_id += n;
	return next + 1;
}
*/

static void composite_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->actual != req->length || req->status)
		debug("%s: setup complete --> %d, %d/%d\n", __func__,
				req->status, req->actual, req->length);
}

static void composite_disconnect(struct usb_gadget *gadget)
{
	struct usb_composite_dev	*pcdev = get_gadget_data(gadget);

	if (pcdev->config)
		reset_config(pcdev);
	if (composite->disconnect)
		composite->disconnect(pcdev);
}


static int composite_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct usb_configuration	*c;
	u8				endp;
	int				standard;
	struct usb_function		*f = NULL;
	u16				w_value = le16_to_cpu(ctrl->wValue);
	u16				w_length = le16_to_cpu(ctrl->wLength);
	u16				w_index = le16_to_cpu(ctrl->wIndex);
	u8				intf = w_index & 0xFF;
	struct usb_composite_dev	*pcdev = get_gadget_data(gadget);
		struct usb_request		*request = pcdev->req;
	int				ret = -EOPNOTSUPP;

    request->length = USB_BUFSIZ;
	request->zero = 0;
	request->complete = composite_setup_complete;
	gadget->ep0->driver_data = pcdev;
	standard = (ctrl->bRequestType & USB_TYPE_MASK)
						== USB_TYPE_STANDARD;
	if (!standard)
		goto unknown;

	switch (ctrl->bRequest) {

	/* we handle all standard USB descriptors */
	case USB_REQ_GET_DESCRIPTOR:
		if (USB_DIR_IN != ctrl->bRequestType)
			goto unknown;
		switch (w_value >> 8) {

		case SPRD_USB_DT_DEVICE:
			pcdev->desc.bNumConfigurations =
				count_configs(pcdev, SPRD_USB_DT_DEVICE);
			pcdev->desc.bMaxPacketSize0 =
				pcdev->gadget->ep0->maxpacket;
			ret = min(w_length, (u16) sizeof pcdev->desc);
			memcpy(request->buf, &pcdev->desc, ret);
			break;
		case USB_DT_DEVICE_QUALIFIER:
			if (!gadget_is_dualspeed(gadget))
				break;
			device_qual(pcdev);
			ret = min_t(int, w_length,
				      sizeof(struct usb_qualifier_descriptor));
			break;
		case USB_DT_OTHER_SPEED_CONFIG:
			if (!gadget_is_dualspeed(gadget))
				break;

		case SPRD_USB_DT_CONFIG:
			ret = config_desc(pcdev, w_value);
			if (ret >= 0)
				ret = min(w_length, (u16) ret);
			break;
		case USB_DT_STRING:
			ret = get_string(pcdev, request->buf,
					w_index, w_value & 0xff);
			if (ret >= 0)
				ret = min(w_length, (u16) ret);
			break;
		case USB_DT_BOS:
			break;
		default:
			goto unknown;
		}
		break;

	/* any number of configs can work */
	case USB_REQ_SET_CONFIGURATION:
		if (0 != ctrl->bRequestType)
			goto unknown;
		if (gadget_is_otg(gadget)) {
			if (gadget->a_hnp_support)
				debug("HNP available\n");
			else if (gadget->a_alt_hnp_support)
				debug("HNP on another port\n");
			else
				debug("HNP inactive\n");
		}

		ret = set_config(pcdev, ctrl, w_value);
		break;
	case USB_REQ_GET_CONFIGURATION:
		if (USB_DIR_IN != ctrl->bRequestType)
			goto unknown;
		if (pcdev->config)
			*(u8 *)request->buf = pcdev->config->bConfigurationValue;
		else
			*(u8 *)request->buf = 0;
		ret = min(w_length, (u16) 1);
		break;

	case USB_REQ_SET_INTERFACE:
		if (USB_RECIP_INTERFACE != ctrl->bRequestType)
			goto unknown;
		if (!pcdev->config || w_index >= MAX_CONFIG_INTERFACES)
			break;
		f = pcdev->config->interface[intf];
		if (!f)
			break;
		if (w_value && !f->set_alt)
			break;
		ret = f->set_alt(f, w_index, w_value);
		break;
	case USB_REQ_GET_INTERFACE:
		if ((USB_DIR_IN|USB_RECIP_INTERFACE) != ctrl->bRequestType)
			goto unknown;
		if (!pcdev->config || w_index >= MAX_CONFIG_INTERFACES)
			break;
		f = pcdev->config->interface[intf];
		if (!f)
			break;
		/* lots of interfaces only need altsetting zero... */
		ret = f->get_alt ? f->get_alt(f, w_index) : 0;
		if (ret < 0)
			break;
		*((u8 *)request->buf) = ret;
		ret = min(w_length, (u16) 1);
		break;
	default:
unknown:
		debug("non-core control req0x%02x.0x%02x v0x%04x i0x%04x l0x%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);

		switch (ctrl->bRequestType & USB_RECIP_MASK) {
		case USB_RECIP_INTERFACE:
			f = pcdev->config->interface[intf];
			break;

		case USB_RECIP_ENDPOINT:
			endp = ((w_index & 0x80) >> 3) | (w_index & 0x0f);
			list_for_each_entry(f, &pcdev->config->functions, list) {
				if (test_bit(endp, f->endpoints))
					break;
			}
			if (&f->list == &pcdev->config->functions)
				f = NULL;
			break;

		case USB_RECIP_DEVICE:
			debug("cdev->config->next_interface_id: %d intf: %d\n",
			       pcdev->config->next_interface_id, intf);
			if (pcdev->config->next_interface_id == 1)
				f = pcdev->config->interface[intf];
			break;
		}

		if (f && f->setup)
			ret = f->setup(f, ctrl);
		else {
			c = pcdev->config;
			if (c && c->setup)
				ret = c->setup(c, ctrl);
		}

		goto done;
	}

	/* respond with data transfer before status phase? */
	if (ret >= 0) {
		request->length = ret;
		request->zero = ret < w_length;
		ret = usb_ep_queue(gadget->ep0, request, GFP_KERNEL);
		if (ret < 0) {
			debug("ep_queue --> %d\n", ret);
			request->status = 0;
			composite_setup_complete(gadget->ep0, request);
		}
	}

done:
	/* device either stalls (value < 0) or reports success */
	return ret;
}


static void composite_unbind(struct usb_gadget *gadget)
{
	struct usb_function		*fun;
	struct usb_composite_dev	*pcdev = get_gadget_data(gadget);
	struct usb_configuration	*configuration;

	BUG_ON(pcdev->config);

	while (!list_is_empty(&pcdev->configs)) {
		configuration = list_first_entry(&pcdev->configs,
				struct usb_configuration, list);
		while (!list_is_empty(&configuration->functions)) {
			fun = list_first_entry(&configuration->functions,
					struct usb_function, list);
			list_delete(&fun->list);
			if (fun->unbind) {
				debug("unbind function '%s'/%p\n",
						fun->name, fun);
				fun->unbind(configuration, fun);
			}
		}
		list_delete(&configuration->list);
		if (configuration->unbind) {
			debug("unbind config '%s'/%p\n", configuration->label, configuration);
			configuration->unbind(configuration);
		}
	}
	if (composite->unbind)
		composite->unbind(pcdev);

	if (pcdev->req) {
		kfree(pcdev->req->buf);
		usb_ep_free_request(gadget->ep0, pcdev->req);
	}
	kfree(pcdev);
	set_gadget_data(gadget, NULL);

	composite = NULL;
}

static void composite_resume(struct usb_gadget *gadget)
{
	struct usb_function		*fun;
	struct usb_composite_dev	*pcdev = get_gadget_data(gadget);

	debug("%s: resume\n", __func__);
	if (composite->resume)
		composite->resume(pcdev);
	if (pcdev->config) {
		list_for_each_entry(fun, &pcdev->config->functions, list) {
			if (fun->resume)
				fun->resume(fun);
		}
	}

	pcdev->suspended = 0;
}

static int composite_bind(struct usb_gadget *gadget)
{
	int				ret = -ENOMEM;
	struct usb_composite_dev	*pcdev;

	pcdev = calloc(sizeof *pcdev, 1);
	if (!pcdev)
		return ret;

	pcdev->gadget = gadget;
	set_gadget_data(gadget, pcdev);
	INIT_LIST_HEAD(&pcdev->configs);

	/* preallocate control response and buffer */
	pcdev->req = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);
	if (!pcdev->req)
		goto fail;
	pcdev->req->buf = memalign(64, USB_BUFSIZ);
	if (!pcdev->req->buf)
		goto fail;
	pcdev->req->complete = composite_setup_complete;
	gadget->ep0->driver_data = pcdev;

	pcdev->bufsiz = USB_BUFSIZ;
	pcdev->driver = composite;

	usb_gadget_set_selfpowered(gadget);
	usb_ep_autoconfig_reset(pcdev->gadget);

	ret = composite->bind(pcdev);
	if (ret < 0)
		goto fail;

	memcpy(&pcdev->desc, composite->dev,
	       sizeof(struct usb_device_descriptor));
	pcdev->desc.bMaxPacketSize0 = gadget->ep0->maxpacket;

	debug("%s: ready\n", composite->name);
	return 0;

fail:
	composite_unbind(gadget);
	return ret;
}


static void composite_suspend(struct usb_gadget *gadget)
{
	struct usb_function		*fun;
	struct usb_composite_dev	*pcdev = get_gadget_data(gadget);

	debug("%s: suspend\n", __func__);
	if (pcdev->config) {
		list_for_each_entry(fun, &pcdev->config->functions, list) {
			if (fun->suspend)
				fun->suspend(fun);
		}
	}
	if (composite->suspend)
		composite->suspend(pcdev);

	pcdev->suspended = 1;
}

static struct usb_gadget_driver composite_driver = {
	.speed		= USB_SPEED_HIGH,

	.bind		= composite_bind,
	.unbind         = composite_unbind,

	.setup		= composite_setup,
	.reset      = composite_disconnect,
	.disconnect	= composite_disconnect,

	.suspend	= composite_suspend,
	.resume		= composite_resume,
};

/**
 * usb_composite_unregister() - unregister a composite driver
 * @driver: the driver to unregister
 *
 * This function is used to unregister drivers using the composite
 * driver framework.
 */
void usb_composite_unregister(struct usb_composite_driver *driver)
{
	if (composite != driver)
		return;
	usb_gadget_unregister_driver(&composite_driver);
	composite = NULL;
}

int usb_composite_register(struct usb_composite_driver *driver)
{
	if (composite || !driver || !driver->dev || !driver->bind)
		return -EINVAL;

	if (!driver->name)
		driver->name = "composite";
	composite = driver;

	return usb_gadget_register_driver(&composite_driver);
}


