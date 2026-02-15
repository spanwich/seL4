/*
 * Copyright 2025, PhD Research Project
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * STM32H7 USART driver for seL4.
 * Register layout from Linux stm32-usart.h (STM32F7/H7/MP2 variant).
 * U-Boot initializes the USART (baud, clocks, GPIO mux) — we just TX/RX.
 */

#include <config.h>
#include <stdint.h>
#include <util.h>
#include <machine/io.h>
#include <plat/machine/devices_gen.h>

#define USART_ISR   0x1c    /* Interrupt and status register */
#define USART_RDR   0x24    /* Receive data register */
#define USART_TDR   0x28    /* Transmit data register */

#define USART_ISR_TXE   BIT(7)  /* Transmit data register empty */
#define USART_ISR_RXNE  BIT(5)  /* Read data register not empty */

#define UART_REG(x) ((volatile uint32_t *)(UART_PPTR + (x)))

#ifdef CONFIG_PRINTING
void uart_drv_putchar(unsigned char c)
{
    while (!(*UART_REG(USART_ISR) & USART_ISR_TXE));
    *UART_REG(USART_TDR) = c;
}
#endif /* CONFIG_PRINTING */

#ifdef CONFIG_DEBUG_BUILD
unsigned char uart_drv_getchar(void)
{
    while (!(*UART_REG(USART_ISR) & USART_ISR_RXNE));
    return *UART_REG(USART_RDR);
}
#endif /* CONFIG_DEBUG_BUILD */
