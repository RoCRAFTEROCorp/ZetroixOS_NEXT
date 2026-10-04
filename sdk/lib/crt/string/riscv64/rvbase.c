/*
 * PROJECT:     LiberNT CRT
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     RISC-V64 RVA20 string and memory routines for kernel and native code
 * COPYRIGHT:   Copyright 2026 Ahmed ARIF <arif.ing@outlook.com>
 */

#include <string.h>
#include "rvstring.h"

size_t __cdecl
strlen(const char *String)
{
    return RvStrlenRva20(String);
}

int __cdecl
strcmp(const char *First, const char *Second)
{
    return RvStrcmpRva20(First, Second);
}

char * __cdecl
strcpy(char *Destination, const char *Source)
{
    return RvStrcpyRva20(Destination, Source);
}

void * __cdecl
memcpy(void *Destination, const void *Source, size_t Length)
{
    return RvMemcpyRva20(Destination, Source, Length);
}

void * __cdecl
memmove(void *Destination, const void *Source, size_t Length)
{
    return RvMemmoveRva20(Destination, Source, Length);
}

void * __cdecl
memset(void *Destination, int Value, size_t Length)
{
    return RvMemsetRva20(Destination, Value, Length);
}
