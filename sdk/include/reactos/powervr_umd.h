/*
 * PROJECT:     LiberNT PowerVR Rogue graphics
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Private escape interface between powervr.sys and its user-mode transport
 * COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
 */

#pragma once

#define POWERVR_ESCAPE_MAGIC        0x52565050UL
#define POWERVR_ESCAPE_ABI_VERSION  1

#define POWERVR_ESCAPE_QUERY_INFO   1
#define POWERVR_ESCAPE_DRM_IOCTL    2
#define POWERVR_ESCAPE_DRM_MMAP     3
#define POWERVR_ESCAPE_DRM_MUNMAP   4

typedef struct _POWERVR_ESCAPE_HEADER
{
    ULONG Magic;
    ULONG AbiVersion;
    ULONG Command;
    ULONG Size;
    LONG Status;
    ULONG Reserved;
} POWERVR_ESCAPE_HEADER;

typedef struct _POWERVR_ESCAPE_INFO
{
    POWERVR_ESCAPE_HEADER Header;
    USHORT Branch;
    USHORT Version;
    USHORT NumberOfScalableUnits;
    USHORT Config;
    ULONG FirmwareState;
    ULONG FirmwareVersion;
    ULONG64 CoreClockHz;
} POWERVR_ESCAPE_INFO;

typedef struct _POWERVR_DRM_IOCTL_ESCAPE
{
    POWERVR_ESCAPE_HEADER Header;
    ULONG Command;
    ULONG ArgumentSize;
    ULONG64 Argument;
} POWERVR_DRM_IOCTL_ESCAPE;

typedef struct _POWERVR_DRM_MAP_ESCAPE
{
    POWERVR_ESCAPE_HEADER Header;
    ULONG64 Offset;
    ULONG64 Length;
    ULONG64 Address;
} POWERVR_DRM_MAP_ESCAPE;
