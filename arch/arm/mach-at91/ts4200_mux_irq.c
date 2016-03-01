#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/bitops.h>
#include <mach/hardware.h>

#include "ts_fpga.h"

#define FPGA_REGS_BASE FPGA_BASE_ADDR_VIRT
#define FPGA_REG(x) (*((volatile u16 *)(FPGA_REGS_BASE + (x))))
#define FPGA_IRQ_OFFSET       0x22     /* IRQ bits */
#define FPGA_IRQ_MASK_OFFSET  0x24     /* IRQ mask bits (1=masked) */
#define MIN_FPGA_IRQ    192
#define MAX_FPGA_IRQ    (MIN_FPGA_IRQ + 2)
#define IS_VALID_FPGA_IRQ(x) ((x) >= MIN_FPGA_IRQ && (x) <= MAX_FPGA_IRQ)

static void mask_muxed_irq(unsigned int irq);
static void unmask_muxed_irq(unsigned int irq);

void mux_irq_handler(unsigned int irq, struct irq_desc *desc)
{
    unsigned int bit, n;
    unsigned short fpga_irq_reg;

    /*
    * On the TS-4200, the FPGA IRQ line is connected to the CPU's external
    * interrupt IRQ0.  This handler is installed on the IRQ down below in
    * ts4200_init_mux_irq().
    *
    * When the FPGA asserts its IRQ line, we end up here.  Now we need to read
    * the FPGA's IRQ register to determine what peripheral(s) need to be
    * serviced.
    */

    /*    Read the IRQ register from the FPGA ...
    *
    *    BIT 0: PC/104 IRQ 5     (IRQ #192)
    *    BIT 1: PC/104 IRQ 6     (IRQ #193)
    *    BIT 2: PC/104 IRQ 7     (IRQ #194)
    *    BITS 3-15: Reserved
    */
	fpga_irq_reg = 0x07 & FPGA_REG(FPGA_IRQ_OFFSET);

	for (n=0, bit=1; bit < 0x08; bit <<= 1, n++)
	{
	    if (fpga_irq_reg & bit)
	    {
		generic_handle_irq(MIN_FPGA_IRQ + n);
	    }
	}
}

static void unmask_muxed_irq(unsigned int irq)
{
    int bit = irq - MIN_FPGA_IRQ;
    if (IS_VALID_FPGA_IRQ(irq)) FPGA_REG(FPGA_IRQ_MASK_OFFSET) |= BIT(bit);
}

static void mask_muxed_irq(unsigned int irq)
{
    int bit = irq - MIN_FPGA_IRQ;
    if (IS_VALID_FPGA_IRQ(irq)) FPGA_REG(FPGA_IRQ_MASK_OFFSET) &= ~BIT(bit);
}

static void ack_muxed_irq(unsigned int irq)
{
#if (0)
    /* temp hack for test... */
    if (IS_VALID_FPGA_IRQ(irq))
    {
       printk("ack %d\n", irq);
    }
#endif
}

static struct irq_chip muxed_irq_chip = {
    .name    = "MUXIRQ",
    .mask    = mask_muxed_irq,
    .unmask  = unmask_muxed_irq,
    .ack     = ack_muxed_irq
};

void __init ts4200_init_mux_irq(void)
{
    int n;

    FPGA_REG(FPGA_IRQ_MASK_OFFSET) = 0x0;  /* mask all */

    for (n=192; n <= 194; n++)
    {
	set_irq_chip(n, &muxed_irq_chip);
	set_irq_handler(n, handle_level_irq);
	set_irq_flags(n, IRQF_VALID);
    }
    set_irq_chained_handler(AT91SAM9260_ID_IRQ0, mux_irq_handler);
}
