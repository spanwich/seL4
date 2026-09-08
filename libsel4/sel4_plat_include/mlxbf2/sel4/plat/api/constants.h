/*
 * Copyright 2026, PhD Research Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sel4/config.h>

#if defined(CONFIG_ARM_CORTEX_A72)
#include <sel4/arch/constants_cortex_a72.h>
#else
#error "unsupported core"
#endif
