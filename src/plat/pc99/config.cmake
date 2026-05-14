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

# multikernel-AMP: override the kernel ELF physical/virtual load address so K0
# and K1 can coexist at distinct paddrs (default 0x00100000 = 1 MiB).
# Constraint: must be in [0, 0x40000000) — KERNEL_ELF_BASE must land in PDPT
# slot 510, asserted by src/arch/x86/64/kernel/vspace.c. The value is injected
# as a compile definition so both the header (hardware.h:90) and the linker
# script (src/plat/pc99/linker.lds) pick it up.
set(KernelX86_64ELFPaddrBase "" CACHE STRING
    "Override KERNEL_ELF_PADDR_BASE for x86_64 multikernel-AMP builds. \
    Must be in [0, 0x40000000). Leave empty to use the default 0x00100000.")
if(KernelSel4ArchX86_64 AND NOT "${KernelX86_64ELFPaddrBase}" STREQUAL "")
    add_compile_definitions(KERNEL_ELF_PADDR_BASE=${KernelX86_64ELFPaddrBase})
    add_compile_definitions(KERNEL_ELF_PADDR_BASE_RAW=${KernelX86_64ELFPaddrBase})
endif()
