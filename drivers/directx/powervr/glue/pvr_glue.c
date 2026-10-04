/*
 * PROJECT:     LiberNT PowerVR Rogue WDDM miniport
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Platform device, interrupt and DRM file plumbing for the imported PowerVR core
 * COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
 */

#include <linux_compat.h>
#include <drm_compat.h>

#include "pvr_device.h"
#include "powervr_glue.h"

struct pvr_glue_instance
{
    struct platform_device pdev;
    struct resource resources[2];
    struct device_node of_node;
    struct platform_driver *driver;
    unsigned int irq;
    u64 core_clock_hz;
    bool probed;
};

static const u32 pvr_glue_power_domain[] = { 0 };
static const struct lc_of_property pvr_glue_properties[] = {
    { "power-domains", pvr_glue_power_domain, ARRAY_SIZE(pvr_glue_power_domain), 1 },
};

int lc_module_init_drm_sched_fence_slab_init(void);
void lc_module_exit_drm_sched_fence_slab_fini(void);
void *lc_module_param_exp_hw_support(void);

static int pvr_glue_clk_enable(void *context)
{
    (void)context;
    return 0;
}

static void pvr_glue_clk_disable(void *context)
{
    (void)context;
}

static unsigned long pvr_glue_clk_rate(void *context)
{
    struct pvr_glue_instance *glue = context;

    return (unsigned long)glue->core_clock_hz;
}

static const struct lc_clk_ops pvr_glue_core_clk_ops = {
    .enable = pvr_glue_clk_enable,
    .disable = pvr_glue_clk_disable,
    .get_rate = pvr_glue_clk_rate,
};

int pvr_glue_driver_init(void)
{
    int err;

    err = lc_compat_init();
    if (err)
        return err;
    err = lc_module_init_drm_sched_fence_slab_init();
    if (err)
        lc_compat_exit();
    return err;
}

void pvr_glue_driver_exit(void)
{
    lc_module_exit_drm_sched_fence_slab_fini();
    lc_compat_exit();
}

int pvr_glue_probe(const struct pvr_glue_platform *platform, void **instance)
{
    struct pvr_glue_instance *glue;
    int irq, err;

    *instance = NULL;
    glue = kzalloc(sizeof(*glue), GFP_KERNEL);
    if (!glue)
        return -ENOMEM;

    lc_set_page_allocation_limit(platform->dma_limit);
    *(bool *)lc_module_param_exp_hw_support() = true;

    irq = lc_irq_register_line(platform->interrupt_vector, platform->interrupt_irql, platform->interrupt_level_sensitive);
    if (irq < 0)
    {
        kfree(glue);
        return irq;
    }
    glue->irq = (unsigned int)irq;
    glue->core_clock_hz = platform->core_clock_hz;
    glue->driver = lc_module_platform_driver();

    glue->resources[0].start = platform->registers_physical;
    glue->resources[0].end = platform->registers_physical + platform->registers_length - 1;
    glue->resources[0].flags = IORESOURCE_MEM;
    glue->resources[0].name = "gpu";
    glue->resources[1].start = (resource_size_t)irq;
    glue->resources[1].end = (resource_size_t)irq;
    glue->resources[1].flags = IORESOURCE_IRQ;
    glue->resources[1].name = "gpu";

    glue->of_node.compatible = platform->compatible;
    glue->of_node.compatible_count = platform->compatible_count;
    glue->of_node.name = "gpu";
    glue->of_node.properties = pvr_glue_properties;
    glue->of_node.property_count = ARRAY_SIZE(pvr_glue_properties);

    glue->pdev.name = "powervr";
    glue->pdev.id = -1;
    glue->pdev.resource = glue->resources;
    glue->pdev.num_resources = ARRAY_SIZE(glue->resources);
    lc_device_initialize(&glue->pdev.dev, "powervr");
    glue->pdev.dev.of_node = &glue->of_node;
    glue->pdev.dev.driver = &glue->driver->driver;
    if (!lc_device_add_clk(&glue->pdev.dev, "core", &pvr_glue_core_clk_ops, glue))
    {
        kfree(glue);
        return -ENOMEM;
    }

    err = glue->driver->probe(&glue->pdev);
    if (err)
    {
        dev_err(&glue->pdev.dev, "probe failed (%d)\n", err);
        lc_device_release_resources(&glue->pdev.dev);
        kfree(glue);
        return err;
    }
    glue->probed = true;
    *instance = glue;
    return 0;
}

void pvr_glue_remove(void *instance)
{
    struct pvr_glue_instance *glue = instance;

    if (!glue)
        return;
    if (glue->probed && glue->driver->remove)
        glue->driver->remove(&glue->pdev);
    lc_device_release_resources(&glue->pdev.dev);
    kfree(glue);
}

int pvr_glue_query(void *instance, struct pvr_glue_info *info)
{
    struct pvr_glue_instance *glue = instance;
    struct drm_device *drm;
    struct pvr_device *pvr_dev;

    if (!glue || !glue->probed)
        return -ENODEV;
    drm = platform_get_drvdata(&glue->pdev);
    pvr_dev = to_pvr_device(drm);
    info->branch = pvr_dev->gpu_id.b;
    info->version = pvr_dev->gpu_id.v;
    info->scalable_units = pvr_dev->gpu_id.n;
    info->config = pvr_dev->gpu_id.c;
    info->firmware_major = pvr_dev->fw_version.major;
    info->firmware_minor = pvr_dev->fw_version.minor;
    info->firmware_booted = READ_ONCE(pvr_dev->fw_dev.initialised) ? 1 : 0;
    return 0;
}

int pvr_glue_isr(void *instance, int *queue_dpc)
{
    struct pvr_glue_instance *glue = instance;
    bool dpc = false;
    bool handled;

    *queue_dpc = 0;
    if (!glue)
        return 0;
    handled = lc_irq_service(glue->irq, &dpc);
    *queue_dpc = dpc ? 1 : 0;
    return handled ? 1 : 0;
}

void pvr_glue_dpc(void *instance)
{
    struct pvr_glue_instance *glue = instance;

    if (glue)
        lc_irq_dpc(glue->irq);
}

int pvr_glue_open(void *instance, void **file)
{
    struct pvr_glue_instance *glue = instance;
    struct drm_file *drm_file;

    *file = NULL;
    if (!glue || !glue->probed)
        return -ENODEV;
    drm_file = lc_drm_file_open(platform_get_drvdata(&glue->pdev));
    if (IS_ERR(drm_file))
        return (int)PTR_ERR(drm_file);
    *file = drm_file;
    return 0;
}

void pvr_glue_close(void *file)
{
    if (file)
        lc_drm_file_close(file);
}

long pvr_glue_ioctl(void *file, uint32_t cmd, void *user_arg, uint32_t arg_size)
{
    if (!file)
        return -EBADF;
    return lc_drm_ioctl(file, cmd, user_arg, arg_size);
}

int pvr_glue_mmap(void *file, uint64_t offset, uint64_t size, uint64_t *user_address)
{
    if (!file)
        return -EBADF;
    return lc_drm_mmap(file, offset, size, user_address);
}

int pvr_glue_munmap(void *file, uint64_t user_address)
{
    if (!file)
        return -EBADF;
    return lc_drm_munmap(file, user_address);
}
