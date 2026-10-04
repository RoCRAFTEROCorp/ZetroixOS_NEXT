/*
 * PROJECT:     LiberNT PowerVR Rogue WDDM miniport
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     SpacemiT K1 GPU power domain, clock and reset
 * COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
 */

#include "powervr.h"

#define NDEBUG
#include <debug.h>

#define K1_APMU_BASE                0xD4282800ULL
#define K1_APMU_SIZE                0x400
#define K1_APMU_GPU_CLK_RES_CTRL    0x0CC
#define K1_APMU_GPU_PWR_CTRL        0x0D0
#define K1_APMU_PWR_STATUS          0x0F0

#define K1_GPU_PWR_ISOLATION        (1UL << 1)
#define K1_GPU_PWR_SLEEP1           (1UL << 2)
#define K1_GPU_PWR_SLEEP2           (1UL << 3)
#define K1_GPU_PWR_STATUS           (1UL << 0)

#define K1_GPU_CLK_RESET            (1UL << 1)
#define K1_GPU_CLK_ENABLE           (1UL << 4)
#define K1_GPU_CLK_DIV_SHIFT        12
#define K1_GPU_CLK_DIV_MASK         (7UL << K1_GPU_CLK_DIV_SHIFT)
#define K1_GPU_CLK_FC               (1UL << 15)
#define K1_GPU_CLK_MUX_SHIFT        18
#define K1_GPU_CLK_MUX_MASK         (7UL << K1_GPU_CLK_MUX_SHIFT)
#define K1_GPU_CLK_MUX_PLL1_D4      0

static ULONG
K1Read(_In_ PPOWERVR_ADAPTER Adapter, _In_ ULONG Offset)
{
    return READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)Adapter->PlatformRegisters + Offset));
}

static VOID
K1Write(_In_ PPOWERVR_ADAPTER Adapter, _In_ ULONG Offset, _In_ ULONG Value)
{
    WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)Adapter->PlatformRegisters + Offset), Value);
}

static BOOLEAN
K1WaitPowerStatus(_In_ PPOWERVR_ADAPTER Adapter)
{
    ULONG Loop;

    for (Loop = 0; Loop < 10000; ++Loop)
    {
        if (K1Read(Adapter, K1_APMU_PWR_STATUS) & K1_GPU_PWR_STATUS)
            return TRUE;
        KeStallExecutionProcessor(5);
    }
    return FALSE;
}

static NTSTATUS
K1PowerUp(_Inout_ PPOWERVR_ADAPTER Adapter)
{
    PHYSICAL_ADDRESS Base;
    ULONG Value, Loop;

    if (!Adapter->PlatformRegisters)
    {
        Base.QuadPart = K1_APMU_BASE;
        Adapter->PlatformRegisters = MmMapIoSpace(Base, K1_APMU_SIZE, MmNonCached);
        if (!Adapter->PlatformRegisters)
            return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (!(K1Read(Adapter, K1_APMU_PWR_STATUS) & K1_GPU_PWR_STATUS))
    {
        Value = K1Read(Adapter, K1_APMU_GPU_PWR_CTRL) | K1_GPU_PWR_SLEEP1;
        K1Write(Adapter, K1_APMU_GPU_PWR_CTRL, Value);
        KeStallExecutionProcessor(25);
        Value |= K1_GPU_PWR_SLEEP2;
        K1Write(Adapter, K1_APMU_GPU_PWR_CTRL, Value);
        KeStallExecutionProcessor(25);
        Value |= K1_GPU_PWR_ISOLATION;
        K1Write(Adapter, K1_APMU_GPU_PWR_CTRL, Value);
        KeStallExecutionProcessor(15);
        if (!K1WaitPowerStatus(Adapter))
        {
            DPRINT1("POWERVR: K1 GPU power domain did not come up (status 0x%08lx)\n",
                    K1Read(Adapter, K1_APMU_PWR_STATUS));
            return STATUS_DEVICE_POWER_FAILURE;
        }
    }

    Value = K1Read(Adapter, K1_APMU_GPU_CLK_RES_CTRL) | K1_GPU_CLK_RESET;
    K1Write(Adapter, K1_APMU_GPU_CLK_RES_CTRL, Value);
    KeStallExecutionProcessor(10);
    Value |= K1_GPU_CLK_ENABLE;
    K1Write(Adapter, K1_APMU_GPU_CLK_RES_CTRL, Value);
    KeStallExecutionProcessor(10);

    Value &= ~(K1_GPU_CLK_MUX_MASK | K1_GPU_CLK_DIV_MASK);
    Value |= K1_GPU_CLK_MUX_PLL1_D4 << K1_GPU_CLK_MUX_SHIFT;
    K1Write(Adapter, K1_APMU_GPU_CLK_RES_CTRL, Value);
    K1Write(Adapter, K1_APMU_GPU_CLK_RES_CTRL, Value | K1_GPU_CLK_FC);
    for (Loop = 0; Loop < 5000; ++Loop)
    {
        if (!(K1Read(Adapter, K1_APMU_GPU_CLK_RES_CTRL) & K1_GPU_CLK_FC))
            break;
        KeStallExecutionProcessor(1);
    }
    if (Loop == 5000)
    {
        DPRINT1("POWERVR: K1 GPU clock change did not complete (0x%08lx)\n",
                K1Read(Adapter, K1_APMU_GPU_CLK_RES_CTRL));
        return STATUS_DEVICE_POWER_FAILURE;
    }

    return STATUS_SUCCESS;
}

static VOID
K1PowerDown(_Inout_ PPOWERVR_ADAPTER Adapter)
{
    ULONG Value;

    if (!Adapter->PlatformRegisters)
        return;

    Value = K1Read(Adapter, K1_APMU_GPU_CLK_RES_CTRL);
    K1Write(Adapter, K1_APMU_GPU_CLK_RES_CTRL, Value & ~K1_GPU_CLK_RESET);
    KeStallExecutionProcessor(10);
    K1Write(Adapter, K1_APMU_GPU_CLK_RES_CTRL, Value & ~(K1_GPU_CLK_RESET | K1_GPU_CLK_ENABLE));

    Value = K1Read(Adapter, K1_APMU_GPU_PWR_CTRL);
    K1Write(Adapter, K1_APMU_GPU_PWR_CTRL, Value & ~K1_GPU_PWR_ISOLATION);
    KeStallExecutionProcessor(15);
    K1Write(Adapter, K1_APMU_GPU_PWR_CTRL, Value & ~(K1_GPU_PWR_ISOLATION | K1_GPU_PWR_SLEEP1 | K1_GPU_PWR_SLEEP2));
    KeStallExecutionProcessor(15);

    MmUnmapIoSpace(Adapter->PlatformRegisters, K1_APMU_SIZE);
    Adapter->PlatformRegisters = NULL;
}

static const char *const K1Compatible[] =
{
    "spacemit,k1-gpu",
    "img,img-bxe2-32",
    "img,img-rogue",
};

const POWERVR_PLATFORM PowerVrK1Platform =
{
    L"FDT\\spacemit_k1-gpu",
    "SpacemiT K1",
    614400000ULL,
    K1Compatible,
    RTL_NUMBER_OF(K1Compatible),
    K1PowerUp,
    K1PowerDown,
};
