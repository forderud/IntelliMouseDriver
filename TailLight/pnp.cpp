#include "driver.h"
#include <Hidport.h>

#include "pnp.h"
#include "dbghid.h"
#include "debug.h"

// TODO: Streamline this once everything works
typedef struct _SET_BLACK_WORK_ITEM_CONTEXT {
    NTSTATUS  resultStatus;
} SET_BLACK_WORK_ITEM_CONTEXT, * PSET_BLACK_WORK_ITEM_CONTEXT;

EVT_WDF_WORKITEM                    EvtSetBlackWorkItem;
EVT_WDF_REQUEST_COMPLETION_ROUTINE  EvtSetBlackCompletionRoutine;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    SET_BLACK_WORK_ITEM_CONTEXT, 
    Get_SetBlackWorkItemContext)

NTSTATUS SetBlack(
    WDFDEVICE device)
/*++

Routine Description:

    Attempts to set the taillight to the default state of black.   

Arguments:

    Device - Handle to a framework device object.

Return Value:

    An NT or WDF status code.

--*/
{
    TailLightReport  report = {};
    NTSTATUS         status = STATUS_FAILED_DRIVER_ENTRY;
    WDFREQUEST       request = NULL;
    WDFIOTARGET      hidTarget = NULL;

    WDFMEMORY InBuffer = 0;
    BOOLEAN ret = FALSE;
    BYTE* pInBuffer = nullptr;

    KdPrint(("TailLight: %s\n", __func__));

    // Set up a WDF memory object for the IOCTL request
    WDF_OBJECT_ATTRIBUTES mem_attrib = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&mem_attrib);

    WDF_REQUEST_SEND_OPTIONS sendOptions = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(
        &sendOptions, 
        WDF_REQUEST_SEND_OPTION_SYNCHRONOUS |
        WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
        &sendOptions, 
        WDF_REL_TIMEOUT_IN_SEC(1));

    hidTarget = WdfDeviceGetIoTarget(device);

    WDFMEMORY memory = 0;
    DumpTarget(hidTarget, memory);

    if (memory != 0) {
        WdfObjectDelete(memory);
        memory = 0;
    }

    Init(&report);

    // TODO: Init to black once working.
    report.Blue = 0xFF;
    report.Green = 0xFF;
    report.Red = 0xFF;

    status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES,
        hidTarget,
        &request);

    WdfRequestSetCompletionRoutine(
        request,
        EvtSetBlackCompletionRoutine,
        WDF_NO_CONTEXT);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfRequestCreate failed 0x%x\n", status));
        goto ExitAndFree;
    }

    mem_attrib.ParentObject = request; // auto-delete with request

    status = WdfMemoryCreate(&mem_attrib,
        NonPagedPool,
        POOL_TAG,
        sizeof(TailLightReport),
        &InBuffer,
        (void**)&pInBuffer);
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfMemoryCreate failed: 0x%x\n", status));
        return status;
    }

    // TODO: Wondering if we just cant cast pInBuffr as a TailLightReport
    RtlCopyMemory(pInBuffer, &report, sizeof(TailLightReport));

    // Format the request as write operation
    status = WdfIoTargetFormatRequestForIoctl(hidTarget,
        request,
        IOCTL_HID_SET_FEATURE,
        InBuffer,
        NULL,
        0,
        0);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoTargetFormatRequestForIoctl failed: 0x%x\n", status));
        return status;
    }

    ret = WdfRequestSend(request, 
        hidTarget, 
        &sendOptions);
    TRACE_REQUEST_BOOL(ret)
    
    status = WdfRequestGetStatus(request);
    KdPrint(("TailLight: WdfRequestSend status status: 0x%x\n", status));

ExitAndFree:
    if (request != NULL) {
        WdfObjectDelete(request);
        request = NULL;
    }

    TRACE_FN_EXIT

    return status;
}


VOID EvtSetBlackWorkItem(
    WDFWORKITEM workItem)
/*++

Routine Description:

    Creates a WDF workitem to do the SetBlack() call after the driver
    stack has initialized.

Arguments:

    Device - Handle to a pre-allocated WDF work item.

Notes:
    TODO: The framework might not be fully finished handling the PNP IRP
    that launched this work item.
--*/
{
    TRACE_FN_ENTRY

    PSET_BLACK_WORK_ITEM_CONTEXT pContext = Get_SetBlackWorkItemContext(workItem);
    WDFDEVICE device = static_cast<WDFDEVICE>(WdfWorkItemGetParentObject(workItem));

    pContext->resultStatus = SetBlack(device);

    TRACE_FN_EXIT
}

NTSTATUS EvtDeviceSelfManagedInitFilter(_In_ WDFDEVICE Device) {

    PSET_BLACK_WORK_ITEM_CONTEXT pWorkItemContext = NULL;
    WDF_OBJECT_ATTRIBUTES        workItemAttributes;
    WDF_WORKITEM_CONFIG          workItemConfig;
    WDFWORKITEM                  hWorkItem;

    NTSTATUS status = STATUS_PNP_DRIVER_CONFIGURATION_INCOMPLETE;

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPF_TRACE_LEVEL, "TailLight: %s\n", __func__));

    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = Device;
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&workItemAttributes,
        SET_BLACK_WORK_ITEM_CONTEXT);

    WDF_WORKITEM_CONFIG_INIT(&workItemConfig, EvtSetBlackWorkItem);

    status = WdfWorkItemCreate(&workItemConfig,
        &workItemAttributes,
        &hWorkItem);

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
            DPF_CONTINUABLE_FAILURE,
            "TailLight: Workitem creation failure NTSTATUS=0x%x\n",
            status));
        return status;
    }

    pWorkItemContext = Get_SetBlackWorkItemContext(hWorkItem);

    WdfWorkItemEnqueue(hWorkItem);

    TRACE_FN_EXIT

    return status;
}

void EvtSetBlackCompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    TRACE_FN_ENTRY
    TRACE_FN_EXIT
}
