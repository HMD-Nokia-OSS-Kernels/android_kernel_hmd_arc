/*
 * sprd serial.c
 */

#include <sprd_common.h>
#include <lk/list.h>
#include <linux/usb/usb_uboot.h>
#include <lk/debug.h>

#include "u_serial.h"
#ifndef pr_warning
#define pr_warning(fmt, args...) dprintf(INFO,fmt, ##args)
#endif
#define GFP_ATOMIC ((gfp_t) 0)

#define PREFIX	"ttyGS"

#define QUEUE_SIZE		1
#define WRITE_BUF_SIZE		8192

#ifdef VERBOSE_DEBUG
#define pr_vdebug(fmt, arg...) \
	pr_debug(fmt, ##arg)
#else
#define pr_vdebug(fmt, arg...) \
	({ if (0) pr_debug(fmt, ##arg); })
#endif

#define GS_CLOSE_TIMEOUT		15		/* seconds */

/* increase N_PORTS if you need more */
#define N_PORTS		4
/* gs circular buffer */
struct gs_buf {
	unsigned		buf_size;
	char			*buf_buf;
	char			*buf_get;
	char			*buf_put;
};

struct gs_port {
	spinlock_t		port_lock;
	struct gserial		*port_usb;
	unsigned		open_count;
	bool			openclose;
	u8			port_num;

	wait_queue_head_t	close_wait;

	struct list_head	read_pool;
	int read_started;
	int read_allocated;
	struct list_head	read_queue;
	unsigned		n_read;

	struct list_head	write_pool;
	int write_started;
	int write_allocated;
	struct gs_buf		port_write_buf;
	wait_queue_head_t	drain_wait;	/* wait while writes drain */

	struct usb_cdc_line_coding port_line_coding;	/* 8-N-1 etc */
};

static struct portmaster {
	struct mutex	lock;			/* protect open/close */
	struct gs_port	*port;
} ports[N_PORTS];
static unsigned	n_ports;

/*
 * gs_buf_free --free the buffer and all associated memory.
 */
static void gs_buf_free(struct gs_buf *gb)
{
	kfree(gb->buf_buf);
	gb->buf_buf = NULL;
}

static int gs_buf_alloc(struct gs_buf *gb, unsigned size)
{
	gb->buf_buf = memalign(64, size);
	if (gb->buf_buf == NULL)
		return -ENOMEM;

	gb->buf_size = size;
	gb->buf_put = gb->buf_buf;
	gb->buf_get = gb->buf_buf;

	return 0;
}


static unsigned gs_buf_space_avail(struct gs_buf *gb)
{
	return (gb->buf_size + gb->buf_get - gb->buf_put - 1) % gb->buf_size;
}


static unsigned gs_buf_data_avail(struct gs_buf *gb)
{
	return (gb->buf_size + gb->buf_put - gb->buf_get) % gb->buf_size;
}


static unsigned
gs_buf_get(struct gs_buf *gb, char *buf, unsigned count)
{
	unsigned length;

	length = gs_buf_data_avail(gb);
	if (count > length)
		count = length;

	if (count == 0)
		return 0;

	length = gb->buf_buf + gb->buf_size - gb->buf_get;
	if (count > length) {
		memcpy(buf, gb->buf_get, length);
		memcpy(buf+length, gb->buf_buf, count - length);
		gb->buf_get = gb->buf_buf + count - length;
	} else {
		memcpy(buf, gb->buf_get, count);
		if (count < length)
			gb->buf_get += count;
		else /* count == length */
			gb->buf_get = gb->buf_buf;
	}

	return count;
}


struct usb_request *
gs_alloc_req(struct usb_ep *ep, unsigned len, gfp_t kmalloc_flags)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, kmalloc_flags);

	if (req != NULL) {
		req->length = len;
		req->buf = memalign(64, len);
		if (req->buf == NULL) {
			usb_ep_free_request(ep, req);
			return NULL;
		}
	}

	return req;
}

/*
 * gs_buf_clear --clear out all data in the circular buffer.
 * 		  equivalent to a get of all data available.
 */
static void gs_buf_clear(struct gs_buf *gb)
{
	gb->buf_get = gb->buf_put;
}


static unsigned
gs_buf_put(struct gs_buf *gb, const char *buf, unsigned count)
{
	unsigned len;

	len  = gs_buf_space_avail(gb);
	if (count > len)
		count = len;

	if (count == 0)
		return 0;

	len = gb->buf_buf + gb->buf_size - gb->buf_put;
	if (count > len) {
		memcpy(gb->buf_put, buf, len);
		memcpy(gb->buf_buf, buf+len, count - len);
		gb->buf_put = gb->buf_buf + count - len;
	} else {
		memcpy(gb->buf_put, buf, count);
		if (count < len)
			gb->buf_put += count;
		else /* count == len */
			gb->buf_put = gb->buf_buf;
	}

	return count;
}




static unsigned gs_send_packet(struct gs_port *port, char *packet, unsigned size)
{
	unsigned len;

	len = gs_buf_data_avail(&port->port_write_buf);
	if (len < size)
		size = len;
	if (size != 0)
		size = gs_buf_get(&port->port_write_buf, packet, size);
	return size;
}


void gs_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);

	usb_ep_free_request(ep, req);
}

static unsigned gs_start_rx(struct gs_port *port)
{
	struct list_head	*pool = &port->read_pool;
	struct usb_ep		*out = NULL;
	if (!port->port_usb){
		return 0;
	}
    out = port->port_usb->out;
	while (!list_is_empty(pool)) {
		struct usb_request	*req;
		int			ret;

		if (port->read_started >= QUEUE_SIZE)
			break;

		req = list_entry(pool->next, struct usb_request, list);
		list_delete(&req->list);
		req->length = out->maxpacket;

		/* drop lock while we call out; the controller driver
		 * may need to call us back (e.g. for disconnect)
		 */
		spin_unlock(&port->port_lock);
		ret = usb_ep_queue(out, req, GFP_ATOMIC);
		spin_lock(&port->port_lock);

		if (ret) {
			debugf("%s: %s %s err %d\n",
					__func__, "queue", out->name, ret);
			list_add(&req->list, pool);
			break;
		}
		port->read_started++;

		/* abort immediately after disconnect */
		if (!port->port_usb){
			break;
		}
	}
	return port->read_started;
}



int vcom_port = 0;
int usb_write_done;
int usb_read_done;
int usb_trans_status;
static int gs_start_io(struct gs_port *port);

int gs_open(void)
{
	int status = 0;
	struct gs_port	*port = ports[vcom_port].port;
	if (port->port_write_buf.buf_buf == NULL) {
		status = gs_buf_alloc(&port->port_write_buf, WRITE_BUF_SIZE);
		if(status){
			errorf("gs_open: gs_buf_alloc failed\n");
			return status;
		}
	}
	/* if connected, start the I/O stream */
	if (port->port_usb) {
		struct gserial	*gser = port->port_usb;

		debugf("gs_open: start ttyGS%d\n", get_unaligned(&port->port_num));
		gs_start_io(port);

		if (gser->connect)
			gser->connect(gser);
	}

	debugf("gs_open: ttyGS%d\n", get_unaligned(&port->port_num));

	status = 0;
	return status;
}

static int gs_start_tx(struct gs_port *port)
{
	struct list_head	*pool = &port->write_pool;
	struct usb_ep		*ep = NULL;
	int			ret = 0;
	bool			do_tty_wake = false;

	if (NULL == port->port_usb){
		return ret;
	}
	ep = port->port_usb->in;

	while (!list_is_empty(pool)) {
		struct usb_request	*prequest;
		int			length;

		if (port->write_started >= QUEUE_SIZE)
			break;

		prequest = list_entry(pool->next, struct usb_request, list);
		length = gs_send_packet(port, prequest->buf, ep->maxpacket);
		if (length == 0) {
			wake_up_interruptible(&port->drain_wait);
			break;
		}
		do_tty_wake = true;

		prequest->length = length;
		list_delete(&prequest->list);
		prequest->zero = (gs_buf_data_avail(&port->port_write_buf) == 0);

		debugf("%d: tx length=%d, 0x%02x 0x%02x 0x%02x ...\n",
				port->port_num, length, *((u8 *)prequest->buf),
				*((u8 *)prequest->buf+1), *((u8 *)prequest->buf+2));

		spin_unlock(&port->port_lock);
		ret = usb_ep_queue(ep, prequest, GFP_ATOMIC);
		spin_lock(&port->port_lock);

		if (ret) {
			debugf("%s: %s %s err %d\n",
					__func__, "queue", ep->name, ret);
			list_add(&prequest->list, pool);
			break;
		}

		port->write_started++;

		if (!port->port_usb){
			break;
		}
	}
	return ret;
}


void gs_close(void)
{
	struct gs_port *port = ports[vcom_port].port;
	struct gserial	*gser;

	if (port->open_count != 1) {
		if (port->open_count == 0)
			;
		else
			--port->open_count;
		return;
	}

	debugf("gs_close: ttyGS%d ...\n", port->port_num);

	port->openclose = true;
	port->open_count = 0;

	gser = port->port_usb;
	if (gser && gser->disconnect)
		gser->disconnect(gser);

	if (gs_buf_data_avail(&port->port_write_buf) > 0 && gser) {
		gser = port->port_usb;
	}

	if (gser == NULL)
		gs_buf_free(&port->port_write_buf);
	else
		gs_buf_clear(&port->port_write_buf);

	//tty->driver_data = NULL;
	//port->port_tty = NULL;

	port->openclose = false;

	debugf("gs_close: ttyGS%d done!\n", port->port_num);

	//wake_up_interruptible(&port->close_wait);
}


//buf: the buf address where data put
//*count: read length wanted, when return store actually read count
//return: weather read really done
int gs_read(const unsigned char *buf, int *count)
{
	struct gs_port *port = ports[vcom_port].port;
	struct list_head *queue = &port->read_queue;
	bool disconnect = false;
	bool			do_push = false;

	if (!list_is_empty(queue)) {
		struct usb_request	*req;

		req = list_first_entry(queue, struct usb_request, list);
		switch (req->status) {
		case -ESHUTDOWN:
			disconnect = true;
			dprintf(ALWAYS, "%d: shutdown\n", port->port_num);
			break;

		default:
			/* presumably a transient fault */
			dprintf(INFO,PREFIX "%d: unexpected RX status %d\n",
					port->port_num, req->status);
			/* FALLTHROUGH */
		case 0:
			/* normal completion */
			break;
		}

		if (req->actual) {
			char		*packet = req->buf;
			unsigned	size = req->actual;
			unsigned	n;


			/* we may have pushed part of this packet already... */
			n = port->n_read;
			if (n) {
				packet += n;
				size -= n;
			}
			if(size)
				do_push = true;
			if(*count >= size){
				memcpy(buf, packet, size);
				*count = size;
			}else{
				memcpy(buf, packet, *count);
				port->n_read += *count;
				//break;
			}
			port->n_read = 0;
		}
recycle:
		list_move(&req->list, &port->read_pool);
		port->read_started--;
	}
	/* If we're still connected, refill the USB RX queue. */
	if (!disconnect && port->port_usb)
		gs_start_rx(port);

	return do_push;

}
/*
buf: date buffer address which store the data want send
count: the data count stored in buf
return: the data send count
*/
int gs_write(const unsigned char *buf, int count)
{
	struct gs_port	*port = ports[vcom_port].port;
	if (count)
		count = gs_buf_put(&port->port_write_buf, buf, count);
	/* treat count == 0 as flush_chars() */
	if (port->port_usb)
		usb_trans_status = gs_start_tx(port);
	return count;
}

static void gs_read_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gs_port	*port = ep->driver_data;

	/* Queue all received data until the tty layer is ready for it. */
	spin_lock(&port->port_lock);
	list_add_tail(&port->read_queue, &req->list);
	usb_read_done = 1;
	usb_trans_status = req->status;
	//tasklet_schedule(&port->push);
	spin_unlock(&port->port_lock);
}

static void gs_write_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gs_port	*port = ep->driver_data;

	spin_lock(&port->port_lock);
	list_add(&req->list, &port->write_pool);
	port->write_started--;

	switch (req->status) {
	default:
		/* presumably a transient fault */
		dprintf(INFO,"%s: unexpected %s status %d\n",
				__func__, ep->name, req->status);
		/* FALL THROUGH */
	case 0:
		/* normal completion */
		gs_start_tx(port);
		break;

	case -ESHUTDOWN:
		/* disconnect */
		debugf("%s: %s shutdown\n", __func__, ep->name);
		break;
	}

	usb_write_done = 1;
	usb_trans_status = req->status;

	spin_unlock(&port->port_lock);
}
/*
wait the read or write done
direct: 1 for output, 0 for input
*/
void usb_wait_trans_done(int direct)
{
	if(direct){
		while(!usb_write_done)
			usb_gadget_handle_interrupts();
		 usb_write_done = 0;
	}else{
		while(!usb_read_done)
			usb_gadget_handle_interrupts();
		usb_read_done = 0;
	}
}
int usb_is_trans_done(int direct)
{
	if(direct){
		if(!usb_write_done)
			usb_gadget_handle_interrupts();
		if(usb_write_done){
            usb_write_done = 0;
            return 1;
        }
	}else{
		if(!usb_read_done)
			usb_gadget_handle_interrupts();
		if(usb_read_done){
            usb_read_done = 0;
            return 1;
        }
	}
    return 0;
}

int usb_serial_configed;
int usb_is_configured(void)
{
	if(!usb_serial_configed)
		usb_gadget_handle_interrupts();

	return usb_serial_configed;
}

extern int usb_port_open;
int usb_is_port_open(void)
{
	if(!usb_port_open)
		usb_gadget_handle_interrupts();

	return usb_port_open;
}


static int gs_alloc_requests(struct usb_ep *ep, struct list_head *head,
		void (*fn)(struct usb_ep *, struct usb_request *),
		int *allocated)
{
	int			i;
	struct usb_request	*prequest;
	int size = allocated ? QUEUE_SIZE - *allocated : QUEUE_SIZE;

	for (i = 0; i < size; i++) {
		prequest = gs_alloc_req(ep, ep->maxpacket, GFP_ATOMIC);
		if (!prequest)
			return list_is_empty(head) ? -ENOMEM : 0;
		prequest->complete = fn;
		list_add_tail(head, &prequest->list);
		if (allocated)
			(*allocated)++;
	}
	return 0;
}

static void gs_free_requests(struct usb_ep *ep, struct list_head *head,
							 int *allocated)
{
	struct usb_request	*req;

	while (!list_is_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_delete(&req->list);
		gs_free_req(ep, req);
		if (allocated)
			(*allocated)--;
	}
}

static int gs_closed(struct gs_port *port)
{
	int cond;

	cond = (port->open_count == 0) && !port->openclose;
	return cond;
}

static int gs_start_io(struct gs_port *port)
{
	struct list_head	*head = &port->read_pool;
	struct usb_ep		*ep = port->port_usb->out;
	int			ret;
	unsigned		started;

	ret = gs_alloc_requests(ep, head, gs_read_complete,
		&port->read_allocated);
	if (ret)
		return ret;

	ret = gs_alloc_requests(port->port_usb->in, &port->write_pool,
			gs_write_complete, &port->write_allocated);
	if (ret) {
		gs_free_requests(ep, head, &port->read_allocated);
		return ret;
	}

	/* queue read requests */
	port->n_read = 0;
	started = gs_start_rx(port);

	/* unblock any pending writes into our circular buffer */
	if (started) {
		//tty_wakeup(port->port_tty);
	} else {
		gs_free_requests(ep, head, &port->read_allocated);
		gs_free_requests(port->port_usb->in, &port->write_pool,
			&port->write_allocated);
		ret = -EIO;
	}

	return ret;
}

static int __init sprd_gs_port_alloc(unsigned port_num,
			struct usb_cdc_line_coding *coding)
{
	struct gs_port	*gsport;

	gsport = kzalloc(sizeof(struct gs_port), GFP_KERNEL);
	if (NULL == gsport)
		return -ENOMEM;

	spin_lock_init(&gsport->port_lock);
	init_waitqueue_head(&gsport->close_wait);
	init_waitqueue_head(&gsport->drain_wait);

	INIT_LIST_HEAD(&gsport->read_pool);
	INIT_LIST_HEAD(&gsport->read_queue);
	INIT_LIST_HEAD(&gsport->write_pool);

	gsport->port_num = port_num;
	gsport->port_line_coding = *coding;

	ports[port_num].port = gsport;

	return 0;
}

/**
 * sprd_gserial_setup - init driver for one or more ports
 */
int __init sprd_gserial_setup(struct usb_gadget *gser, unsigned count)
{
	unsigned			i;
	struct usb_cdc_line_coding	coding;
	int				ret =0;

	if (count == 0 || count > N_PORTS)
		return -EINVAL;

	memset(&coding,0,sizeof(struct usb_cdc_line_coding));
	/* make devices be openable */
	for (i = 0; i < count; i++) {
		mutex_init(&ports[i].lock);
		ret = sprd_gs_port_alloc(i, &coding);
		if (ret) {
			count = i;
			goto fail;
		}
	}
	n_ports = count;

	return ret;
fail:
	while (count--) {
		kfree(ports[count].port);
		ports[count].port = NULL;
	}

	return ret;
}


int sprd_gserial_connect(struct gserial *gser, u8 port_num)
{
	struct gs_port	*port;
	unsigned long	flags;
	int		ret;

	if (port_num >= n_ports)
		return -ENXIO;

	/* we "know" sprd_gserial_cleanup() hasn't been called */
	port = ports[port_num].port;

	/* activate the endpoints */
	ret = usb_ep_enable(gser->in, gser->in->desc);
	if (ret < 0)
		return ret;
	gser->in->driver_data = port;

	ret = usb_ep_enable(gser->out, gser->out->desc);
	if (ret < 0)
		goto fail_out;
	gser->out->driver_data = port;

	/* then tell the tty glue that I/O can work */
	spin_lock_irqsave(&port->port_lock, flags);
	gser->ioport = port;
	port->port_usb = gser;

	gser->port_line_coding = port->port_line_coding;

	if (port->open_count) {
		debugf("%s: start ttyGS%d\n", __func__, port->port_num);
		gs_start_io(port);
		if (gser->connect)
			gser->connect(gser);
	} else {
		if (gser->disconnect)
			gser->disconnect(gser);
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
	usb_serial_configed = 1;
	return ret;

fail_out:
	usb_ep_disable(gser->in);
	gser->in->driver_data = NULL;
	return ret;
}

void sprd_gserial_cleanup(void)
{
	unsigned	i;
	struct gs_port	*port;

	for (i = 0; i < n_ports; i++) {
		/* prevent new opens */
		mutex_lock(&ports[i].lock);
		port = ports[i].port;
		ports[i].port = NULL;
		mutex_unlock(&ports[i].lock);

		kfree(port);
	}
	n_ports = 0;

	debugf("%s: cleaned up ttyGS* support\n", __func__);
}


void sprd_gserial_disconnect(struct gserial *p)
{
	struct gs_port	*port = p->ioport;
	unsigned long	ulflags;

	if (NULL == port)
		return;

	/* tell the TTY glue not to do I/O here any more */
	spin_lock_irqsave(&port->port_lock, ulflags);

	/* REVISIT as above: how best to track this? */
	port->port_line_coding = p->port_line_coding;

    p->ioport = NULL;
	port->port_usb = NULL;
	if (port->open_count > 0 || port->openclose) {
		wake_up_interruptible(&port->drain_wait);
	}
	spin_unlock_irqrestore(&port->port_lock, ulflags);

	/* disable endpoints, aborting down any active I/O */
	if (p->out) {
		usb_ep_disable(p->out);
		p->out->driver_data = NULL;
	}
	if (p->in) {
		usb_ep_disable(p->in);
		p->in->driver_data = NULL;
	}

	/* finally, free any unused/unusable I/O buffers */
	spin_lock_irqsave(&port->port_lock, ulflags);
	if (port->open_count == 0 && !port->openclose)
		gs_buf_free(&port->port_write_buf);
	gs_free_requests(p->out, &port->read_pool, NULL);
	gs_free_requests(p->out, &port->read_queue, NULL);
	gs_free_requests(p->in, &port->write_pool, NULL);

	port->read_allocated = port->read_started =
		port->write_allocated = port->write_started = 0;
	usb_serial_configed = 0;
	spin_unlock_irqrestore(&port->port_lock, ulflags);
}
void gs_reset_usb_param(void)
{
	vcom_port = 0;
	usb_write_done = 0;
	usb_read_done = 0;
	usb_trans_status = 0;
	usb_serial_configed = 0;
	usb_port_open = 0;
}
