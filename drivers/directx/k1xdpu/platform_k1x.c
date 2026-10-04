/*
 * PROJECT:     LiberNT SpacemiT K1 display miniport
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     K1 DPU and HDMI platform provider for the software GPU engine
 * COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
 */

#include "softgpu.h"
#include "softgpu_2d_core.h"

#define K1XDPU_TAG                       'pDiK'
#define K1XDPU_WORKING_SURFACE_COUNT     12UL

#define K1X_HDMI_BASE                    0xC0400500ULL
#define K1X_HDMI_SIZE                    0x200
#define K1X_HDMI_DDC_TX                  0x00
#define K1X_HDMI_DDC_RX                  0x04
#define K1X_HDMI_DDC_COMMAND             0x08
#define K1X_HDMI_STATUS                  0x0C
#define K1X_HDMI_STATUS_RX_COUNT_SHIFT   4
#define K1X_HDMI_STATUS_RX_COUNT_MASK    0x1F0
#define K1X_HDMI_STATUS_HPD              (1UL << 12)
#define K1X_HDMI_STATUS_DDC_DONE         (1UL << 14)
#define K1X_HDMI_STATUS_DDC_NACK         (1UL << 15)
#define K1X_HDMI_STATUS_DDC_LOST         (1UL << 16)
#define K1X_HDMI_DDC_CHUNK               16
#define K1X_EDID_ADDRESS                 0x50
#define K1X_EDID_BLOCK                   128
#define K1X_EDID_MAX                     256
#define K1X_DDC_TIMEOUT_US               50000

#define K1X_DPU_INT_OFFSET               0x900
#define K1X_DPU_INT_SIZE                 0x100
#define K1X_DPU_INT_ONLINE2_STATUS       0x38
#define K1X_DPU_INT_ONLINE2_RAW          0x60
#define K1X_DPU_INT_VSYNC                (1UL << 0)
#define K1X_DPU_VSYNC_TIMEOUT_US         50000
#define K1X_DPU_POLL_US                  20

typedef struct _K1XDPU_CONTEXT
{
    PUCHAR Hdmi;
    PUCHAR DpuInterrupts;
    ULONG EdidLength;
    UCHAR Edid[K1X_EDID_MAX];
} K1XDPU_CONTEXT, *PK1XDPU_CONTEXT;

static ULONG
K1xRead(_In_ PUCHAR Base, _In_ ULONG Offset)
{
    return READ_REGISTER_ULONG((PULONG)(Base + Offset));
}

static VOID
K1xWrite(_In_ PUCHAR Base, _In_ ULONG Offset, _In_ ULONG Value)
{
    WRITE_REGISTER_ULONG((PULONG)(Base + Offset), Value);
}

static NTSTATUS
K1xDdcFinish(_In_ PUCHAR Hdmi)
{
    ULONG Elapsed, Status;

    for (Elapsed = 0; Elapsed < K1X_DDC_TIMEOUT_US; Elapsed += K1X_DPU_POLL_US)
    {
        Status = K1xRead(Hdmi, K1X_HDMI_STATUS);
        if (Status & K1X_HDMI_STATUS_DDC_DONE)
        {
            K1xWrite(Hdmi, K1X_HDMI_STATUS, Status | K1X_HDMI_STATUS_DDC_DONE);
            return (Status & (K1X_HDMI_STATUS_DDC_NACK | K1X_HDMI_STATUS_DDC_LOST)) ?
                       STATUS_DEVICE_PROTOCOL_ERROR : STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(K1X_DPU_POLL_US);
    }
    return STATUS_IO_TIMEOUT;
}

static NTSTATUS
K1xDdcReadEdid(_In_ PUCHAR Hdmi, _In_ UCHAR Offset, _Out_writes_bytes_(Length) PUCHAR Buffer, _In_ ULONG Length)
{
    ULONG Done = 0, Count, Index, Elapsed, Status;
    NTSTATUS Result;

    if (!(K1xRead(Hdmi, K1X_HDMI_STATUS) & K1X_HDMI_STATUS_HPD))
        return STATUS_DEVICE_NOT_CONNECTED;

    K1xWrite(Hdmi, K1X_HDMI_DDC_TX, Offset);
    K1xWrite(Hdmi, K1X_HDMI_DDC_COMMAND, K1X_EDID_ADDRESS << 1);
    Result = K1xDdcFinish(Hdmi);
    if (!NT_SUCCESS(Result))
        return Result;

    while (Done < Length)
    {
        Count = min(Length - Done, (ULONG)K1X_HDMI_DDC_CHUNK);
        K1xWrite(Hdmi, K1X_HDMI_DDC_COMMAND, ((Count - 1) << 8) | (K1X_EDID_ADDRESS << 1) | 1);
        for (Elapsed = 0; ; Elapsed += K1X_DPU_POLL_US)
        {
            Status = K1xRead(Hdmi, K1X_HDMI_STATUS);
            if (!(Status & K1X_HDMI_STATUS_HPD))
                return STATUS_DEVICE_NOT_CONNECTED;
            if (((Status & K1X_HDMI_STATUS_RX_COUNT_MASK) >> K1X_HDMI_STATUS_RX_COUNT_SHIFT) >= Count)
                break;
            if (Elapsed >= K1X_DDC_TIMEOUT_US)
                return STATUS_IO_TIMEOUT;
            KeStallExecutionProcessor(K1X_DPU_POLL_US);
        }
        for (Index = 0; Index < Count; ++Index)
            Buffer[Done + Index] = (UCHAR)K1xRead(Hdmi, K1X_HDMI_DDC_RX);
        Result = K1xDdcFinish(Hdmi);
        if (!NT_SUCCESS(Result))
            return Result;
        Done += Count;
    }
    return STATUS_SUCCESS;
}

static BOOLEAN
K1xEdidBlockValid(_In_reads_bytes_(K1X_EDID_BLOCK) const UCHAR *Block, _In_ BOOLEAN Base)
{
    static const UCHAR Header[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    UCHAR Sum = 0;
    ULONG Index;

    if (Base && !RtlEqualMemory(Block, Header, sizeof(Header)))
        return FALSE;
    for (Index = 0; Index < K1X_EDID_BLOCK; ++Index)
        Sum = (UCHAR)(Sum + Block[Index]);
    return Sum == 0;
}

static VOID
K1xReadEdid(_Inout_ PK1XDPU_CONTEXT Context)
{
    const UCHAR *Timing;
    NTSTATUS Status;

    Context->EdidLength = 0;
    Status = K1xDdcReadEdid(Context->Hdmi, 0, Context->Edid, K1X_EDID_BLOCK);
    if (!NT_SUCCESS(Status) || !K1xEdidBlockValid(Context->Edid, TRUE))
    {
        DPRINT1("K1XDPU: EDID unavailable (0x%08lx)\n", Status);
        return;
    }
    Context->EdidLength = K1X_EDID_BLOCK;
    if (Context->Edid[126] &&
        NT_SUCCESS(K1xDdcReadEdid(Context->Hdmi, K1X_EDID_BLOCK, Context->Edid + K1X_EDID_BLOCK, K1X_EDID_BLOCK)) &&
        K1xEdidBlockValid(Context->Edid + K1X_EDID_BLOCK, FALSE))
    {
        Context->EdidLength = K1X_EDID_MAX;
    }

    Timing = Context->Edid + 54;
    DPRINT1("K1XDPU: EDID %c%c%c%04x, %lu block(s), preferred %ux%u at %lu kHz\n",
            ((Context->Edid[8] >> 2) & 0x1F) + '@',
            (((Context->Edid[8] & 3) << 3) | (Context->Edid[9] >> 5)) + '@',
            (Context->Edid[9] & 0x1F) + '@',
            Context->Edid[10] | (Context->Edid[11] << 8),
            Context->EdidLength / K1X_EDID_BLOCK,
            Timing[2] | ((Timing[4] & 0xF0) << 4),
            Timing[5] | ((Timing[7] & 0xF0) << 4),
            (ULONG)(Timing[0] | (Timing[1] << 8)) * 10);
}

static PUCHAR
K1xMapDpuInterrupts(_In_ PDXGK_INTERFACE DxgkInterface)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor, Dpu = NULL;
    DXGK_DEVICE_INFO Information;
    PHYSICAL_ADDRESS Address;
    ULONG Index;

    if (!DxgkInterface->DxgkCbGetDeviceInformation ||
        !NT_SUCCESS(DxgkInterface->DxgkCbGetDeviceInformation(DxgkInterface->DeviceHandle, &Information)) ||
        !Information.TranslatedResourceList || !Information.TranslatedResourceList->Count)
    {
        return NULL;
    }
    for (Index = 0; Index < Information.TranslatedResourceList->List[0].PartialResourceList.Count; ++Index)
    {
        Descriptor = &Information.TranslatedResourceList->List[0].PartialResourceList.PartialDescriptors[Index];
        if (Descriptor->Type == CmResourceTypeMemory &&
            Descriptor->u.Memory.Length >= K1X_DPU_INT_OFFSET + K1X_DPU_INT_SIZE &&
            (!Dpu || Descriptor->u.Memory.Start.QuadPart > Dpu->u.Memory.Start.QuadPart))
        {
            Dpu = Descriptor;
        }
    }
    if (!Dpu)
        return NULL;
    Address.QuadPart = Dpu->u.Memory.Start.QuadPart + K1X_DPU_INT_OFFSET;
    return MmMapIoSpace(Address, K1X_DPU_INT_SIZE, MmNonCached);
}

static BOOLEAN
K1xWaitVsync(_In_ PUCHAR DpuInterrupts)
{
    ULONG Elapsed;

    K1xWrite(DpuInterrupts, K1X_DPU_INT_ONLINE2_STATUS, K1X_DPU_INT_VSYNC);
    for (Elapsed = 0; Elapsed < K1X_DPU_VSYNC_TIMEOUT_US; Elapsed += K1X_DPU_POLL_US)
    {
        if (K1xRead(DpuInterrupts, K1X_DPU_INT_ONLINE2_RAW) & K1X_DPU_INT_VSYNC)
            return TRUE;
        KeStallExecutionProcessor(K1X_DPU_POLL_US);
    }
    return FALSE;
}

NTSTATUS
SoftGpuPlatformValidatePdo(
    _In_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    return PhysicalDeviceObject != NULL ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

NTSTATUS
SoftGpuPlatformQueryStart(
    _In_ PSOFTGPU_DEVICE Device,
    _In_ PDXGK_INTERFACE DxgkInterface,
    _Out_ PSOFTGPU_PLATFORM_CONFIG Config)
{
    DXGK_DISPLAY_INFORMATION PostDisplayInfo;
    ULONGLONG PostVisibleLength;
    PHYSICAL_ADDRESS Address;
    PK1XDPU_CONTEXT Context;
    SIZE_T WorkingSetSize;
    NTSTATUS Status;

    if (Device == NULL || DxgkInterface == NULL || Config == NULL)
        return STATUS_INVALID_PARAMETER;

    RtlZeroMemory(Config, sizeof(*Config));
    Config->Width = SOFTGPU_DEFAULT_WIDTH;
    Config->Height = SOFTGPU_DEFAULT_HEIGHT;
    Config->Format = SOFTGPU_DEFAULT_FORMAT;

    Status = SoftGpuAcquirePostDisplay(DxgkInterface, &PostDisplayInfo, &PostVisibleLength);
    if (!NT_SUCCESS(Status))
        return Status;
    if (PostDisplayInfo.Width != 0)
    {
        Config->Width = PostDisplayInfo.Width;
        Config->Height = PostDisplayInfo.Height;
        Config->Format = PostDisplayInfo.ColorFormat;
        Config->ScanoutPhysicalAddress = PostDisplayInfo.PhysicAddress;
        Config->ScanoutPitch = PostDisplayInfo.Pitch;
        Config->ScanoutSize = PostVisibleLength;
    }

    Status = SoftGpu2dComputeAllocationSlabSize(Config->Width,
                                                Config->Height,
                                                K1XDPU_WORKING_SURFACE_COUNT,
                                                MAXULONG_PTR,
                                                &WorkingSetSize);
    if (!NT_SUCCESS(Status))
        return Status;
    Config->MinimumAllocationSlabSize = min(WorkingSetSize, SOFTGPU_MAX_ALLOCATION_SLAB_SIZE);

    if (Device->PlatformContext)
        return STATUS_SUCCESS;
    Context = ExAllocatePoolZero(NonPagedPool, sizeof(*Context), K1XDPU_TAG);
    if (!Context)
        return STATUS_INSUFFICIENT_RESOURCES;
    Address.QuadPart = K1X_HDMI_BASE;
    Context->Hdmi = MmMapIoSpace(Address, K1X_HDMI_SIZE, MmNonCached);
    Context->DpuInterrupts = K1xMapDpuInterrupts(DxgkInterface);
    if (Context->Hdmi)
        K1xReadEdid(Context);
    Device->PlatformContext = Context;
    return STATUS_SUCCESS;
}

ULONG
SoftGpuPlatformDmaBufferPrivateDataSize(VOID)
{
    return 0;
}

ULONG
SoftGpuPlatformDmaBufferSegmentSet(VOID)
{
    return 0;
}

NTSTATUS
SoftGpuPlatformStartScanout(
    _Inout_ PSOFTGPU_DEVICE Device)
{
    /* FIXME: program the DPU (RDMA layer, compositor, output timing) and the
     * HDMI PHY/PLL for the committed mode instead of reusing U-Boot's setup. */
    UNREFERENCED_PARAMETER(Device);
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
SoftGpuPlatformStopScanout(
    _Inout_ PSOFTGPU_DEVICE Device)
{
    PK1XDPU_CONTEXT Context;

    if (Device == NULL || Device->PlatformContext == NULL)
        return STATUS_SUCCESS;
    Context = Device->PlatformContext;
    Device->PlatformContext = NULL;
    InterlockedExchange(&Device->ScanoutVBlankAvailable, 0);
    if (Context->DpuInterrupts)
        MmUnmapIoSpace(Context->DpuInterrupts, K1X_DPU_INT_SIZE);
    if (Context->Hdmi)
        MmUnmapIoSpace(Context->Hdmi, K1X_HDMI_SIZE);
    ExFreePoolWithTag(Context, K1XDPU_TAG);
    return STATUS_SUCCESS;
}

NTSTATUS
SoftGpuPlatformSetPrimary(
    _Inout_ PSOFTGPU_DEVICE Device,
    _In_ PHYSICAL_ADDRESS PrimaryAddress,
    _In_ ULONG Pitch,
    _In_ ULONG Width,
    _In_ ULONG Height,
    _In_ BOOLEAN Visible)
{
    /* FIXME: direct scanout needs the DPU layer address and cache maintenance
     * for the non-coherent DPU reads. */
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PrimaryAddress);
    UNREFERENCED_PARAMETER(Pitch);
    UNREFERENCED_PARAMETER(Width);
    UNREFERENCED_PARAMETER(Height);
    UNREFERENCED_PARAMETER(Visible);
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
SoftGpuPlatformPresentDisplayOnly(
    _Inout_ PSOFTGPU_DEVICE Device,
    _In_ const DXGKARG_PRESENT_DISPLAYONLY *PresentDisplayOnly)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PresentDisplayOnly);
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
SoftGpuPlatformUpdatePointer(
    _Inout_ PSOFTGPU_DEVICE Device)
{
    UNREFERENCED_PARAMETER(Device);
    return STATUS_NOT_SUPPORTED;
}

VOID
SoftGpuPlatformFillNodeMetadata(
    _In_ ULONG NodeOrdinal,
    _Out_ DXGKARG_GETNODEMETADATA *GetNodeMetadata)
{
    static const WCHAR FriendlyName[] = L"3D";

    UNREFERENCED_PARAMETER(NodeOrdinal);
    GetNodeMetadata->EngineType = DXGK_ENGINE_TYPE_3D;
    RtlCopyMemory(GetNodeMetadata->FriendlyName, FriendlyName, sizeof(FriendlyName));
}

VOID
SoftGpuPlatformInitializeTiming(
    _Inout_ PSOFTGPU_DEVICE Device)
{
    PK1XDPU_CONTEXT Context;
    LARGE_INTEGER Start, End, Frequency;
    ULONGLONG PeriodUs;

    if (Device == NULL)
        return;
    InterlockedExchange(&Device->ScanoutVBlankAvailable, 0);
    Context = Device->PlatformContext;
    if (!Context || !Context->DpuInterrupts || !K1xWaitVsync(Context->DpuInterrupts))
        return;
    Start = KeQueryPerformanceCounter(&Frequency);
    if (!K1xWaitVsync(Context->DpuInterrupts))
        return;
    End = KeQueryPerformanceCounter(NULL);
    PeriodUs = (ULONGLONG)(End.QuadPart - Start.QuadPart) * 1000000ULL / (ULONGLONG)Frequency.QuadPart;
    DPRINT1("K1XDPU: vertical blank every %I64u us\n", PeriodUs);
    if (PeriodUs >= 5000 && PeriodUs <= K1X_DPU_VSYNC_TIMEOUT_US)
        InterlockedExchange(&Device->ScanoutVBlankAvailable, 1);
}

BOOLEAN
SoftGpuPlatformWaitForVerticalBlank(
    _Inout_ PSOFTGPU_DEVICE Device)
{
    PK1XDPU_CONTEXT Context;

    if (Device == NULL || InterlockedCompareExchange(&Device->ScanoutVBlankAvailable, 0, 0) == 0)
        return FALSE;
    Context = Device->PlatformContext;
    if (Context && Context->DpuInterrupts && K1xWaitVsync(Context->DpuInterrupts))
        return TRUE;
    InterlockedExchange(&Device->ScanoutVBlankAvailable, 0);
    return FALSE;
}

NTSTATUS
SoftGpuPlatformQueryScanLine(
    _In_ PSOFTGPU_DEVICE Device,
    _Inout_ PDXGKARG_GETSCANLINE GetScanLine)
{
    if (Device == NULL || GetScanLine == NULL || GetScanLine->VidPnTargetId != 0)
        return STATUS_INVALID_PARAMETER;
    return STATUS_NOT_SUPPORTED;
}

MEMORY_CACHING_TYPE
SoftGpuPlatformSegmentCacheType(VOID)
{
    return MmWriteCombined;
}

NTSTATUS
SoftGpuPlatformQueryDescriptor(
    _In_ PSOFTGPU_DEVICE Device,
    _Inout_ PDXGK_DEVICE_DESCRIPTOR DeviceDescriptor)
{
    PK1XDPU_CONTEXT Context = Device->PlatformContext;
    ULONG Length;

    if (!Context || !Context->EdidLength)
        return STATUS_MONITOR_NO_DESCRIPTOR;
    if (DeviceDescriptor->DescriptorOffset >= Context->EdidLength)
        return STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA;
    if (!DeviceDescriptor->DescriptorBuffer)
        return STATUS_INVALID_PARAMETER;
    Length = min(DeviceDescriptor->DescriptorLength, Context->EdidLength - DeviceDescriptor->DescriptorOffset);
    RtlCopyMemory(DeviceDescriptor->DescriptorBuffer, Context->Edid + DeviceDescriptor->DescriptorOffset, Length);
    DeviceDescriptor->DescriptorLength = Length;
    return STATUS_SUCCESS;
}
