/*
 * Copyright 2021, Axel Heider <axelheider@gmx.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include <machine/io.h>
#include <drivers/uart.h>
#include <plat/machine/devices_gen.h>

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

#ifdef MULTIKERNEL_SHARED_POOL_PPTR
/* Cross-kernel UART proxy ring buffer.
 *
 * Lives in the shared 4 KiB device frame at MULTIKERNEL_SHARED_POOL_PPTR
 * (mapped to the same kernel virtual address in K0 and K1; backed by the
 * same physical page 0x6FFFF000). K1 enqueues its UART chars; K0 polls
 * the ring and drains them to the real PL011 between its own writes.
 *
 * Lockless single-producer / single-consumer:
 *   - K1 writes data[head] then advances head (release).
 *   - K0 reads head (acquire), reads data[tail..head], advances tail.
 * No tearing because head/tail are word-aligned _Atomic uint32_t.
 *
 * Cortex-A35 inner-shareable coherency makes default cacheable mappings
 * sufficient — confirmed in docs/uart_dataport_design_review.md (Q1).
 */
struct mk_log_ring {
    uint32_t head;                /* writer (K1) advances */
    uint32_t tail;                /* reader (K0) advances */
    uint8_t data[4096 - 16];      /* ring payload (shrunk by 8 for mailbox) */
    /* Task #9 bidirectional mailbox: simple counters that each kernel
     * increments per newline. Each kernel embeds the OTHER kernel's
     * latest seq in its line tag, demonstrating round-trip cross-kernel
     * comm over shared memory: K0 writes -> K1 reads, K1 writes -> K0 reads. */
    uint32_t k0_seq;              /* K0 increments, K1 reads */
    uint32_t k1_seq;              /* K1 increments, K0 reads */
};
static volatile struct mk_log_ring *const mk_ring =
    (volatile struct mk_log_ring *)MULTIKERNEL_SHARED_POOL_PPTR;

#define MK_RING_DATA_SZ  (sizeof(((struct mk_log_ring *)0)->data))

/* Kernel is built -nostdinc so <stdatomic.h> isn't available. Use the GCC
 * builtins directly with explicit memory orders. ARMv8 LDAR/STLR map onto
 * __ATOMIC_ACQUIRE / __ATOMIC_RELEASE. */
static void mk_ring_push(unsigned char c)
{
    uint32_t h = __atomic_load_n(&mk_ring->head, __ATOMIC_RELAXED);
    uint32_t t = __atomic_load_n(&mk_ring->tail, __ATOMIC_ACQUIRE);
    uint32_t next_h = h + 1;
    /* Drop on full — kernel logging mustn't block. */
    if (next_h - t > MK_RING_DATA_SZ) {
        return;
    }
    mk_ring->data[h % MK_RING_DATA_SZ] = c;
    __atomic_store_n(&mk_ring->head, next_h, __ATOMIC_RELEASE);
}

static void mk_ring_drain(void)
{
    uint32_t h = __atomic_load_n(&mk_ring->head, __ATOMIC_ACQUIRE);
    uint32_t t = __atomic_load_n(&mk_ring->tail, __ATOMIC_RELAXED);
    while (t != h) {
        uart_console_putchar(mk_ring->data[t % MK_RING_DATA_SZ]);
        t++;
    }
    __atomic_store_n(&mk_ring->tail, t, __ATOMIC_RELEASE);
}
#endif /* MULTIKERNEL_SHARED_POOL_PPTR */

/* Helpers for the Task #9 bidirectional mailbox. Embed the OTHER kernel's
 * latest seq in our tag so each line shows the round-trip number. */
#ifdef MULTIKERNEL_SHARED_POOL_PPTR
static void put_dec_via(void (*put)(unsigned char), uint32_t v)
{
    char buf[12];
    int i = 0;
    if (v == 0) {
        put('0');
        return;
    }
    while (v && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i--) {
        put((unsigned char)buf[i]);
    }
}

static void uart_put(unsigned char c) { uart_console_putchar(c); }
static void ring_put(unsigned char c) { mk_ring_push(c); }
#endif

void kernel_putDebugChar(unsigned char c)
{
    static int at_line_start = 1;
    word_t aff0 = mk_read_mpidr_aff0();

#ifdef MULTIKERNEL_SHARED_POOL_PPTR
    /* K1 (and any non-boot CPU): forward to shared ring instead of UART
     * to avoid char-level interleaving on the single PL011. */
    if (aff0 != 0) {
        if (at_line_start) {
            uint32_t k0 = __atomic_load_n(&mk_ring->k0_seq, __ATOMIC_ACQUIRE);
            mk_ring_push('[');
            mk_ring_push('K');
            mk_ring_push('0' + (unsigned char)aff0);
            mk_ring_push('/');
            mk_ring_push('k'); mk_ring_push('0'); mk_ring_push('=');
            put_dec_via(ring_put, k0);
            mk_ring_push(']');
            mk_ring_push(' ');
            at_line_start = 0;
        }
        mk_ring_push(c);
        if (c == '\n') {
            /* Increment our seq so K0 sees us moving forward. */
            uint32_t s = __atomic_load_n(&mk_ring->k1_seq, __ATOMIC_RELAXED);
            __atomic_store_n(&mk_ring->k1_seq, s + 1, __ATOMIC_RELEASE);
            at_line_start = 1;
        }
        return;
    }
    /* K0: drain anything K1 has written, then write our own char. */
    mk_ring_drain();
#endif

    if (at_line_start) {
#ifdef MULTIKERNEL_SHARED_POOL_PPTR
        uint32_t k1 = __atomic_load_n(&mk_ring->k1_seq, __ATOMIC_ACQUIRE);
        uart_console_putchar('[');
        uart_console_putchar('K');
        uart_console_putchar('0' + (unsigned char)aff0);
        uart_console_putchar('/');
        uart_console_putchar('k'); uart_console_putchar('1'); uart_console_putchar('=');
        put_dec_via(uart_put, k1);
        uart_console_putchar(']');
        uart_console_putchar(' ');
#else
        uart_console_putchar('[');
        uart_console_putchar('K');
        uart_console_putchar('0' + (unsigned char)aff0);
        uart_console_putchar(']');
        uart_console_putchar(' ');
#endif
        at_line_start = 0;
    }

    uart_console_putchar(c);

    if (c == '\n') {
#ifdef MULTIKERNEL_SHARED_POOL_PPTR
        uint32_t s = __atomic_load_n(&mk_ring->k0_seq, __ATOMIC_RELAXED);
        __atomic_store_n(&mk_ring->k0_seq, s + 1, __ATOMIC_RELEASE);
#endif
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
