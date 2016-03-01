#ifndef TS_FPGA_H
#define TS_FPGA_H

#include <asm/mach-types.h>

#ifdef CONFIG_MACH_TS42XX

#include <mach/hardware.h>

#define FPGA_BASE_ADDR	    0x30000000
#define FPGA_BASE_ADDR_VIRT (AT91_IO_VIRT_BASE - AT91SAM9G20_SRAM0_SIZE - AT91SAM9G20_SRAM1_SIZE - FPGA_SIZE)
#define FPGA_SIZE	    SZ_4K

#endif /* CONFIG_MACH_TS42XX */

#endif /* TS_FPGA_H */
