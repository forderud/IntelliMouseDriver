#include "driver.h"
#include <Hidport.h>


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
            KdPrint(("MouseMirror: WdfDeviceCreate, Error %x\n", status));
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
            KdPrint(("MouseMirror: WdfDeviceAllocAndQueryProperty failed 0x%x\n", status));
            return STATUS_UNSUCCESSFUL;
        }

        // initialize pDeviceContext->PdoName based on memory
        size_t bufferLength = 0;
        deviceContext->PdoName.Buffer = (WCHAR*)WdfMemoryGetBuffer(memory, &bufferLength);
        if (deviceContext->PdoName.Buffer == NULL)
            return STATUS_UNSUCCESSFUL;

        deviceContext->PdoName.MaximumLength = (USHORT)bufferLength;
        deviceContext->PdoName.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);

        KdPrint(("MouseMirror: PdoName: %wZ\n", deviceContext->PdoName)); // outputs "\Device\00000083
    }

    {
        // create queue for filtering
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel); // don't synchronize
        //queueConfig.EvtIoRead  // pass-through read requests 
        //queueConfig.EvtIoWrite // pass-through write requests
        // queueConfig.EvtIoDeviceControl // pass-through IOCTL requests

        WDFQUEUE queue = 0; // auto-deleted when parent is deleted
        NTSTATUS status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

        if (!NT_SUCCESS(status)) {
            KdPrint(("MouseMirror: WdfIoQueueCreate failed 0x%x\n", status));
            return status;
        }
    }

    // Initialize WMI provider
    NTSTATUS status = WmiInitialize(device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("MouseMirror: Error initializing WMI 0x%x\n", status));
        return status;
    }

    return status;
}
