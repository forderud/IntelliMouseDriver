#pragma once

/** Driver-specific struct for storing instance-specific data. */
typedef struct _DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    ULONG          TailLight; ///< last written color
} DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

// Sets the taillight to the default state of black.
//  IN: DEVICE_CONTEXT* -> The device context for taillight
// 
//  Returns: NTSTATUS: STATUS_SUCCESS on success
//                     Anything else on failure
NTSTATUS TailLight_SetBlack(DEVICE_CONTEXT* pDeviceContext);