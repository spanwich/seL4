/*
 * Copyright 2025, PhD Research Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Platform constants for Digi CCMP25-DVK (STM32MP255, dual Cortex-A35)
 */

#pragma once

#include <sel4/config.h>
#include <sel4/arch/constants_cortex_a35.h>

#ifdef CONFIG_ARCH_AARCH32
#error "AARCH32 is unsupported for stm32mp25x"
#endif /* CONFIG_ARCH_AARCH32 */
