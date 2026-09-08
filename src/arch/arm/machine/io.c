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

static word_t mk_read_kernel_index(void)
{
#ifdef CONFIG_BF2_LIVE_KERNEL_INDEX
    return (word_t)CONFIG_BF2_LIVE_KERNEL_INDEX;
#else
    word_t mpidr;
#ifdef CONFIG_ARCH_AARCH64
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
#else
    asm volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
#endif
    return mpidr & 0xff;  /* AFF0 only fallback. */
#endif
}

#ifdef MULTIKERNEL_SHARED_POOL_PPTR
/* Cross-kernel UART proxy rings.
 *
 * Lives in the shared 4 KiB device frame at MULTIKERNEL_SHARED_POOL_PPTR
 * (mapped to the same kernel virtual address in every kernel; backed by the
 * same physical page 0x87fff0000). Nonzero kernels enqueue UART chars into
 * their own per-kernel rings; K0 drains all rings to the real TMFIFO console.
 *
 * Each per-kernel ring is single-producer / single-consumer:
 *   - Kx writes data[head] then advances head (release).
 *   - K0 reads head (acquire), reads data[tail..head], advances tail.
 * This avoids multi-producer races when K1..K7 print at the same time.
 *
 * Offset 4 remains k0_ready because the secondary trampoline waits there before
 * entering seL4. K0 is the only TMFIFO owner.
 */
#define MK_RING_MAGIC   0x4d4b4c47u /* "MKLG" */
#define MK_MAX_KERNELS  8
#define MK_RING_DATA_SZ 480

struct mk_log_ring {
    uint32_t magic;                       /* offset 0: set by elfloader */
    uint32_t k0_ready;                    /* offset 4: trampoline gate */
    uint32_t head[MK_MAX_KERNELS];        /* producer advances */
    uint32_t tail[MK_MAX_KERNELS];        /* K0 advances */
    uint32_t dropped[MK_MAX_KERNELS];     /* producer drop count */
    uint32_t seq[MK_MAX_KERNELS];         /* per-kernel completed lines */
    uint8_t data[MK_MAX_KERNELS][MK_RING_DATA_SZ];
};
static volatile struct mk_log_ring *const mk_ring =
    (volatile struct mk_log_ring *)MULTIKERNEL_SHARED_POOL_PPTR;

/* Kernel is built -nostdinc so <stdatomic.h> isn't available. Use the GCC
 * builtins directly with explicit memory orders. ARMv8 LDAR/STLR map onto
 * __ATOMIC_ACQUIRE / __ATOMIC_RELEASE. */
static int mk_ring_ready(void)
{
    return __atomic_load_n(&mk_ring->magic, __ATOMIC_ACQUIRE) == MK_RING_MAGIC;
}

static void mk_ring_init(void)
{
    if (!mk_ring_ready()) {
        for (unsigned int i = 0; i < MK_MAX_KERNELS; i++) {
            __atomic_store_n(&mk_ring->head[i], 0, __ATOMIC_RELAXED);
            __atomic_store_n(&mk_ring->tail[i], 0, __ATOMIC_RELAXED);
            __atomic_store_n(&mk_ring->dropped[i], 0, __ATOMIC_RELAXED);
            __atomic_store_n(&mk_ring->seq[i], 0, __ATOMIC_RELAXED);
        }
        __atomic_store_n(&mk_ring->magic, MK_RING_MAGIC, __ATOMIC_RELEASE);
    }
    __atomic_store_n(&mk_ring->k0_ready, 1, __ATOMIC_RELEASE);
#ifdef CONFIG_ARCH_AARCH64
    asm volatile("sev" ::: "memory");
#endif
}

static void mk_ring_push(word_t idx, unsigned char c)
{
    if (idx >= MK_MAX_KERNELS) {
        idx = MK_MAX_KERNELS - 1;
    }
    if (!mk_ring_ready()) {
        uint32_t d = __atomic_load_n(&mk_ring->dropped[idx], __ATOMIC_RELAXED);
        __atomic_store_n(&mk_ring->dropped[idx], d + 1, __ATOMIC_RELAXED);
        return;
    }

    uint32_t h = __atomic_load_n(&mk_ring->head[idx], __ATOMIC_RELAXED);
    uint32_t t = __atomic_load_n(&mk_ring->tail[idx], __ATOMIC_ACQUIRE);
    uint32_t next_h = h + 1;
    /* Drop on full; kernel logging must not block. */
    if (next_h - t > MK_RING_DATA_SZ) {
        uint32_t d = __atomic_load_n(&mk_ring->dropped[idx], __ATOMIC_RELAXED);
        __atomic_store_n(&mk_ring->dropped[idx], d + 1, __ATOMIC_RELAXED);
        return;
    }
    mk_ring->data[idx][h % MK_RING_DATA_SZ] = c;
    __atomic_store_n(&mk_ring->head[idx], next_h, __ATOMIC_RELEASE);
}

static void mk_ring_drain(void)
{
    if (!mk_ring_ready()) {
        mk_ring_init();
        return;
    }

    for (word_t idx = 1; idx < MK_MAX_KERNELS; idx++) {
        uint32_t h = __atomic_load_n(&mk_ring->head[idx], __ATOMIC_ACQUIRE);
        uint32_t t = __atomic_load_n(&mk_ring->tail[idx], __ATOMIC_RELAXED);
        while (t != h) {
            uart_console_putchar(mk_ring->data[idx][t % MK_RING_DATA_SZ]);
            t++;
        }
        __atomic_store_n(&mk_ring->tail[idx], t, __ATOMIC_RELEASE);
    }
}

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
static word_t current_ring_index;
static void ring_put(unsigned char c) { mk_ring_push(current_ring_index, c); }
#endif /* MULTIKERNEL_SHARED_POOL_PPTR */

void kernel_putDebugChar(unsigned char c)
{
    static int at_line_start = 1;
    word_t kidx = mk_read_kernel_index();

#ifdef MULTIKERNEL_SHARED_POOL_PPTR
    /* K1..K7: forward to shared rings instead of touching TMFIFO. */
    if (kidx != 0) {
        current_ring_index = kidx;
        if (at_line_start) {
            uint32_t k0 = __atomic_load_n(&mk_ring->seq[0], __ATOMIC_ACQUIRE);
            mk_ring_push(kidx, '[');
            mk_ring_push(kidx, 'K');
            mk_ring_push(kidx, '0' + (unsigned char)kidx);
            mk_ring_push(kidx, '/');
            mk_ring_push(kidx, 'k'); mk_ring_push(kidx, '0'); mk_ring_push(kidx, '=');
            put_dec_via(ring_put, k0);
            mk_ring_push(kidx, ']');
            mk_ring_push(kidx, ' ');
            at_line_start = 0;
        }
        mk_ring_push(kidx, c);
        if (c == '\n') {
            uint32_t s = __atomic_load_n(&mk_ring->seq[kidx], __ATOMIC_RELAXED);
            __atomic_store_n(&mk_ring->seq[kidx], s + 1, __ATOMIC_RELEASE);
            at_line_start = 1;
        }
        return;
    }
    /* K0: initialize/drain dataport, then write the only real TMFIFO char. */
    mk_ring_init();
    mk_ring_drain();
#endif

    if (at_line_start) {
#ifdef MULTIKERNEL_SHARED_POOL_PPTR
        uint32_t other = 0;
        for (word_t idx = 1; idx < MK_MAX_KERNELS; idx++) {
            other += __atomic_load_n(&mk_ring->seq[idx], __ATOMIC_ACQUIRE);
        }
        uart_console_putchar('[');
        uart_console_putchar('K');
        uart_console_putchar('0' + (unsigned char)kidx);
        uart_console_putchar('/');
        uart_console_putchar('s'); uart_console_putchar('=');
        put_dec_via(uart_put, other);
        uart_console_putchar(']');
        uart_console_putchar(' ');
#else
        uart_console_putchar('[');
        uart_console_putchar('K');
        uart_console_putchar('0' + (unsigned char)kidx);
        uart_console_putchar(']');
        uart_console_putchar(' ');
#endif
        at_line_start = 0;
    }

    uart_console_putchar(c);

    if (c == '\n') {
#ifdef MULTIKERNEL_SHARED_POOL_PPTR
        uint32_t s = __atomic_load_n(&mk_ring->seq[0], __ATOMIC_RELAXED);
        __atomic_store_n(&mk_ring->seq[0], s + 1, __ATOMIC_RELEASE);
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
