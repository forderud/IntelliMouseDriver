#include "driver.h"
#include <Hidport.h>

#include "pnp.h"

// TODO: Streamline this once everything works
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
} SET_BLACK_WORK_ITEM_CONTEXT, * PSET_BLACK_WORK_ITEM_CONTEXT;

EVT_WDF_WORKITEM                    EvtSetBlackWorkItem;
EVT_WDF_REQUEST_COMPLETION_ROUTINE  EvtSetBlackCompletionRoutine;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    SET_BLACK_WORK_ITEM_CONTEXT, 
    Get_SetBlackWorkItemContext)

PAGED_CODE_SEG
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
    TailLightReport* pReport = NULL;
    NTSTATUS         status = STATUS_FAILED_DRIVER_ENTRY;
    WDFREQUEST       request = NULL;
    WDFIOTARGET      hidTarget = NULL;

    WDFMEMORY InBuffer = 0;
    BOOLEAN ret = FALSE;
    BYTE* pInBuffer = nullptr;

    PAGED_CODE();

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

    //
    // Create a report to send to the device.
    //
    pReport = (TailLightReport*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(TailLightReport),
        POOL_TAG_TAILLIGHT_REPORT);

    if (pReport == NULL) {
        KdPrint(("TailLight: Can't create Tail Light report.\n"));
        goto ExitAndFree;
    }

    Init(pReport);

    // TODO: Init to black once working.
    pReport->Blue = 0;
    pReport->Green = 0x0;
    pReport->Red = 0;

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
    RtlCopyMemory(pInBuffer, &pReport, sizeof(TailLightReport));

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
    
    if (ret == FALSE) {
        status = WdfRequestGetStatus(request);
        TRACE_REQUEST_FAILURE(status)
        goto ExitAndFree;
    }

ExitAndFree:
    if (pReport != NULL) {
        ExFreePool(pReport);
        pReport = NULL;
    }

    if (request != NULL) {
        WdfObjectDelete(request);
        request = NULL;
    }

    TRACE_FN_EXIT

    return status;
}

PAGED_CODE_SEG
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

    PAGED_CODE();

    TRACE_FN_ENTRY

    PSET_BLACK_WORK_ITEM_CONTEXT pContext = Get_SetBlackWorkItemContext(workItem);
    WDFDEVICE device = static_cast<WDFDEVICE>(WdfWorkItemGetParentObject(workItem));

    pContext->resultStatus = SetBlack(device);

    TRACE_FN_EXIT
}

PAGED_CODE_SEG
NTSTATUS EvtDeviceSelfManagedInitFilter(_In_ WDFDEVICE Device) {

    PAGED_CODE();

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
    pWorkItemContext->owningDevice = Device;

    WdfWorkItemEnqueue(hWorkItem);

    TRACE_FN_EXIT

    return status;
}

PAGED_CODE_SEG
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

    PAGED_CODE();

    TRACE_FN_ENTRY
    TRACE_FN_EXIT
}
