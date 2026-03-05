/*
 * (C) Copyright 2021 Spreadtrum Communications Inc.
 */
#include <config.h>
#include <sprd_common.h>
#include <linux/compiler.h>
#include "serial_sprd.h"
#include <dl_channel.h>


typedef struct UartPort
{
	void *regbase;
	void *apbbase;
	unsigned  int eb;
	int id;
	unsigned long int baudRate;
}uart_t;

#ifdef CONFIG_SPRD_SP_UART
#define msleep(cnt) udelay(cnt*1000)
#endif

#ifdef CONFIG_ZEBU
#define CONFIG_BAUDRATE		2600000
#define CONFIG_CONS_INDEX	0
#else
/* defined in config/xxx.h  */
#define CONFIG_BAUDRATE		921600
#define CONFIG_CONS_INDEX	1
/*=================*/
#endif

#define CONSOLE_PORT CONFIG_CONS_INDEX
#define NUM_PORTS (sizeof(port)/sizeof(port[0]))
#define ARM_APB_CLK    26000000UL

//DECLARE_GLOBAL_DATA_PTR;

static uart_t  port[] = {
#ifdef CONFIG_SPRD_SP_UART
	{
		(void *)(SPRD_SP_UART0_PHYS),
		(void *)SPRD_SP_AHB_PHYS,
		BIT_SP_UART0_EB,
		0,
		0
	},
#else
	{
		(void *)(SPRD_UART0_PHYS),
		(void *)REG_AP_APB_APB_EB,
		BIT_UART0_EB,
		0,
		0
	},
#endif
	{
		(void *)(SPRD_UART1_PHYS),
		(void *)REG_AP_APB_APB_EB,
		BIT_UART1_EB,
		1,
		0
	}
};

extern int platform_debug_enable(void);
int volatile con_en = 1;
static struct sprd_uart_regs *sprd_get_regs(int num)
{
	return (struct sprd_uart_regs *)(port[num].regbase);
}

#ifdef CONFIG_SPRD_SP_UART
void sprd_pmic_arm7_RAM_active(void)
{
	*((volatile u32*)REG_AON_APB_CM4_SYS_SOFT_RST) |= BIT_AON_APB_CM4_CORE_SOFT_RST | BIT_AON_APB_CM4_SYS_SOFT_RST;
	msleep(50);
	*((volatile u32*)REG_PMU_APB_CP_SOFT_RST) &= ~BIT_PMU_APB_SP_SYS_SOFT_RST;
	*((volatile u32*)REG_PMU_APB_SLEEP_CTRL) &= ~BIT_PMU_APB_SP_SYS_FORCE_DEEP_SLEEP;
	/*clear sp force sleep */
	*((volatile  u32*)REG_AON_APB_CM4_SYS_SOFT_RST)	&= ~BIT_AON_APB_CM4_SYS_SOFT_RST;
	msleep(50);
}
#endif

static void sprd_apb_eb(int num)
{
	u32 temp;
#ifdef CONFIG_SPRD_SP_UART
	sprd_pmic_arm7_RAM_active();
#endif
	temp = readl(port[num].apbbase);

	if(con_en)
		writel(temp | port[num].eb, port[num].apbbase);
	else
		writel(temp & (~port[num].eb), port[num].apbbase);
}

static void sprd_apb_disable(int num)
{
	u32 temp;

	temp = readl(port[num].apbbase);
	writel(temp & (~port[num].eb), port[num].apbbase);
}

void sprd_serial_close(void)
{
	sprd_apb_disable(CONSOLE_PORT);
}

static int sprd_init( int num, u32 baudrate)
{
	struct sprd_uart_regs *regs = sprd_get_regs(num);

	if(!con_en)
		return 0;
	/* Set port for 8 bit, one stop, no parity  */
	writel(UARTCTL_BL8BITS | UARTCTL_SL1BITS, &regs->uart_ctrl0);
	/* Set baud rate  */
	writel(baudrate, &regs->uart_ckd0);
	return 0;
}

void sprd_serial_init(void)
{
	u32 divider = ARM_APB_CLK / CONFIG_BAUDRATE;

#ifdef DISABLE_UART
	if( (platform_debug_enable()&0xff) == 0xff)
		con_en = 1;
	else
		con_en = 0;
#endif
	/* Enable UART*/
	sprd_apb_eb(CONSOLE_PORT);

#ifdef CONFIG_ZEBU
	dprintf(INFO,"Using uart baudrate setting in SML\n");
#else
	sprd_init(CONSOLE_PORT, divider);
#endif
}

void sprd_serial_setbrg(int num, u32 baudrate)
{
	u32 divider = ARM_APB_CLK / baudrate;
	struct sprd_uart_regs *regs = sprd_get_regs(num);

	writel(divider, &regs->uart_ckd0);
}

static int sprd_txfifo_cnt(void)
{
	struct sprd_uart_regs *regs = sprd_get_regs(CONSOLE_PORT);

	return ((readl(&regs->uart_sts1) >> 8) & 0xff );
}

int sprd_rxfifo_cnt(void)
{
	struct sprd_uart_regs *regs = sprd_get_regs(CONSOLE_PORT);

	return (readl(&regs->uart_sts1) & 0xff);
}

static void sprd_putc (int portnum, char c)
{
	struct sprd_uart_regs *regs = sprd_get_regs(portnum);

	if(!con_en){
		udelay(100);
		return;
	}
	while ( ((readl(&regs->uart_sts1) >> 8) & 0xff ))
	/* wait for room in the tx FIFO */ ;
	writeb(c, &regs->uart_txd);
	/* Ensure the last byte is written successfully */
	while ( ((readl(&regs->uart_sts1) >> 8) & 0xff ));
}

static int sprd_getc (int portnum)
{
	struct sprd_uart_regs *regs = sprd_get_regs(portnum);

	if(!con_en)
		return 0;

	while (!(readl(&regs->uart_sts1) & 0xff))
	/* wait for character to arrive */ ;
	return (readb(&regs->uart_rxd) & 0xff);
}

static int sprd_tstc (int portnum)
{
	struct sprd_uart_regs *regs = sprd_get_regs(portnum);

	if(!con_en)
		return 0;

	/* If receive fifo is empty, return false */
	return (readl(&regs->uart_sts1) & 0xff);
}

int sprd_serial_getc(void)
{
	return sprd_getc (CONSOLE_PORT);
}

/*
 *  * Test whether a character is in the RX buffer
 *   */
int sprd_serial_tstc(void)
{
	return sprd_tstc (CONSOLE_PORT);
}
void sprd_serial_putc(const char c)
{
	if (c == '\n')
		sprd_putc (CONSOLE_PORT, '\r');

	sprd_putc (CONSOLE_PORT, c);
}

void sprd_serial_puts(const char *s)
{
	while (*s)
		sprd_serial_putc(*s++);

}
#if 0
static struct serial_device sprd_serial_drv = {
	.name	= "sprd_serial",
	.start	= sprd_serial_init,
	.stop	= NULL,
	.setbrg	= sprd_serial_setbrg,
	.getc	= sprd_serial_getc,
	.tstc	= sprd_serial_tstc,
	.putc	= sprd_serial_putc,
	.puts	= default_serial_puts,
};

void sprd_uart_initialize(void)
{
	serial_register(&sprd_serial_drv);
}
__weak struct serial_device *default_serial_console(void)
{
	return &sprd_serial_drv;
}

#endif

#ifdef CONFIG_UART_DOWNLOAD
/*
   for uart download
*/
static unsigned int sprd_getHwDivider (unsigned int baudrate)
{
    return (unsigned int) (ARM_APB_CLK / baudrate  );
}

static int sprd_setBaudrate (struct FDL_ChannelHandler  *channel,  unsigned int baudrate)
{
	unsigned int divider;
	uart_t *port  = (uart_t *) channel->priv;
	struct sprd_uart_regs *regs = (struct sprd_uart_regs *)(port->regbase);

	divider = sprd_getHwDivider (baudrate);
	/* Set baud rate  */
	 regs->uart_ckd0 = LWORD (divider);

	 return 0;
}

static int sprd_read (struct FDL_ChannelHandler  *channel, const unsigned char *buf, unsigned int len)
{
	unsigned char *pstart = (unsigned char *) buf;
	const unsigned char *pend = pstart + len;
	uart_t *port  = (uart_t *) channel->priv;
	volatile struct sprd_uart_regs *regs = (struct sprd_uart_regs *)(port->regbase);

	while ( (pstart < pend)&& ((regs->uart_sts1)&0xff)){
	    *pstart++ = (regs->uart_rxd);
	}

	return pstart - (unsigned char *) buf;
}

static char sprd_getChar (struct FDL_ChannelHandler  *channel)
{
	uart_t *port  = (uart_t *) channel->priv;
	volatile  struct sprd_uart_regs *regs = (struct sprd_uart_regs *)(port->regbase);
	while(!((regs->uart_sts1)&0xff)){
	}
	return (regs->uart_rxd);
}

static int sprd_getSingleChar (struct FDL_ChannelHandler  *channel)
{
	char ch;
	uart_t *port  = (uart_t *) channel->priv;
	volatile struct sprd_uart_regs *regs = (struct sprd_uart_regs *)(port->regbase);

	while(!((regs->uart_sts1)&0xff)){
	}
	ch  = (regs->uart_rxd);
	return ch;

}
static int sprd_write (struct FDL_ChannelHandler  *channel, const unsigned char *buf, unsigned int len)
{
	const unsigned char *pstart = (const unsigned char *) buf;
	const unsigned char *pend = pstart + len;
	uart_t *port  = (uart_t *) channel->priv;
	volatile struct sprd_uart_regs *regs = (volatile struct sprd_uart_regs *)(port->regbase);
	unsigned char tx_fifo_cnt = 0;

	while (pstart < pend){
		while ((((regs->uart_sts1)>>8)&0xff)){
		}
		regs->uart_txd = (*pstart);
		++pstart;
	}

	while ((((regs->uart_sts1)>>8)&0xff)){
	}

	return pstart - (const unsigned char *) buf;
}

static int sprd_putChar (struct FDL_ChannelHandler  *channel, const unsigned char ch)
{
	uart_t *port  = (uart_t *) channel->priv;
	volatile struct sprd_uart_regs *regs = (struct sprd_uart_regs *)(port->regbase);
	while (((regs->uart_sts1)>>8)&0xff){
	}
	regs->uart_txd  = ch;
	/* Ensure the last byte is written successfully */
	while (((regs->uart_sts1)>>8)&0xff){
	/* Do nothing */
	}
	return 0;
}
static int sprd_close (struct FDL_ChannelHandler  *channel)
{
	return 0;
}
static int sprd_open (struct FDL_ChannelHandler  *channel, unsigned int baudrate)
{
	uart_t *port  = (uart_t *) channel->priv;
	u32 divider = ARM_APB_CLK / baudrate;

	sprd_apb_eb(port->id);
	sprd_init(port->id, divider);
    return 0;
}

/****
   FDL2 download struct
***/
FDL_ChannelHandler_T  gUart1Channel = {
	.Open = sprd_open,
	.Read = sprd_read,
	.GetChar = sprd_getChar,
	.GetSingleChar = sprd_getSingleChar,
	.Write = sprd_write,
	.PutChar = sprd_putChar,
	.SetBaudrate =sprd_setBaudrate,
	.DisableHDLC = NULL,
	.Close = sprd_close,
	.priv = &port[1],
};
FDL_ChannelHandler_T  gUart0Channel = {
	.Open = sprd_open,
	.Read = sprd_read,
	.GetChar = sprd_getChar,
	.GetSingleChar = sprd_getSingleChar,
	.Write = sprd_write,
	.PutChar = sprd_putChar,
	.SetBaudrate =sprd_setBaudrate,
	.DisableHDLC = NULL,
	.Close = sprd_close,
	.priv = &port[0],
};
#endif

void sprd_uart_setbaudrate(int baudrate)
{
	struct sprd_uart_regs *regs = sprd_get_regs(CONSOLE_PORT);
	/*make sure baudrate not be zero*/
	if (baudrate <= 0)
		baudrate = 115200;
	u32 divider = ARM_APB_CLK / baudrate;
	CHIP_REG_SET(&regs->uart_ckd0, divider);
}
