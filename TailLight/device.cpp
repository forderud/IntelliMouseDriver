#include "driver.h"
#include <Hidport.h>

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlFilter;
EVT_WDF_REQUEST_COMPLETION_ROUTINE IoctlRequestCompletionRoutine;


VOID IoctlRequestCompletionRoutine(
    WDFREQUEST                      Request,
    WDFIOTARGET                     Target,
    WDF_REQUEST_COMPLETION_PARAMS*  CompletionParams,
    WDFCONTEXT                      Context
)
/*++
Arguments:
    Dummy competion routine that simply calls WdfRequestComplete. We have to use
    this dummy competion routine instead of WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET,
    because we're formating a manually created request.
--*/
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(CompletionParams);
    UNREFERENCED_PARAMETER(Context);

    WdfRequestCompleteWithInformation(Request, WdfRequestGetStatus(Request), WdfRequestGetInformation(Request));
}


NTSTATUS EvtDriverDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
/*++
Routine Description:
    EvtDriverDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent to be part of the device stack as a filter.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.
--*/    
{
    UNREFERENCED_PARAMETER(Driver);

    // Configure the device as a filter driver
    WdfFdoInitSetFilter(DeviceInit);

    WDFDEVICE device = 0;
    {
        // create device
        WDF_OBJECT_ATTRIBUTES attributes = {};
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

        NTSTATUS status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfDeviceCreate, Error %x\n", status));
            return status;
        }
    }

    // Driver Framework always zero initializes an objects context memory
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

    {
        // initialize DEVICE_CONTEXT struct with PdoName

        // In order to send ioctls to our PDO, we have open to open it
        // by name so that we have a valid filehandle (fileobject).
        // When we send ioctls using the IoTarget, framework automatically 
        // sets the filobject in the stack location.
        WDF_OBJECT_ATTRIBUTES attributes = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = device; // auto-delete with device

        WDFMEMORY memory = 0;
        NTSTATUS status = WdfDeviceAllocAndQueryProperty(device,
            DevicePropertyPhysicalDeviceObjectName,
            NonPagedPoolNx,
            &attributes,
            &memory);

        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfDeviceAllocAndQueryProperty failed 0x%x\n", status));
            return STATUS_UNSUCCESSFUL;
        }

        // initialize pDeviceContext->PdoName based on memory
        size_t bufferLength = 0;
        deviceContext->PdoName.Buffer = (WCHAR*)WdfMemoryGetBuffer(memory, &bufferLength);
        if (deviceContext->PdoName.Buffer == NULL)
            return STATUS_UNSUCCESSFUL;

        deviceContext->PdoName.MaximumLength = (USHORT)bufferLength;
        deviceContext->PdoName.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);

        KdPrint(("TailLight: PdoName: %wZ\n", deviceContext->PdoName)); // outputs "\Device\00000083
    }

    {
        // create queue to process IOCTLs
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel); // don't synchronize
        //queueConfig.EvtIoRead // pass-through read requests 
        //queueConfig.EvtIoWrite // pass-through write requests
        queueConfig.EvtIoDeviceControl = EvtIoDeviceControlFilter; // filter I/O device control requests

        WDFQUEUE queue = 0; // auto-deleted when parent is deleted
        NTSTATUS status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfIoQueueCreate failed 0x%x\n", status));
            return status;
        }
    }

    // Initialize WMI provider
    NTSTATUS status = WmiInitialize(device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: Error initializing WMI 0x%x\n", status));
        return status;
    }

    {
        // initialize tail-light to black
        WDF_OBJECT_ATTRIBUTES  req_attr = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&req_attr);
        req_attr.ParentObject = device; // auto-delete with device

        WDFREQUEST request = 0;
        status = WdfRequestCreate(&req_attr, WdfDeviceGetIoTarget(device), &request);
        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfRequestCreate failed: 0x%x\n", status));
            return status;
        }

        // populate request
        TailLightReport report;
        report.SetColor(0); // black color

        // Set up a WDF memory object for the IOCTL request
        WDF_OBJECT_ATTRIBUTES mem_attrib = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&mem_attrib);
        mem_attrib.ParentObject = request; // auto-delete with request

        WDFMEMORY InBuffer = 0;
        BYTE* InBuffer_ptr = nullptr;
        status = WdfMemoryCreate(&mem_attrib, NonPagedPool, POOL_TAG, sizeof(report), &InBuffer, (void**)&InBuffer_ptr);
        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfMemoryCreatePreallocated failed: 0x%x\n", status));
            return status;
        }
        RtlCopyMemory(InBuffer_ptr, &report, sizeof(report));

        // Format the request as write operation
        status = WdfIoTargetFormatRequestForIoctl(WdfDeviceGetIoTarget(device), request, IOCTL_HID_SET_FEATURE, InBuffer, 0, 0, 0);
        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: WdfIoTargetFormatRequestForIoctl failed: 0x%x\n", status));
            return status;
        }

        WdfRequestSetCompletionRoutine(request, IoctlRequestCompletionRoutine, NULL);

        // Forward the request down the driver stack
        WDF_REQUEST_SEND_OPTIONS options = {};
        WDF_REQUEST_SEND_OPTIONS_INIT(&options, 0); // WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET incompatible with WdfIoTargetFormatRequest...

        BOOLEAN ret = WdfRequestSend(request, WdfDeviceGetIoTarget(device), &options);
        if (ret == FALSE) {
            status = WdfRequestGetStatus(request);
            KdPrint(("TailLight: WdfRequestSend failed: 0x%x\n", status));
        }

    }

    return status;
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

    (KMDF) IOCTL_HID_Xxx code when handlng IRP_MJ_INTERNAL_DEVICE_CONTROL
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
    UNREFERENCED_PARAMETER(InputBufferLength);

    //KdPrint(("TailLight: EvtIoDeviceControl (IoControlCode=0x%x, InputBufferLength=%Iu)\n", IoControlCode, InputBufferLength));

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);

    NTSTATUS status = STATUS_SUCCESS; //unhandled
    switch (IoControlCode) {
      case IOCTL_HID_SET_FEATURE: // 0xb0191
        status = SetFeatureFilter(device, Request, InputBufferLength);
        break;
    }
    // No NT_SUCCESS(status) check here since we don't want to fail blocked calls

    // Forward the request down the driver stack
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), &options);
    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        KdPrint(("TailLight: WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }
}
