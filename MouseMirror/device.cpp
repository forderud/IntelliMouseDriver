#include "driver.h"
#include <Hidport.h>

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL EvtIoDeviceControlInternalFilter;


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
            DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: WdfDeviceCreate, Error %x\n", status);
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
            DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: WdfDeviceAllocAndQueryProperty failed 0x%x\n", status);
            return STATUS_UNSUCCESSFUL;
        }

        // initialize pDeviceContext->PdoName based on memory
        size_t bufferLength = 0;
        deviceContext->PdoName.Buffer = (WCHAR*)WdfMemoryGetBuffer(memory, &bufferLength);
        if (deviceContext->PdoName.Buffer == NULL)
            return STATUS_UNSUCCESSFUL;

        deviceContext->PdoName.MaximumLength = (USHORT)bufferLength;
        deviceContext->PdoName.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);

        DebugPrint(DPFLTR_INFO_LEVEL, "MouseMirror: PdoName: %wZ\n", deviceContext->PdoName); // outputs "\Device\00000083
    }

    {
        // create queue for filtering
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel); // don't synchronize
        //queueConfig.EvtIoRead  // pass-through read requests 
        //queueConfig.EvtIoWrite // pass-through write requests
        // queueConfig.EvtIoDeviceControl // pass-through IOCTL requests
        queueConfig.EvtIoInternalDeviceControl = EvtIoDeviceControlInternalFilter; // filter internal IOCTLs

        WDFQUEUE queue = 0; // auto-deleted when parent is deleted
        NTSTATUS status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: WdfIoQueueCreate failed 0x%x\n", status);
            return status;
        }
    }

    // Initialize WMI provider
    NTSTATUS status = WmiInitialize(device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: Error initializing WMI 0x%x\n", status);
        return status;
    }

    return status;
}

VOID MouFilter_ServiceCallback(
    _In_ DEVICE_OBJECT* DeviceObject,
    _In_ MOUSE_INPUT_DATA* InputDataStart,
    _In_ MOUSE_INPUT_DATA* InputDataEnd,
    _Inout_ ULONG* InputDataConsumed
) {
    //DebugPrint(DPFLTR_INFO_LEVEL, "MouseMirror: MouFilter_ServiceCallback (packages=%u\n", InputDataEnd - InputDataStart);

    WDFDEVICE device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);
    MouseMirrorDeviceInformation* pInfo = WdfObjectGet_MouseMirrorDeviceInformation(deviceContext->WmiInstance);

    // mirror mouse events in queue
    for (MOUSE_INPUT_DATA* id = InputDataStart; id != InputDataEnd; ++id) {
        if (!(id->Flags & MOUSE_MOVE_ABSOLUTE)) {
            // invert relative mouse movement
            if (pInfo->FlipLeftRight)
                id->LastX = -id->LastX;

            if (pInfo->FlipUpDown)
                id->LastY = -id->LastY;
        }

        // TODO: Process button events:
        //if (id->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN)
    }

    // UpperConnectData must be called at DISPATCH
    (*(PSERVICE_CALLBACK_ROUTINE)deviceContext->UpperConnectData.ClassService)
        (deviceContext->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);
}

VOID EvtIoDeviceControlInternalFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode
) {
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    //DebugPrint(DPFLTR_INFO_LEVEL, "MouseMirror: EvtIoDeviceControlInternal (IoControlCode=0x%x, InputBufferLength=%Iu)\n", IoControlCode, InputBufferLength);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

    NTSTATUS status = STATUS_SUCCESS; //unhandled
    switch (IoControlCode) {
    case IOCTL_INTERNAL_MOUSE_CONNECT:
    {
        DebugPrint(DPFLTR_INFO_LEVEL, "MouseMirror: IOCTL_INTERNAL_MOUSE_CONNECT\n");

        // Only allow one connection
        if (deviceContext->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }

        CONNECT_DATA* connectData = nullptr;
        size_t        length = 0;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(CONNECT_DATA), (void**)&connectData, &length);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: WdfRequestRetrieveInputBuffer failed %x\n", status);
            break;
        }

        // Copy the connection parameters to the device context.
        deviceContext->UpperConnectData = *connectData;

        // attach to the report chain to intercept mouse packets
        connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);
        connectData->ClassService = MouFilter_ServiceCallback;
    }
    break;

    case IOCTL_INTERNAL_MOUSE_DISCONNECT:
        status = STATUS_NOT_IMPLEMENTED; // according to https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/kbdmou/ni-kbdmou-ioctl_internal_mouse_disconnect
        break;
    }
    // No NT_SUCCESS(status) check here since we don't want to fail blocked calls

    // Forward the request down the driver stack
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), &options);
    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: WdfRequestSend failed with status: 0x%x\n", status);
        WdfRequestComplete(Request, status);
    }
}
