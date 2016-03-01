/*
 *  linux/drivers/serial/tsuart-bat3.c
 *
 *  TS-UART loader for Technologic Systems TS-RF2
 *
 *  (c) 2006 Technologic Systems
 *
 * This source code is hereby licensed to the Linux 2.4 kernel under the
 * GPL, and to Technologic Systems customers under the BSD license.
 *
 * 2007-11-28:mos:initial 2.6 version ported from 2.4 version
 * 2012-07-25:mos:initial port to 2.6.36 for TS-4200 + TS-8160
 *
 */
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/serial_core.h>
#include <asm/uaccess.h>
#include "tsuart1.h"

static unsigned int io=0,irq6=0,irq7=0;
#define BOARD_ID_TSRF 0x5162

//static unsigned long saved_A=0, saved_B=0;

// enable PC104: devmem 0x30000020 16 0x181

static void on104(void) {
  volatile unsigned short *tmp;
  tmp = ((unsigned short *) ioremap_nocache(0x30000000, 4096));
  if (tmp == (unsigned short *)0x30000000) {
    printk(KERN_ERR "tsuart1: cannot map 0x30000020\n");
    return;
  }
  tmp[0x10] = 0xF0FF; // conservative bus timings, as default don't work
  tmp[0x12] = 0xFFFF; // enable interrupts
  __iounmap(tmp);
}

static void off104(void) {
  volatile unsigned short *tmp;
  tmp = ((unsigned short *) ioremap_nocache(0x30000000, 4096));
  if (tmp == (unsigned short *)0x30000000) {
    printk(KERN_ERR "tsuart1: cannot map 0x30000020\n");
    return;
  }
  tmp[0x10] = 0;
  __iounmap(tmp);
}


static int mdmctrl(struct tsuart_port *ts,int mctrl) {
  int dtr=1;

  if (mctrl & 0x80000000) {
    tsuartPutMEM8(ts,-1,
		 (tsuartGetMEM8(ts,-1) & 0xFE) | 
		 ((mctrl & TIOCM_DTR) ? 1 : 0));
    dtr =  mctrl & TIOCM_DTR;
  } else {
    dtr = (tsuartGetMEM8(ts,-1) & 0x01) ? TIOCM_DTR : 0;
  }
  return (TIOCM_CAR | TIOCM_DSR | dtr);
}

//static unsigned getx(struct tsuart_port *port,int offset) {
//  return (offset) ? 0 : 0xE1;
//}
//static void putx(struct tsuart_port *port,int offset,unsigned value) { }

//struct tsuart_port TS1[4];

// TS-4200 8-bit PC-104 at 0x10000400, length 0x400

static int tryInitPort(int iobase) {
  struct tsuart_port *ts;
  unsigned val;
  volatile unsigned char *sh;

  if (!io) {
    io = 0x10000400;
  }
  ts = kmalloc(sizeof(struct tsuart_port),GFP_KERNEL);
/*
  switch (iobase) {
  case 0x100: ts = &TS1[0]; break;
  case 0x110: ts = &TS1[1]; break;
  case 0x200: ts = &TS1[2]; break;
  case 0x210: ts = &TS1[3]; break;
  default: ts = 0; break;
  }
*/
  if (!ts) {
    printk("Unable to allocate memory for tsuart port\n");
    return 0;
  }
  memset(ts,0,sizeof(struct tsuart_port));
  init_tsuart_port(ts);

  ts->u.iotype = SERIAL_IO_MEM;
//  ts->u.iotype = SERIAL_IO_PORT;
//  ts->u.iobase = 0x100;

  ts->get = tsuartGetMEM8;
  ts->put = tsuartPutMEM8;
//  ts->get = getx;
//  ts->put = putx;

  ts->debugFlags = portDebug;
  ts->u.mapbase = io + iobase + 2;
  ts->boardId = BOARD_ID_TSRF;
  ts->mdmctrl = mdmctrl;
//  ts->mdmctrl = 0;
 sh = ((volatile unsigned char *) ioremap_nocache(io & 0xFFFFF000, 4096)) + (io&0xFFF);
 if (sh == (volatile unsigned char *)io) {
   printk(KERN_ERR "tsuart-bat3: cannot map %p\n",(char *)io);
   return -ENOMEM;
  }
  sh += iobase;
  val = sh[0];
  if (val == 0x8e) {
    val = sh[1];
    ts->localStatus |= PORT_STATUS_SHIRQ; //share IRQ6 & IRQ7 with IDE
    if ((val & 0x80) == 0x80) {
      ts->u.irq = irq7; // IRQ 7
    } else {
      ts->u.irq = irq6; // IRQ 6
    }
    printk("TS-UART/RF2 found at port 0x%04X\n",iobase);
    if (tsuart_register_port(0,ts) == 0) {
      __iounmap(sh);
      return 1;
    }
  }
  __iounmap(sh);

  return 0;
}

static int __init tsuart_init(void)
{
  int ports=0,io0=io;

  if (!io0) {
    on104();
  }
  if (!irq6) {
    irq6 = 193;
  }
  if (!irq7) {
    irq7 = 194;
  }
  if (tryInitPort(0x100)) {
    ports++;
  }
  if (tryInitPort(0x110)) {
    ports++;
  }
  if (tryInitPort(0x200)) {
    ports++;
  }
  if (tryInitPort(0x210)) {
    ports++;
  }
  if (ports == 0) {
    printk("TS-UART/RF2 did not detect a TS-RF2 board\n");
    if (!io0) {
      off104();
    }
  }
  return (ports > 0) ? 0 : -1;
}

static void __exit tsuart_exit(void)
{
  // This module can be safely unloaded after initialization is complete.
  // We allow module to load only so error conditions can be detected.
}

module_init(tsuart_init);
module_exit(tsuart_exit);

module_param(io, uint, 0644);
MODULE_PARM_DESC(io, "Base address for TS-RF2 UARTS");
module_param(irq6, uint, 0644);
MODULE_PARM_DESC(io, "ARM IRQ for ISA IRQ 6");
module_param(irq7, uint, 0644);
MODULE_PARM_DESC(io, "ARM IRQ for ISA IRQ 7");

