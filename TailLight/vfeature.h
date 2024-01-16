#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "inc\TailLight.h"

NTSTATUS SetFeatureColor (
    _In_  WDFDEVICE Device,
    _In_  ULONG     Color
    );

TailLightReport* SetFeatureFilter(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request,
    _In_ size_t     InputBufferLength
);
