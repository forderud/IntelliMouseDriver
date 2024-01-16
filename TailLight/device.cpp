#include "driver.h"
#include <Hidport.h>

#include "deviocontrol.h"
#include "pnp.h"

PAGED_CODE_SEG
NTSTATUS CreateQueue(
    _In_ WDFDEVICE device)
/*++

Routine Description:

    Creates one parallel WDF device queue.

Arguments:

    Device - Handle to a framework device object.

Return Value:

   An NT or WDF status code.

--*/
{
    NTSTATUS status = STATUS_FAILED_DRIVER_ENTRY;

    PAGED_CODE();

    // create queue to process IOCTLs
    WDF_IO_QUEUE_CONFIG queueConfig = {};
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel); // don't synchronize
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControlFilter; // filter I/O device control requests
    //queueConfig.PowerManaged = WdfFalse;    // Otherwise don't get PNP notifications

    WDFQUEUE queue = 0; // auto-deleted when parent is deleted
    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoQueueCreate failed 0x%x\n", status));
    }

    return status;
}

PAGED_CODE_SEG
NTSTATUS CreateTaillightDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit,
    _Out_ WDFDEVICE* device)
/*++

Routine Description:

    Creates a WDF device for controlling the tail light on a Microsoft Pro
    IntelliMouse, hereby called the taillight device.

Arguments:

    DeviceInit - A new framework device initialization structure.
    *device    - A framework device handle that will refer to the device.

Return Value:

   An NT or WDF status code.

--*/
 {
     PAGED_CODE();
     
     NTSTATUS status = STATUS_FAILED_DRIVER_ENTRY;

     // Configure the device as a filter driver
     WdfFdoInitSetFilter(DeviceInit);

    // Initialize the PNP filter. The device returns that it's in an invalid state
    // If the IOCTL is sent too early. Therefore, we'll hitch a ride on self managed
    // init or restart (EvtDeviceSelfManagedInit).
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks = {};
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtDeviceSelfManagedInitFilter;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    // create device
    WDF_OBJECT_ATTRIBUTES attributes = {};
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfDeviceCreate, Error %x\n", status));
    }

    return status;
}

PAGED_CODE_SEG
NTSTATUS EvtDriverDeviceAdd(
    _In_ WDFDRIVER Driver, 
    _Inout_ PWDFDEVICE_INIT DeviceInit)
/*++
Routine Description:
    EvtDriverDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent to be part of the device stack as a filter.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

    Return Value:

    An NT or WDF status code.

--*/    
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    WDFDEVICE device = 0;
    NTSTATUS status = STATUS_FAILED_DRIVER_ENTRY;

    status = CreateTaillightDevice(DeviceInit, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = CreateQueue(device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Driver Framework always zero initializes an objects context memory
    // initialize DEVICE_CONTEXT struct with PdoName

    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

    // sets the filobject in the stack location.
    WDF_OBJECT_ATTRIBUTES attributes = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device; // auto-delete with device

    WDFMEMORY memory = 0;
    status = WdfDeviceAllocAndQueryProperty(device,
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

    // Initialize WMI provider
    status = WmiInitialize(device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: Error initializing WMI 0x%x\n", status));
        return status;
    }

    return status;
}


