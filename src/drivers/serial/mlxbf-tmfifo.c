/*
 * Copyright 2026, PhD Research Project
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <config.h>
#include <stdint.h>
#include <plat/machine/devices_gen.h>

#ifdef CONFIG_PRINTING
#define MLXBF_TMFIFO_TX_DATA       0x00u
#define MLXBF_TMFIFO_TX_STS        0x08u
#define MLXBF_TMFIFO_TX_CTL        0x10u
#define MLXBF_TMFIFO_TX_STS_COUNT  0x1ffu
#define MLXBF_TMFIFO_TX_CTL_MAX_ENTRIES_SHIFT 32u
#define MLXBF_TMFIFO_TX_CTL_MAX_ENTRIES_MASK  0x1ffu
#define MLXBF_TMFIFO_MIN_TX_WORDS  2u
#define MLXBF_TMFIFO_WAIT_LIMIT    10000000u
#define MLXBF_TMFIFO_VIRTIO_ID_CONSOLE 3u

#define TMFIFO_TX_REG64(x) ((volatile uint64_t *)(TMFIFO_TX_PPTR + (x)))

static uint64_t mlxbf_tmfifo_tx_read64(uint32_t offset)
{
    return *TMFIFO_TX_REG64(offset);
}

static void mlxbf_tmfifo_tx_write64(uint32_t offset, uint64_t value)
{
    *TMFIFO_TX_REG64(offset) = value;
}

static uint32_t mlxbf_tmfifo_tx_fifo_size(void)
{
    uint64_t ctl = mlxbf_tmfifo_tx_read64(MLXBF_TMFIFO_TX_CTL);
    uint32_t size = (uint32_t)((ctl >> MLXBF_TMFIFO_TX_CTL_MAX_ENTRIES_SHIFT) &
                               MLXBF_TMFIFO_TX_CTL_MAX_ENTRIES_MASK);

    return size ? size : 256u;
}

static uint32_t mlxbf_tmfifo_tx_avail(void)
{
    uint32_t size = mlxbf_tmfifo_tx_fifo_size();
    uint64_t sts = mlxbf_tmfifo_tx_read64(MLXBF_TMFIFO_TX_STS);
    uint32_t used = (uint32_t)(sts & MLXBF_TMFIFO_TX_STS_COUNT);

    if (used >= size - 1u) {
        return 0;
    }

    return size - 1u - used;
}

void uart_drv_putchar(unsigned char c)
{
    uint64_t header;
    uint64_t payload = c;
    uint32_t wait;

    for (wait = 0; wait < MLXBF_TMFIFO_WAIT_LIMIT; wait++) {
        if (mlxbf_tmfifo_tx_avail() >= MLXBF_TMFIFO_MIN_TX_WORDS) {
            break;
        }
    }

    if (wait == MLXBF_TMFIFO_WAIT_LIMIT) {
        return;
    }

    /* Linux mlxbf_tmfifo_msg_hdr: type byte, big-endian u16 len, five zero bytes. */
    header = MLXBF_TMFIFO_VIRTIO_ID_CONSOLE | (1ull << 16);
    mlxbf_tmfifo_tx_write64(MLXBF_TMFIFO_TX_DATA, header);
    mlxbf_tmfifo_tx_write64(MLXBF_TMFIFO_TX_DATA, payload);
}
#endif /* CONFIG_PRINTING */

#ifdef CONFIG_DEBUG_BUILD
unsigned char uart_drv_getchar(void)
{
    return 0;
}
#endif /* CONFIG_DEBUG_BUILD */
