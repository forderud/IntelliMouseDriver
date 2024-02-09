#include "driver.h"
#include <Hidport.h>

#include "debug.h"
#include "SetBlack.h"

// TODO: Streamline this once everything works

EVT_WDF_REQUEST_COMPLETION_ROUTINE  SetBlackCompletionRoutine;
EVT_WDF_WORKITEM                    SetBlackWorkItem;

// TODO: Streamline this once everything works
typedef struct _SET_BLACK_WORK_ITEM_CONTEXT {
    WDFTIMER setBlackTimer;
} SET_BLACK_WORK_ITEM_CONTEXT, * PSET_BLACK_WORK_ITEM_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    SET_BLACK_WORK_ITEM_CONTEXT,
    Get_SetBlackWorkItemContext)

VOID SetBlackTimerProc(WDFTIMER timer) {

    TRACE_FN_ENTRY
    
    PSET_BLACK_WORK_ITEM_CONTEXT pWorkItemContext = NULL;
    WDFWORKITEM                  hWorkItem = 0;
    NTSTATUS status = STATUS_PNP_DRIVER_CONFIGURATION_INCOMPLETE;

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "TailLight: %s\n", __func__));

    WDFDEVICE device = (WDFDEVICE)WdfTimerGetParentObject(timer);

    {
        WDF_WORKITEM_CONFIG        workItemConfig;
        WDF_OBJECT_ATTRIBUTES      workItemAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
        workItemAttributes.ParentObject = device;
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&workItemAttributes,
            SET_BLACK_WORK_ITEM_CONTEXT);

        WDF_WORKITEM_CONFIG_INIT(&workItemConfig, SetBlackWorkItem);

        status = WdfWorkItemCreate(&workItemConfig,
            &workItemAttributes,
            &hWorkItem);

        pWorkItemContext = Get_SetBlackWorkItemContext(hWorkItem);
        pWorkItemContext->setBlackTimer = timer;

        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                DPFLTR_ERROR_LEVEL,
                "TailLight: Workitem creation failure 0x%x\n",
                status));
            return; // Maybe better luck next time.
        }
    }

    WdfWorkItemEnqueue(hWorkItem);

    TRACE_FN_EXIT
}

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

    DEVICE_CONTEXT* pDeviceContext = NULL;

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

    WDF_IO_TARGET_OPEN_PARAMS   openParams = {};

    pDeviceContext =
        WdfObjectGet_DEVICE_CONTEXT(device);

    if (pDeviceContext == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (pDeviceContext->ulSetBlackTimerTicksLeft) {
        pDeviceContext->ulSetBlackTimerTicksLeft--;
    }
    else
    {
        return STATUS_TIMEOUT;
    }

    status = WdfIoTargetCreate(
        device,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hidTarget);

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "TailLight: 0x%x while creating I/O target from worker thread\n",
            status));
        return status;
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_EXISTING_DEVICE(
        &openParams,
        pDeviceContext->pdo);

    openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;
    openParams.DesiredAccess = FILE_WRITE_ACCESS | FILE_READ_ACCESS;

    status = WdfIoTargetOpen(hidTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "TailLight: 0x%x while opening the I/O target from worker thread\n",
            status));
        goto ExitAndFree;
    }

    // TODO: Init to black once working.
    report.Blue = 0x0;
    report.Green = 0x0;
    report.Red = 0x0;

    WDF_MEMORY_DESCRIPTOR inputMemDesc;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemDesc, &report, sizeof(TailLightReport));

    status = WdfIoTargetSendInternalIoctlSynchronously(
        WdfDeviceGetIoTarget(device),
        NULL,
        IOCTL_HID_SET_FEATURE,
        &inputMemDesc,
        NULL,
        &sendOptions,
        NULL);

    KdPrint(("TailLight: %s WdfRequestSend status: 0x%x\n", __func__, status));

ExitAndFree:
    if (request != NULL) {
        WdfObjectDelete(request);
        request = NULL;
    }

    if (hidTarget != NULL) {
        WdfObjectDelete(hidTarget);
        hidTarget = NULL;
    }
        
    return status;
}


VOID EvtQueryRelationsFilter(WDFDEVICE device, DEVICE_RELATION_TYPE RelationType)
{
    UNREFERENCED_PARAMETER(RelationType);
    SetBlack(device);
}

void SetBlackCompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);
}


VOID SetBlackWorkItem(
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

    NTSTATUS status = SetBlack_CreateRequest(device);

    if (NT_SUCCESS(status)) {
        WdfTimerStop(pContext->setBlackTimer, FALSE);
        pContext->setBlackTimer = NULL;
    }

    TRACE_FN_EXIT
}

NTSTATUS SetBlack_CreateRequest(WDFDEVICE device) {
    TailLightReport  report = {};
    NTSTATUS         status = STATUS_FAILED_DRIVER_ENTRY;
    WDFREQUEST       request = NULL;
    WDFIOTARGET      hidTarget = NULL;

    WDFMEMORY InBuffer = 0;
    BOOLEAN ret = FALSE;
    BYTE* pInBuffer = nullptr;
    DEVICE_CONTEXT* pDeviceContext = NULL;

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

    WDF_IO_TARGET_OPEN_PARAMS   openParams = {};

    pDeviceContext =
        WdfObjectGet_DEVICE_CONTEXT(device);

    if (pDeviceContext == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (pDeviceContext->ulSetBlackTimerTicksLeft) {
        pDeviceContext->ulSetBlackTimerTicksLeft--;
    }
    else
    {
        return STATUS_TIMEOUT;
    }

    status = WdfIoTargetCreate(
        device,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hidTarget);

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "TailLight: 0x%x while creating I/O target from worker thread\n",
            status));
        return status;
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_CREATE_BY_NAME(
        &openParams,
        &pDeviceContext->PdoName,
        FILE_SHARE_WRITE);

    openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;
    //openParams.DesiredAccess = FILE_WRITE_ACCESS | FILE_READ_ACCESS;

    status = WdfIoTargetOpen(hidTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "TailLight: 0x%x while opening the I/O target from worker thread\n",
            status));
        goto ExitAndFree;
    }

    // TODO: Init to black once working.
    report.Blue = 0x0;
    report.Green = 0x0;
    report.Red = 0x0;
    
    status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES,
        hidTarget,
        &request);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfRequestCreate failed 0x%x\n", status));
        goto ExitAndFree;
    }

    WdfRequestSetCompletionRoutine(
        request,
        SetBlackCompletionRoutine,
        WDF_NO_CONTEXT);

    mem_attrib.ParentObject = request; // auto-delete with request*/

    status = WdfMemoryCreate(&mem_attrib,
        NonPagedPool,
        POOL_TAG,
        sizeof(TailLightReport),
        &InBuffer,
        (void**)&pInBuffer);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfMemoryCreate failed: 0x%x\n", status));
        goto ExitAndFree;
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
        goto ExitAndFree;
    }

    ret = WdfRequestSend(request,
        hidTarget,
        &sendOptions);

    status = WdfRequestGetStatus(request);
    KdPrint(("TailLight: %s WdfRequestSend status: 0x%x\n", __func__, status));

ExitAndFree:
    if (request != NULL) {
        WdfObjectDelete(request);
        request = NULL;
    }

    if (hidTarget != NULL) {
        WdfObjectDelete(hidTarget);
        hidTarget = NULL;
    }

    return status;
}