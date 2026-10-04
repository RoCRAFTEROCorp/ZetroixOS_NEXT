/*
 * PROJECT:     LiberNT Storage
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Unique identifier of a basic volume on an MBR or GPT disk
 * COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
 */

#pragma once

#include <pshpack1.h>
typedef union _BASIC_VOLUME_UNIQUE_ID
{
    struct
    {
        ULONG Signature;
        ULONGLONG StartingOffset;
    } Mbr;
    struct
    {
        ULONGLONG Signature;
        GUID PartitionGuid;
    } Gpt;
} BASIC_VOLUME_UNIQUE_ID, *PBASIC_VOLUME_UNIQUE_ID;
#include <poppack.h>
C_ASSERT(RTL_FIELD_SIZE(BASIC_VOLUME_UNIQUE_ID, Mbr) == 0x0C);
C_ASSERT(RTL_FIELD_SIZE(BASIC_VOLUME_UNIQUE_ID, Gpt) == 0x18);

#define DMIO_ID_SIGNATURE   (*(ULONGLONG*)"DMIO:ID:")
