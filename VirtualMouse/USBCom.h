#pragma once
/*++
     This module contains the core USB-side transfer logic
     (business logic, just a bit above hardware interfaces)
--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include "trace.h"
#include "Public.h"

// in case it becomes a queue one day, an arbitrary limit
#define INTR_STATE_MAX_CACHED_UPDATES 100

struct DEVICE_INTR_STATE {
    MOUSE_INPUT_REPORT latestStatus;
    ULONG             numUnreadUpdates;
    WDFSPINLOCK       sync;
};



struct IO_CONTEXT {
    WDFQUEUE          ControlQueue;
    WDFQUEUE          BulkOutQueue;
    WDFQUEUE          BulkInQueue;
    WDFQUEUE          InterruptUrbQueue;
    WDFQUEUE          IntrDeferredQueue;
    BOOLEAN           bStopping;

    DEVICE_INTR_STATE IntrState;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(IO_CONTEXT, WdfDeviceGetIoContext);



NTSTATUS
Io_AllocateContext(
    _In_ UDECXUSBDEVICE Object
);



NTSTATUS
Io_RaiseInterrupt(
    _In_ UDECXUSBDEVICE    Device,
    _In_ MOUSE_INPUT_REPORT LatestStatus
);



NTSTATUS
Io_RetrieveEpQueue(
    _In_ UDECXUSBDEVICE  Device,
    _In_ UCHAR           EpAddr,
    _Out_ WDFQUEUE     * Queue
);


VOID
Io_StopDeferredProcessing(
    _In_ UDECXUSBDEVICE  Device,
    _Out_ IO_CONTEXT*   pIoContextCopy
);



VOID
Io_FreeEndpointQueues(
    _In_ IO_CONTEXT* pIoContext
);




NTSTATUS
Io_DeviceSlept(
    _In_ UDECXUSBDEVICE  Device
);


NTSTATUS
Io_DeviceWokeUp(
    _In_ UDECXUSBDEVICE  Device
);
