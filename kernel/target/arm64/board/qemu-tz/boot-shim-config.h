// Copyright 2018 Open Trust Group
// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define HAS_DEVICE_TREE 0
#define USE_DEVICE_TREE_CPU_COUNT 1

static bootdata_cpu_config_t cpu_config = {
    .cluster_count = 1,
    .clusters = {
        {
            .cpu_count = 1,
        },
    },
};

static const bootdata_mem_range_t mem_config[] = {
    {
        .type = BOOTDATA_MEM_RANGE_RAM,
        .paddr = 0x0e100000,
        .length = 0x01f00000,
    },
    {
        .type = BOOTDATA_MEM_RANGE_PERIPHERAL,
        .paddr = 0,
        .length = 0x40000000,
    },
};

static const dcfg_simple_t uart_driver = {
    .mmio_phys = 0x09040000,
    .irq = 40,
};

static const dcfg_arm_gicv3_driver_t gicv3_driver = {
    .mmio_phys = 0x08000000,
    .gicd_offset = 0x00000,
    .gicr_offset = 0xa0000,
    .gicr_stride = 0x20000,
    .ipi_base = 13,
    .optional = true,
};

static const dcfg_arm_gicv2_driver_t gicv2_driver = {
    .mmio_phys = 0x08000000,
    .msi_frame_phys = 0x08020000,
    .gicd_offset = 0x00000,
    .gicc_offset = 0x10000,
    .ipi_base = 13,
    .optional = true,
    .use_msi = true,
};

static const dcfg_arm_psci_driver_t psci_driver = {
    .use_hvc = false,
};

static const dcfg_arm_generic_timer_driver_t timer_driver = {
    .irq_sphys = 29,
};

// non-secure share memory chosen arbitrary, in a way that it does not interfere
// with initial location of linux kernel, dtb and initrd
static const dcfg_sm_ns_shm_t ns_shm_config = {
    .base_phys = 0x40000000 + 0x2000000,
    .length = 0x500000,
    .use_cache = true,
};

static const bootdata_platform_id_t platform_id = {
    .vid = PDEV_VID_QEMU,
    .pid = PDEV_PID_QEMU,
    .board_name = "qemu-tz",
};

static void append_board_bootdata(bootdata_t* bootdata) {
    // add CPU configuration
    append_bootdata(bootdata, BOOTDATA_CPU_CONFIG, 0, &cpu_config,
                    sizeof(bootdata_cpu_config_t) +
                    sizeof(bootdata_cpu_cluster_t) * cpu_config.cluster_count);

    // add memory configuration
    append_bootdata(bootdata, BOOTDATA_MEM_CONFIG, 0, &mem_config,
                    sizeof(bootdata_mem_range_t) * countof(mem_config));

    // add kernel drivers
    append_bootdata(bootdata, BOOTDATA_KERNEL_DRIVER, KDRV_PL011_UART, &uart_driver,
                    sizeof(uart_driver));
    append_bootdata(bootdata, BOOTDATA_KERNEL_DRIVER, KDRV_ARM_GIC_V3, &gicv3_driver,
                    sizeof(gicv3_driver));
    append_bootdata(bootdata, BOOTDATA_KERNEL_DRIVER, KDRV_ARM_GIC_V2, &gicv2_driver,
                    sizeof(gicv2_driver));
    append_bootdata(bootdata, BOOTDATA_KERNEL_DRIVER, KDRV_ARM_PSCI, &psci_driver,
                    sizeof(psci_driver));
    append_bootdata(bootdata, BOOTDATA_KERNEL_DRIVER, KDRV_ARM_GENERIC_TIMER, &timer_driver,
                    sizeof(timer_driver));
    append_bootdata(bootdata, BOOTDATA_KERNEL_DRIVER, KDRV_SM_NS_SHM, &ns_shm_config,
                    sizeof(ns_shm_config));

    // add platform ID
    append_bootdata(bootdata, BOOTDATA_PLATFORM_ID, 0, &platform_id, sizeof(platform_id));
}

static void set_cpu_count(uint32_t cpu_count) {
    if (cpu_count > 0) {
        cpu_config.clusters[0].cpu_count = cpu_count;
    }
}
