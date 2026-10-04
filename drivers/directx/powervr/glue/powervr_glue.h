/*
 * PROJECT:     LiberNT PowerVR Rogue WDDM miniport
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Interface between the WDDM miniport and the imported PowerVR core
 * COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
 */

#pragma once

#include <stdint.h>

struct pvr_glue_platform
{
    uint64_t registers_physical;
    uint64_t registers_length;
    uint32_t interrupt_vector;
    uint8_t interrupt_irql;
    uint8_t interrupt_level_sensitive;
    uint64_t core_clock_hz;
    uint64_t dma_limit;
    const char *const *compatible;
    unsigned int compatible_count;
};

struct pvr_glue_info
{
    uint16_t branch;
    uint16_t version;
    uint16_t scalable_units;
    uint16_t config;
    uint32_t firmware_major;
    uint32_t firmware_minor;
    uint32_t firmware_booted;
};

int pvr_glue_driver_init(void);
void pvr_glue_driver_exit(void);
int pvr_glue_probe(const struct pvr_glue_platform *platform, void **instance);
void pvr_glue_remove(void *instance);
int pvr_glue_query(void *instance, struct pvr_glue_info *info);
int pvr_glue_isr(void *instance, int *queue_dpc);
void pvr_glue_dpc(void *instance);
int pvr_glue_open(void *instance, void **file);
void pvr_glue_close(void *file);
long pvr_glue_ioctl(void *file, uint32_t cmd, void *user_arg, uint32_t arg_size);
int pvr_glue_mmap(void *file, uint64_t offset, uint64_t size, uint64_t *user_address);
int pvr_glue_munmap(void *file, uint64_t user_address);
