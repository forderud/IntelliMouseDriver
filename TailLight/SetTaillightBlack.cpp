#include "driver.h"
#include <Hidport.h>

#include "debug.h"
#include "SetBlack.h"

EVT_WDF_REQUEST_COMPLETION_ROUTINE  SetBlackCompletionRoutine;
EVT_WDF_WORKITEM                    SetBlackWorkItem;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    SET_BLACK_WORK_ITEM_CONTEXT,
    Get_SetBlackWorkItemContext)

NTSTATUS CreateWorkItemForIoTargetOpenDevice(WDFDEVICE device)
    /*++

    Routine Description:

        Creates a WDF workitem to do the SetBlack() call after the driver
        stack has initialized.

    Arguments:

        Device - Handle to a pre-allocated WDF work item.

    Requirements:
        Must be synchronized to the device.

    --*/
{

    //TRACE_FN_ENTRY
    
    WDFWORKITEM hWorkItem = 0;
    NTSTATUS status = STATUS_PNP_DRIVER_CONFIGURATION_INCOMPLETE;
    {
        WDF_WORKITEM_CONFIG        workItemConfig;
        WDF_OBJECT_ATTRIBUTES      workItemAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
        workItemAttributes.ParentObject = device;

        DEVICE_CONTEXT* pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

        // It's possible to get called twice. Been there, done that?
        if (pDeviceContext->fSetBlackSuccess) {
            return STATUS_SUCCESS;
        }

        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&workItemAttributes,
            SET_BLACK_WORK_ITEM_CONTEXT);

        WDF_WORKITEM_CONFIG_INIT(&workItemConfig, SetBlackWorkItem);

        status = WdfWorkItemCreate(&workItemConfig,
            &workItemAttributes,
            &hWorkItem);

        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                DPFLTR_ERROR_LEVEL,
                "TailLight: Workitem creation failure 0x%x\n",
                status));
            return status; // Maybe better luck next time.
        }

        pDeviceContext->pSetBlackWorkItemContext = 
            Get_SetBlackWorkItemContext(hWorkItem);
    }

    WdfWorkItemEnqueue(hWorkItem);

    //TRACE_FN_EXIT

    return status;
}

static NTSTATUS TryToOpenIoTarget(WDFIOTARGET target, 
    DEVICE_CONTEXT& DeviceContext) {

    PAGED_CODE();

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
    --*/
{
    TRACE_FN_ENTRY

    PSET_BLACK_WORK_ITEM_CONTEXT pWorkItemContext = Get_SetBlackWorkItemContext(workItem);
    pWorkItemContext->Init();

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    auto &remainingTicks = pWorkItemContext->setBlackTimerTicksLeft;
    WDFDEVICE device = static_cast<WDFDEVICE>(WdfWorkItemGetParentObject(workItem));
    DEVICE_CONTEXT* pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

    do {
        status = SetBlackAsync(device);
        if (NT_SUCCESS(status)) {
            pDeviceContext->fSetBlackSuccess = TRUE;
            break;
        }

        InterlockedDecrement((PLONG)&remainingTicks);
        status = pWorkItemContext->Wait();
        NT_ASSERTMSG("Taillight: delay wait failed\n", NT_SUCCESS(status));
    } while (remainingTicks);
    
    pDeviceContext->pSetBlackWorkItemContext = NULL;
    NukeWdfHandle(workItem);

    //TRACE_FN_EXIT
}

NTSTATUS SetBlackAsync(WDFDEVICE device) {

    TRACE_FN_ENTRY

    NTSTATUS        status = STATUS_FAILED_DRIVER_ENTRY;
    WDFIOTARGET     hidTarget = NULL;

    DEVICE_CONTEXT* pDeviceContext = NULL;

    pDeviceContext =
        WdfObjectGet_DEVICE_CONTEXT(device);

    if (pDeviceContext == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }
    {
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
            NonPagedPoolNx,
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

        pDeviceContext->previousThread = KeGetCurrentThread();

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