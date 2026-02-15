#
# Copyright 2025, PhD Research Project
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Platform: Digi ConnectCore MP25 DVK (STM32MP255, dual Cortex-A35)
# GIC: GICv2 @ 0x4ac10000 (GICD) / 0x4ac20000 (GICC)
# UART: USART2 @ 0x400e0000 (st,stm32h7-uart)
# Timer: ARM generic timer @ 40 MHz
#

declare_platform(stm32mp25x KernelPlatformSTM32MP25x PLAT_STM32MP25X KernelArchARM)

if(KernelPlatformSTM32MP25x)
    declare_seL4_arch(aarch64)
    set(KernelArmCortexA35 ON)
    set(KernelArchArmV8a ON)
    set(KernelArmGicV2 ON)
    config_set(KernelARMPlatform ARM_PLAT ${KernelPlatform})
    list(APPEND KernelDTSList "tools/dts/${KernelPlatform}.dts")
    list(APPEND KernelDTSList "src/plat/stm32mp25x/overlay-${KernelPlatform}.dts")
    declare_default_headers(
        TIMER_FREQUENCY 40000000
        MAX_IRQ 511
        TIMER drivers/timer/arm_generic.h
        INTERRUPT_CONTROLLER arch/machine/gic_v2.h
        NUM_PPI 32
        CLK_MAGIC 1llu
        CLK_SHIFT 3u
        KERNEL_WCET 10u
    )
endif()

add_sources(
    DEP "KernelPlatformSTM32MP25x"
    CFILES src/arch/arm/machine/gic_v2.c src/arch/arm/machine/l2c_nop.c
)
