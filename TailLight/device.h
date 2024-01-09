#pragma once

#define DPF_TRACE_LEVEL 1
#define DPF_CONTINUABLE_FAILURE 30

#define TRACE_FN_ENTRY KdPrintEx((DPFLTR_IHVDRIVER_ID, 1, "TailLight: Entry %s\n", __func__));
#define TRACE_FN_EXIT  KdPrintEx((DPFLTR_IHVDRIVER_ID, 1, "TailLight: Exit: %s\n", __func__));
#define TRACE_REQUEST_BOOL(ret)  KdPrint(("TailLight: WdfRequestSend returned bool: 0x%x\n", ret));
#define TRACE_REQUEST_FAILURE(status)  KdPrint(("TailLight: WdfRequestSend failed. Status=: 0x%x\n", ret));

#define POOL_TAG_TAILLIGHT_REPORT 'TLrp'
#define POOL_TAG_WORK_ITEM_CONTEXT 'TLt2'

typedef struct _SET_BLACK_WORK_ITEM_CONTEXT {
    enum class state {
        initialized = 0,
        queued_waiting,
        executing,
        completed,
        owning_device_unloaded
    };

    state _state;

    WDFDEVICE owningDevice;
    NTSTATUS  resultStatus;
} SET_BLACK_WORK_ITEM_CONTEXT, *PSET_BLACK_WORK_ITEM_CONTEXT;

/** Driver-specific struct for storing instance-specific data. */
typedef struct _DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    ULONG          TailLight; ///< last written color

    // If present, keep track of the set black work item
    // context, in case we are unloaded.
    PSET_BLACK_WORK_ITEM_CONTEXT pSetBlackWorkItemContext;
} DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SET_BLACK_WORK_ITEM_CONTEXT, Get_SetBlackWorkItemContext)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

// Sets the taillight to the default state of black.
//  IN: WDFDEVICE -> The device context for taillight
// 
//  Returns: NTSTATUS: STATUS_SUCCESS on success
//                     Anything else on failure
NTSTATUS SetBlack(WDFDEVICE device);