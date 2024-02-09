#pragma once

#define MSINTELLIMOUSE_USBINTERFACE5_PREFIX L"\\??\\HID#VID_045E&PID_082A&MI_01&Col05"

/** Driver-specific struct for storing instance-specific data. */
typedef struct _DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    ULONG          TailLight; ///< last written color
    ULONG          ulSetBlackTimerTicksLeft;
    PDEVICE_OBJECT pdo;
} DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

NTSTATUS PnpNotifyDeviceInterfaceChange(
    _In_ PVOID NotificationStructure,
    _Inout_opt_ PVOID Context);
