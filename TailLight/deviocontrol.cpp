#include "driver.h"
#include <Hidport.h>

#include "debug.h"

EVT_WDF_REQUEST_COMPLETION_ROUTINE  EvtDeviceIoControlCompletionRoutine;

void EvtDeviceIoControlCompletionRoutine(
    _In_
    WDFREQUEST Request,
    _In_
    WDFIOTARGET Target,
    _In_
    PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_
    WDFCONTEXT Context)
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    TRACE_FN_ENTRY

    WdfRequestComplete(Request, STATUS_SUCCESS);

    TRACE_FN_EXIT
}


VOID EvtIoDeviceControlFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode
)
/*++
Routine Description:
    This event callback function is called when the driver receives an

    (KMDF) IOCTL_HID_Xxx code when handling IRP_MJ_INTERNAL_DEVICE_CONTROL
    (UMDF) IOCTL_HID_Xxx, IOCTL_UMDF_HID_Xxx when handling IRP_MJ_DEVICE_CONTROL

Arguments:
    Queue - A handle to the queue object that is associated with the I/O request

    Request - A handle to a framework request object.

    OutputBufferLength - The length, in bytes, of the request's output buffer,
            if an output buffer is available.

    InputBufferLength - The length, in bytes, of the request's input buffer, if
            an input buffer is available.

    IoControlCode - The driver or system defined IOCTL associated with the request
--*/
{
    UNREFERENCED_PARAMETER(OutputBufferLength);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    WDFIOTARGET target = WdfDeviceGetIoTarget(device);
    WDFMEMORY   inBuffer = 0;

    PCHAR       pInBuffer = nullptr;
    BOOLEAN     ret = FALSE;

    TailLightReport* packet = nullptr;

    NTSTATUS  status = STATUS_UNSUCCESSFUL;
    HID_COLLECTION_INFORMATION  collectionInformation = { 0 };

    // Set up a WDF memory object for the IOCTL request
    WDF_OBJECT_ATTRIBUTES mem_attrib = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&mem_attrib);

    WDF_REQUEST_SEND_OPTIONS sendOptions = {};

    KdPrint(("TailLight: EvtIoDeviceControl (IoControlCode=0x%x)\n",
        IoControlCode));

    switch (IoControlCode) {
    case IOCTL_HID_SET_FEATURE: // 0xb0191
        // Ensure that we send this driver-owned request down in all failure cases.
        //
        // Create a report to send to the device.
        // Remember: Being returned the IRP's system buffer - read only!
        //
        packet = SetFeatureFilter(device, Request, InputBufferLength);
        if (packet == nullptr) {
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
            break;
        }

        status = WdfMemoryCreate(&mem_attrib,
            NonPagedPool,
            POOL_TAG,
            sizeof(TailLightReport),
            &inBuffer,
            (void**)&pInBuffer);

        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfMemoryCreate failed: 0x%x\n", status));
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
            break;
        }
        RtlCopyMemory(pInBuffer, packet, sizeof(TailLightReport));

        WdfRequestSetCompletionRoutine(Request,
            EvtDeviceIoControlCompletionRoutine,
            WDF_NO_CONTEXT);

        // Format the request as write operation
        status = WdfIoTargetFormatRequestForIoctl(target,
            Request,
            IOCTL_HID_SET_FEATURE,
            inBuffer,
            NULL,
            0,
            0);

        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfIoTargetFormatRequestForIoctl failed: 0x%x\n", status));
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
            break;
        }
        // No NT_SUCCESS(status) check here since we don't want to fail blocked calls

        // Forward the request down the formatted driver stack
        WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS |
            WDF_REQUEST_SEND_OPTION_TIMEOUT);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(1));
        break;

    default:

        // Since untouched and drive owned, take the easy route.
        WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
        break;
    }

    ret = WdfRequestSend(Request, target, &sendOptions);
    KdPrint(("TailLight: WdfRequestSend returned: 0x%x\n", ret));

    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        KdPrint(("TailLight: WdfRequestSend failed with status: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }

    if (inBuffer != 0) {
        WdfObjectDelete(inBuffer);
        inBuffer = 0;
    }
}