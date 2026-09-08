#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: GPL-2.0-only
#

declare_platform(pc99 KernelPlatPC99 PLAT_PC99 KernelArchX86)

if(KernelPlatPC99)
    declare_seL4_arch(x86_64 ia32)
endif()

add_sources(
    DEP "KernelPlatPC99"
    PREFIX src/plat/pc99/machine
    CFILES
        acpi.c
        hardware.c
        pic.c
        ioapic.c
        pit.c
        io.c
        intel-vtd.c
)

add_bf_source_old(
    "KernelSel4ArchX86_64"
    "hardware.bf"
    "include/plat/pc99/plat/64"
    "plat_mode/machine"
)
add_bf_source_old(
    "KernelSel4ArchIA32"
    "hardware.bf"
    "include/plat/pc99/plat/32"
    "plat_mode/machine"
)

config_string(
    KernelPC99TSCFrequency PC99_TSC_FREQUENCY
    "Provide a static definition of the TSC frequency (in Hz). \
    If this isn't set then the boot code will try and read the frequency from a MSR. \
    If it can't calculate the frequency from a MSR then it will estimate it from running the PIT for about 200ms."
    DEFAULT 0
    DEPENDS "KernelPlatPC99"
    UNQUOTE UNDEF_DISABLED
)
set(KernelX86_64ELFPaddrBase "" CACHE STRING "Independent kernel ELF physical base")
if(KernelSel4ArchX86_64 AND NOT "${KernelX86_64ELFPaddrBase}" STREQUAL "")
    add_compile_definitions(KERNEL_ELF_PADDR_BASE=${KernelX86_64ELFPaddrBase}
                            KERNEL_ELF_PADDR_BASE_RAW=${KernelX86_64ELFPaddrBase})
endif()
option(KernelOptiplexIdle "Independent unicore hardware idle experiment" OFF)
if(KernelOptiplexIdle)
    if(NOT KernelSel4ArchX86_64 OR NOT KernelMaxNumNodes EQUAL 1)
        message(FATAL_ERROR "Optiplex idle requires x86_64 unicore kernels")
    endif()
    add_compile_definitions(CONFIG_OPTIPLEX_IDLE=1)
endif()
