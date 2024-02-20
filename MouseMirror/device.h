#pragma once
#include <kbdmou.h>

/** Driver-specific struct for storing instance-specific data. */
typedef struct _DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    WDFWMIINSTANCE WmiInstance;
    CONNECT_DATA   UpperConnectData; // callback to intercept mouse packets
} DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(MouseMirrorDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;
