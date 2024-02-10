#include "driver.h"
#include <Hidport.h>

#include "debug.h"
#include "SetBlack.h"

// TODO: Streamline this once everything works

EVT_WDF_REQUEST_COMPLETION_ROUTINE  SetBlackCompletionRoutine;
EVT_WDF_WORKITEM                    SetBlackWorkItem;

typedef struct _SET_BLACK_WORK_ITEM_CONTEXT {
    WDFTIMER setBlackTimer;
} SET_BLACK_WORK_ITEM_CONTEXT, * PSET_BLACK_WORK_ITEM_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    SET_BLACK_WORK_ITEM_CONTEXT,
    Get_SetBlackWorkItemContext)

VOID SetBlackTimerProc(WDFTIMER timer) {

    //TRACE_FN_ENTRY
    
    PSET_BLACK_WORK_ITEM_CONTEXT pWorkItemContext = NULL;
    WDFWORKITEM                  hWorkItem = 0;
    NTSTATUS status = STATUS_PNP_DRIVER_CONFIGURATION_INCOMPLETE;

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

    //TRACE_FN_EXIT
}

static NTSTATUS TryToOpenIoTarget(WDFIOTARGET target, 
    DEVICE_CONTEXT& DeviceContext) {

    WDF_IO_TARGET_OPEN_PARAMS   openParams = {};
    WDF_IO_TARGET_OPEN_PARAMS_INIT_CREATE_BY_NAME(
        &openParams,
        &DeviceContext.PdoName,
        FILE_WRITE_ACCESS);

    openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    // Ensure freed if fails.
    status = WdfIoTargetOpen(target, &openParams);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "TailLight: 0x%x while opening the I/O target from worker thread\n",
            status));
        NukeWdfHandle<WDFIOTARGET>(target);
    }

    return status;
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
    NTSTATUS         status = STATUS_UNSUCCESSFUL;
    WDFREQUEST       request = NULL;
    WDFIOTARGET      hidTarget = NULL;

    DEVICE_CONTEXT*  pDeviceContext = NULL;

    TRACE_FN_ENTRY

    RETURN_RESULT_IF_SET_OPERATION_NULL(pDeviceContext, 
        WdfObjectGet_DEVICE_CONTEXT(device), 
        STATUS_DEVICE_NOT_READY);

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

    status = TryToOpenIoTarget(hidTarget, *pDeviceContext);

    if (NT_SUCCESS(status)) {

        // TODO: Init to black once working.
        report.Blue = 0x0;
        report.Green = 0x0;
        report.Red = 0x0;

        WDF_MEMORY_DESCRIPTOR inputMemDesc;
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemDesc, &report, sizeof(TailLightReport));

        WDF_REQUEST_SEND_OPTIONS sendOptions = {};
        WDF_REQUEST_SEND_OPTIONS_INIT(
            &sendOptions,
            WDF_REQUEST_SEND_OPTION_SYNCHRONOUS |
            WDF_REQUEST_SEND_OPTION_TIMEOUT);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
            &sendOptions,
            WDF_REL_TIMEOUT_IN_SEC(1));

        status = WdfIoTargetSendInternalIoctlSynchronously(
            hidTarget,
            NULL,
            IOCTL_HID_SET_FEATURE,
            &inputMemDesc,
            NULL,
            &sendOptions,
            NULL);

        KdPrint(("TailLight: %s WdfRequestSend status: 0x%x\n", __func__, status));
    }

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

void SetBlackCompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context)
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    status = WdfRequestGetStatus(Request);
    KdPrint(("TailLight: %s WdfRequestSend status: 0x%x\n", __func__, status));

    // One-shot and top of stack, so delete and pray.
    WdfObjectDelete(Request);
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
    //TRACE_FN_ENTRY

    PSET_BLACK_WORK_ITEM_CONTEXT pContext = Get_SetBlackWorkItemContext(workItem);
    WDFDEVICE device = static_cast<WDFDEVICE>(WdfWorkItemGetParentObject(workItem));
    DEVICE_CONTEXT* pDeviceContext = NULL;

    NTSTATUS status = SetBlackAsync(device);

    pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

    if (pDeviceContext == NULL) {
        NT_ASSERTMSG("Device context not passed in", 0);
        return;
    }

    auto& remainingTicks = pDeviceContext->ulSetBlackTimerTicksLeft;

    if (remainingTicks)  {
        remainingTicks--;
    }

    if (NT_SUCCESS(status) || (!remainingTicks)) {
        WdfTimerStop(pContext->setBlackTimer, FALSE);
        pContext->setBlackTimer = NULL;
    }

    //TRACE_FN_EXIT
}

NTSTATUS SetBlackAsync(WDFDEVICE device) {

    TRACE_FN_ENTRY

    NTSTATUS        status = STATUS_FAILED_DRIVER_ENTRY;
    WDFIOTARGET     hidTarget = NULL;

    {
        DEVICE_CONTEXT* pDeviceContext = NULL;

        pDeviceContext =
            WdfObjectGet_DEVICE_CONTEXT(device);

        if (pDeviceContext == NULL) {
            return STATUS_DEVICE_NOT_READY;
        }

        // Ensure freed if fails.
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

        status = TryToOpenIoTarget(hidTarget, *pDeviceContext);
    }

    if (NT_SUCCESS(status)) {

        WDFREQUEST  request = NULL;

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

        TailLightReport  report = {};
        report.Blue = 0x0;
        report.Green = 0x0;
        report.Red = 0x0;

        // Set up a WDF memory object for the IOCTL request
        WDF_OBJECT_ATTRIBUTES mem_attrib = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&mem_attrib);
        mem_attrib.ParentObject = request; // auto-delete with request*/

        WDFMEMORY InBuffer = 0;
        BYTE* pInBuffer = nullptr;

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

        if (!WdfRequestSend(request, hidTarget, WDF_NO_SEND_OPTIONS)) {
            WdfObjectDelete(request);
            request = NULL;
        }
    }

ExitAndFree:
    if (hidTarget != NULL) {
        WdfObjectDelete(hidTarget);
        hidTarget = NULL;
    }

    TRACE_FN_EXIT

    return status;
}