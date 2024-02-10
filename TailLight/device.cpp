#include "driver.h"
#include <Hidport.h>

#include "debug.h"
#include "SetBlack.h"

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlFilter;

NTSTATUS StartPeriodicTimerToOpenIoTarget(WDFDEVICE device) {
    
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    WDFTIMER timer = nullptr;

    {
        // Initialize tail-light to black to have control over HW state
        WDF_TIMER_CONFIG timerCfg = {};
        WDF_TIMER_CONFIG_INIT_PERIODIC(
            &timerCfg, 
            &SetBlackTimerProc,
            1);

        WDF_OBJECT_ATTRIBUTES attribs = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        attribs.ParentObject = device;

        status = WdfTimerCreate(&timerCfg, &attribs, &timer);
        if (!NT_SUCCESS(status)) {
            KdPrint(("WdfTimerCreate failed 0x%x\n", status));
            return status;
        }
    }

    WdfObjectGet_DEVICE_CONTEXT(device)->ulSetBlackTimerTicksLeft = MAX_SET_BLACK_TIMER_TICKS;

    status = WdfTimerStart(timer, 0);
    KdPrint(("TailLight: Periodic timer started.\n"));
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfTimerStart failed 0x%x\n", status));
        return status;
    }

    return status;
}

void EvtDeviceFileCreate(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject) {

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    TRACE_FN_ENTRY

    WdfRequestComplete(Request, STATUS_SUCCESS);

    TRACE_FN_EXIT
}

NTSTATUS StartTimerToOpenDevice(WDFDEVICE device) {

    NTSTATUS status;

    // Initialize tail-light to black to have control over HW state
    WDF_TIMER_CONFIG timerCfg = {};
    WDF_TIMER_CONFIG_INIT(&timerCfg, SetBlackTimerProc);

    WDF_OBJECT_ATTRIBUTES attribs = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
    attribs.ParentObject = device;
    attribs.ExecutionLevel = WdfExecutionLevelPassive; // required to access HID functions

    WDFTIMER timer = nullptr;
    status = WdfTimerCreate(&timerCfg, &attribs, &timer);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfTimerCreate failed 0x%x\n", status));
        return status;
    }

    WdfObjectGet_DEVICE_CONTEXT(device)->ulSetBlackTimerTicksLeft = MAX_SET_BLACK_TIMER_TICKS;

    status = WdfTimerStart(timer, 0); // no wait
    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfTimerStart failed 0x%x\n", status));
        return status;
    }

    return status;
}

NTSTATUS PnpNotifyDeviceInterfaceChange(
    _In_ PVOID pvNotificationStructure,
    _Inout_opt_ PVOID pvContext) {

    //KdPrint(("TailLight: PnpNotifyDeviceInterfaceChange enter\n"));
    ASSERTMSG("WDFDEVICE not passed in!", pvContext);

    if (pvNotificationStructure == NULL) {
        return STATUS_SUCCESS;
    }

    PDEVICE_INTERFACE_CHANGE_NOTIFICATION pDevInterface =
        (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)pvNotificationStructure;

    ASSERT(IsEqualGUID(*(_GUID*)&(pDevInterface->InterfaceClassGuid),
        *(_GUID*)&GUID_DEVINTERFACE_HID));

    if (IsEqualGUID(*(LPGUID) & (pDevInterface->Event),
        *(LPGUID)&GUID_DEVICE_INTERFACE_ARRIVAL)) {

        // We don't care about removal or query removes.
        // If the Pro Intellimouse is removed it goes black, so no work needed.
        auto& symLinkName = pDevInterface->SymbolicLinkName;
        if (symLinkName->Length < sizeof(MSINTELLIMOUSE_USBINTERFACE5_PREFIX)) {
            return STATUS_SUCCESS;
        }

        // Ensure that the Microsoft Mouse USB interface #5 has arrived.
        if (!memcmp((PVOID)MSINTELLIMOUSE_USBINTERFACE5_PREFIX,
            symLinkName->Buffer,
            sizeof(MSINTELLIMOUSE_USBINTERFACE5_PREFIX) - 2)) {

            // Opening a device may trigger PnP operations. Ensure that either a
            // timer or a work item is used when opening up a device.
            // Refer to p356 of Oney and IoGetDeviceObjectPointer.
            return StartTimerToOpenDevice((WDFDEVICE)pvContext);
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS SelfManagedIoInit(WDFDEVICE device) {

    WDFDRIVER driver = WdfDeviceGetDriver(device);
    PDRIVER_CONTEXT driverContext = WdfObjectGet_DRIVER_CONTEXT(driver);
    UNREFERENCED_PARAMETER(device);

    NTSTATUS status = STATUS_SUCCESS;

    status = IoRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (PVOID)&GUID_DEVINTERFACE_HID,
        WdfDriverWdmGetDriverObject(driver),
        PnpNotifyDeviceInterfaceChange,
        (PVOID)device,
        &driverContext->pnpDevInterfaceChangedHandle
    );

    return status;
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
    //WdfDeviceInitSetExclusive(DeviceInit, TRUE);
    auto pdo = WdfFdoInitWdmGetPhysicalDevice(DeviceInit);

    { // TODO: Determine how effective
        WDF_FILEOBJECT_CONFIG fileObjectConfig = {};
        WDF_FILEOBJECT_CONFIG_INIT(&fileObjectConfig, 
            EvtDeviceFileCreate, 
            NULL, 
            NULL);
    }

    {
        // register PnP callbacks (must be done before WdfDeviceCreate)
        WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
        PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = SelfManagedIoInit;
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);
    }

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

        deviceContext->pdo = pdo;
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
    UNREFERENCED_PARAMETER(InputBufferLength);

    //KdPrint(("TailLight: EvtIoDeviceControl (IoControlCode=0x%x, InputBufferLength=%Iu)\n", IoControlCode, InputBufferLength));

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);

    NTSTATUS status = STATUS_SUCCESS; //unhandled
    switch (IoControlCode) {
      case IOCTL_HID_SET_FEATURE: // 0xb0191
        status = SetFeatureFilter(device, Request, InputBufferLength);
        break;
    }

    // Forward the request down the driver stack
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), &options);
    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        KdPrint(("TailLight: WdfRequestSend failed with status: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }
}
