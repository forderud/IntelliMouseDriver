#pragma once

#include "debug.h"

#define MSINTELLIMOUSE_USBINTERFACE5_PREFIX L"\\??\\HID#VID_045E&PID_082A&MI_01&Col05"

#define BEGIN_WITH(x) { \
        auto &_ = x;
#define END_WITH() }

/** Driver-specific struct for storing instance-specific data. */
typedef struct _DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    ULONG          TailLight; ///< last written color

    // Useful for debugging. This way less need to hunt for the stack that
    // through all of the system threads after calling IoCallDriver.
    PKTHREAD       previousThread;
    BOOLEAN        fSetBlackSuccess;
    WDFWMIINSTANCE WmiInstance;
} DEVICE_CONTEXT;

template<typename T> inline void NukeWdfHandle(T& handle) {
    if (handle) {
        WdfObjectDelete(handle);
        handle = 0;
    }
}

#define RETURN_RESULT_IF_SET_OPERATION_NULL( var, op, result ) { \
    var = op; \
    if (var == NULL) return status; }

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

NTSTATUS PnpNotifyDeviceInterfaceChange(
    _In_ PVOID NotificationStructure,
    _Inout_opt_ PVOID Context);