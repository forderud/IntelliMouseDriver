#include "device.hpp"
#include <Hidport.h>
#include "vfeature.hpp"
#include "wmi.hpp"


EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlFilter;


UNICODE_STRING GetTargetPropertyString(WDFIOTARGET target, DEVICE_REGISTRY_PROPERTY DeviceProperty) {
    WDF_OBJECT_ATTRIBUTES attributes = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = target; // auto-delete with I/O target

    WDFMEMORY memory = 0;
    NTSTATUS status = WdfIoTargetAllocAndQueryTargetProperty(target, DeviceProperty, NonPagedPoolNx, &attributes, &memory);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfIoTargetAllocAndQueryTargetProperty with property=0x%x failed 0x%x"), DeviceProperty, status);
        return {};
    }

    // initialize string based on memory
    size_t bufferLength = 0;
    UNICODE_STRING result = {};
    result.Buffer = (WCHAR*)WdfMemoryGetBuffer(memory, &bufferLength);
    if (result.Buffer == NULL)
        return {};

    result.MaximumLength = (USHORT)bufferLength;
    result.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);
    return result;
}

NTSTATUS EvhHidInterfaceChange(_In_ void* NotificationStruct, _Inout_opt_ void* Context) {
    auto* devNotificationStruct = (DEVICE_INTERFACE_CHANGE_NOTIFICATION*)NotificationStruct;
    auto* deviceContext = (DEVICE_CONTEXT*)Context;

    ASSERTMSG("EvhHidInterfaceChange interface mismatch", IsEqualGUID(devNotificationStruct->InterfaceClassGuid, GUID_DEVINTERFACE_HID));

    if (!IsEqualGUID(devNotificationStruct->Event, GUID_DEVICE_INTERFACE_ARRIVAL))
        return STATUS_SUCCESS; // ignore interface removal and other non-arrival events

    auto device = (WDFDEVICE)WdfObjectContextGetObject(deviceContext);

    {
        // open device to get underlying PDO name
        WDFIOTARGET_Wrap newDev;
        NTSTATUS status = WdfIoTargetCreate(device, WDF_NO_OBJECT_ATTRIBUTES, &newDev);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfIoTargetCreate failed 0x%x"), status);
            return status;
        }

        WDF_IO_TARGET_OPEN_PARAMS openParams = {};
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, devNotificationStruct->SymbolicLinkName, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;

        status = WdfIoTargetOpen(newDev, &openParams);
        if (!NT_SUCCESS(status)) {
            //DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfIoTargetOpen failed 0x%x"), status);
            return status;
        }

        UNICODE_STRING devPdo = GetTargetPropertyString(newDev, DevicePropertyPhysicalDeviceObjectName); // owned by newDev

        if (!RtlEqualUnicodeString(&devPdo, &deviceContext->PdoName, /*case insensitive*/FALSE)) {
            //DebugPrint(DPFLTR_WARNING_LEVEL, "TailLight: EvhHidInterfaceChange skipping due to SymbolicLinkName %wZ vs. PDO %wZ mismatch\n", &devPdo, &deviceContext->PdoName);
            return STATUS_SUCCESS; // incorrect device
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "TailLight: EvhHidInterfaceChange opening %wZ\n", &devPdo);
    }

    // unsubscribe from additional PnP events
    IoUnregisterPlugPlayNotificationEx(deviceContext->NotificationHandle);
    deviceContext->NotificationHandle = nullptr;


    // Initialize tail-light to black to have control over HW state
    NTSTATUS status = SetFeatureColor(device, 0);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: EvtSetBlackTimer failure 0x%x"), status);
        return status;
    }

    return STATUS_SUCCESS;
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

    WDFDEVICE Device = 0;
    {
        // create device
        WDF_OBJECT_ATTRIBUTES attributes = {};
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

        NTSTATUS status = WdfDeviceCreate(&DeviceInit, &attributes, &Device);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfDeviceCreate, Error %x"), status);
            return status;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "TailLight: PDO(0x%p) FDO(0x%p), Lower(0x%p)\n", WdfDeviceWdmGetPhysicalDevice(Device), WdfDeviceWdmGetDeviceObject(Device), WdfDeviceWdmGetAttachedDevice(Device));
    }

    // Driver Framework always zero initializes an objects context memory
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

    {
        // initialize DEVICE_CONTEXT struct with PdoName
        deviceContext->PdoName = GetTargetPropertyString(WdfDeviceGetIoTarget(Device), DevicePropertyPhysicalDeviceObjectName);
        if (!deviceContext->PdoName.Buffer) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: PdoName query failed"));
            return STATUS_UNSUCCESSFUL;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "TailLight: PdoName: %wZ\n", deviceContext->PdoName); // outputs "\Device\00000083"
    }

    {
        // create queue for filtering
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel); // don't synchronize
        //queueConfig.EvtIoRead  // pass-through read requests 
        //queueConfig.EvtIoWrite // pass-through write requests
        queueConfig.EvtIoDeviceControl = EvtIoDeviceControlFilter; // filter IOCTL requests

        WDFQUEUE queue = 0; // auto-deleted when parent is deleted
        NTSTATUS status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfIoQueueCreate failed 0x%x"), status);
            return status;
        }
    }

    // subscribe to PnP events for deferred HID PDO opening
    NTSTATUS status = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange, PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES, (PVOID)&GUID_DEVINTERFACE_HID,
        WdfDriverWdmGetDriverObject(WdfDeviceGetDriver(Device)), EvhHidInterfaceChange, (PVOID)deviceContext, &deviceContext->NotificationHandle);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: IoRegisterPlugPlayNotification failed: 0x%x"), status);
        return status;
    }

    // Initialize WMI provider
    status = WmiInitialize(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: Error initializing WMI 0x%x"), status);
        return status;
    }

    KeInitializeEvent(&deviceContext->SelfTestCompleted, SynchronizationEvent, FALSE);

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

    //DebugPrint(DPFLTR_INFO_LEVEL, "TailLight: EvtIoDeviceControl (IoControlCode=0x%x, InputBufferLength=%Iu)\n", IoControlCode, InputBufferLength);

    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);

    NTSTATUS status = STATUS_SUCCESS; //unhandled
    switch (IoControlCode) {
      case IOCTL_HID_SET_FEATURE: // 0xb0191
        status = SetFeatureFilter(Device, Request, InputBufferLength);
        break;
    }
    // No NT_SUCCESS(status) check here since we don't want to fail blocked calls

    // Forward the request down the driver stack
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), &options);
    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}
