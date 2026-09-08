#
# Copyright 2026, PhD Research Project
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Platform: NVIDIA BlueField-2 DPU
# CPU: 8x Arm Cortex-A72
# Console: PL011/SBSA UART @ 0x01000000
#

declare_platform(mlxbf2 KernelPlatformMlxBF2 PLAT_MLXBF2 KernelSel4ArchAarch64)

if(KernelPlatformMlxBF2)
    declare_seL4_arch(aarch64)
    set(KernelArmCortexA72 ON)
    set(KernelArchArmV8a ON)
    set(KernelArmGicV3 ON)
    set(KernelAArch64SErrorIgnore ON)
    config_set(KernelARMPlatform ARM_PLAT "mlxbf2")
    list(APPEND KernelDTSList "tools/dts/mlxbf2.dts")
    list(APPEND KernelDTSList "src/plat/mlxbf2/overlay-mlxbf2.dts")
    if(DEFINED BF2LiveMemoryBase AND DEFINED BF2LiveMemorySize)
        math(EXPR BF2LiveMemoryBaseValue "${BF2LiveMemoryBase}")
        math(EXPR BF2LiveMemorySizeValue "${BF2LiveMemorySize}")
        math(EXPR BF2LiveMemoryBaseHi "(${BF2LiveMemoryBaseValue} >> 32) & 0xffffffff" OUTPUT_FORMAT HEXADECIMAL)
        math(EXPR BF2LiveMemoryBaseLo "${BF2LiveMemoryBaseValue} & 0xffffffff" OUTPUT_FORMAT HEXADECIMAL)
        math(EXPR BF2LiveMemorySizeHi "(${BF2LiveMemorySizeValue} >> 32) & 0xffffffff" OUTPUT_FORMAT HEXADECIMAL)
        math(EXPR BF2LiveMemorySizeLo "${BF2LiveMemorySizeValue} & 0xffffffff" OUTPUT_FORMAT HEXADECIMAL)
        math(EXPR BF2LiveMemoryNodeBase "${BF2LiveMemoryBaseValue}" OUTPUT_FORMAT HEXADECIMAL)
        if(NOT DEFINED BF2LiveKernelIndex)
            set(BF2LiveKernelIndex 0)
        endif()
        config_set(KernelBF2LiveKernelIndex BF2_LIVE_KERNEL_INDEX "${BF2LiveKernelIndex}")
        string(REPLACE "0x" "" BF2LiveMemoryNodeBaseName "${BF2LiveMemoryNodeBase}")
        set(BF2LiveMemoryOverlay "${CMAKE_BINARY_DIR}/bf2-live-memory.dts")
        if(BF2LiveMultikernel OR NOT "${BF2LiveKernelIndex}" STREQUAL "0")
            file(WRITE "${BF2LiveMemoryOverlay}"
"/ {
	/delete-node/ memory@100000000;
	memory@${BF2LiveMemoryNodeBaseName} {
		device_type = \"memory\";
		reg = <${BF2LiveMemoryBaseHi} ${BF2LiveMemoryBaseLo} ${BF2LiveMemorySizeHi} ${BF2LiveMemorySizeLo}>;
	};
	multikernel_shared_pool: shared-pool@87fff0000 {
		compatible = \"multikernel,shared-pool\";
		reg = <0x8 0x7fff0000 0x0 0x1000>;
	};
	chosen {
		seL4,bf2-live-kernel-index = <${BF2LiveKernelIndex}>;
		seL4,bf2-live-memory-base = <${BF2LiveMemoryBaseHi} ${BF2LiveMemoryBaseLo}>;
		seL4,bf2-live-memory-size = <${BF2LiveMemorySizeHi} ${BF2LiveMemorySizeLo}>;
		seL4,kernel-devices =
		    &{/tmfifo@800a20},
		    &{/interrupt-controller@12000000},
		    &{/timer},
		    &{/shared-pool@87fff0000};
	};
};
")
        else()
            file(WRITE "${BF2LiveMemoryOverlay}"
"/ {
	/delete-node/ memory@100000000;
	memory@${BF2LiveMemoryNodeBaseName} {
		device_type = \"memory\";
		reg = <${BF2LiveMemoryBaseHi} ${BF2LiveMemoryBaseLo} ${BF2LiveMemorySizeHi} ${BF2LiveMemorySizeLo}>;
	};
	chosen {
		seL4,bf2-live-kernel-index = <${BF2LiveKernelIndex}>;
		seL4,bf2-live-memory-base = <${BF2LiveMemoryBaseHi} ${BF2LiveMemoryBaseLo}>;
		seL4,bf2-live-memory-size = <${BF2LiveMemorySizeHi} ${BF2LiveMemorySizeLo}>;
	};
};
")
        endif()
        list(APPEND KernelDTSList "${BF2LiveMemoryOverlay}")
    endif()
    declare_default_headers(
        TIMER_FREQUENCY 200000000
        MAX_IRQ 991
        NUM_PPI 32
        KERNEL_WCET 10u
        TIMER drivers/timer/arm_generic.h
        INTERRUPT_CONTROLLER arch/machine/gic_v3.h
    )
endif()

add_sources(
    DEP "KernelPlatformMlxBF2"
    CFILES src/arch/arm/machine/gic_v3.c src/arch/arm/machine/l2c_nop.c
)
