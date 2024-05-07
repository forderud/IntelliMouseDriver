#pragma once
/*++
The interface exposed here is used by the test application to feed information
to the virtual USB device, so it knows what/when to answer to queries from the hist.
All the code in here is for testing purpose, and it is NOT related to UDE directly.
--*/

#include "public.h"
#include "Misc.h"


BOOLEAN
BackChannelIoctl(
    _In_ ULONG IoControlCode,
    _In_ WDFDEVICE ctrdevice,
    _In_ WDFREQUEST Request
);
