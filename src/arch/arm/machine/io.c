/*
 * Copyright 2021, Axel Heider <axelheider@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include <machine/io.h>
#include <drivers/uart.h>

#ifdef CONFIG_PRINTING

/* Multikernel-AMP: prefix every printed line with the running CPU's MPIDR.AFF0
 * so we can tell K0 (CPU 0) from K1 (CPU 1) on the shared UART. Diagnostic
 * only — has no effect under single-kernel boot since AFF0 is always 0. */
static word_t mk_read_mpidr_aff0(void)
{
    word_t mpidr;
#ifdef CONFIG_ARCH_AARCH64
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
#else
    asm volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
#endif
    return mpidr & 0xff;  /* AFF0 only */
}

void kernel_putDebugChar(unsigned char c)
{
    static int at_line_start = 1;

    if (at_line_start) {
        word_t aff0 = mk_read_mpidr_aff0();
        uart_console_putchar('[');
        uart_console_putchar('K');
        uart_console_putchar('0' + (unsigned char)aff0);
        uart_console_putchar(']');
        uart_console_putchar(' ');
        at_line_start = 0;
    }

    uart_console_putchar(c);

    if (c == '\n') {
        at_line_start = 1;
    }
}
#endif /* CONFIG_PRINTING */

#ifdef CONFIG_DEBUG_BUILD
unsigned char kernel_getDebugChar(void)
{
    return uart_drv_getchar();
}
#endif /* CONFIG_DEBUG_BUILD */
