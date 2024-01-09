//#include "debug.h"
#include "driver.h"
#include <Hidport.h>

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlFilter;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT EvtDeviceSelfManagedInitFilter;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT EvtDeviceSelfManagedInitDummy;
//EVT_WDF_REQUEST_COMPLETION_ROUTINE EvtDeviceSelfManagedInitCompletionRoutine;
EVT_WDF_REQUEST_COMPLETION_ROUTINE EvtSetBlackCompletionRoutine;
EVT_WDF_WORKITEM    EvtSetBlackWorkItem;

NTSTATUS DummyWorkItem(WDFWORKITEM workItem) {
    UNREFERENCED_PARAMETER(workItem);
    TRACE_FN_EXIT;
    return STATUS_SUCCESS;
}

VOID EvtSetBlackWorkItem(WDFWORKITEM workItem) {

    TRACE_FN_ENTRY
    
    PSET_BLACK_WORK_ITEM_CONTEXT pContext = Get_SetBlackWorkItemContext(workItem);
    WDFDEVICE device = static_cast<WDFDEVICE>(WdfWorkItemGetParentObject(workItem));

    pContext->resultStatus = SetBlack(device);

    TRACE_FN_EXIT
}

NTSTATUS EvtDeviceSelfManagedInitDummy(_In_ WDFDEVICE Device) {
    UNREFERENCED_PARAMETER(Device);
    TRACE_FN_EXIT;
    return STATUS_SUCCESS;
}

NTSTATUS EvtDeviceSelfManagedInitFilter(_In_ WDFDEVICE Device) {
    
    PSET_BLACK_WORK_ITEM_CONTEXT pWorkItemContext = NULL;
    WDF_OBJECT_ATTRIBUTES        workItemAttributes;
    WDF_WORKITEM_CONFIG          workItemConfig;
    WDFWORKITEM                  hWorkItem;

    NTSTATUS status = STATUS_PNP_DRIVER_CONFIGURATION_INCOMPLETE;

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPF_TRACE_LEVEL, "TailLight: %s\n", __func__));

    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = Device;
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&workItemAttributes,
        SET_BLACK_WORK_ITEM_CONTEXT);

    WDF_WORKITEM_CONFIG_INIT(&workItemConfig, EvtSetBlackWorkItem);

    status = WdfWorkItemCreate(&workItemConfig,
        &workItemAttributes,
        &hWorkItem);

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
            DPF_CONTINUABLE_FAILURE,
            "TailLight: Workitem creation failure NTSTATUS=0x%x\n",
            status));
        return status;
    }

    pWorkItemContext = Get_SetBlackWorkItemContext(hWorkItem);
    pWorkItemContext->owningDevice = Device;

    WdfWorkItemEnqueue(hWorkItem);

    TRACE_FN_EXIT

    return status;
}

NTSTATUS CreateQueue(WDFDEVICE device)
{
    NTSTATUS status = STATUS_FAILED_DRIVER_ENTRY;

    // create queue to process IOCTLs
    WDF_IO_QUEUE_CONFIG queueConfig = {};
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel); // don't synchronize
    //queueConfig.EvtIoRead // pass-through read requests 
    //queueConfig.EvtIoWrite // pass-through write requests
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControlFilter; // filter I/O device control requests
    //queueConfig.PowerManaged = WdfFalse;    // Otherwise don't get PNP notifications

    WDFQUEUE queue = 0; // auto-deleted when parent is deleted
    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoQueueCreate failed 0x%x\n", status));
    }

    return status;
}

 NTSTATUS CreateTaillightDevice(_Inout_ PWDFDEVICE_INIT DeviceInit, 
     _Out_ WDFDEVICE *device) {
     
     NTSTATUS status = STATUS_FAILED_DRIVER_ENTRY;

     // Configure the device as a filter driver
     WdfFdoInitSetFilter(DeviceInit);

    // Initialize the PNP filter. The device returns that it's in an invalid state
    // If the IOCTL is sent too early. Therefore, we'll hitch a ride on self managed
    // init or restart (EvtDeviceSelfManagedInit).
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks = {};
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtDeviceSelfManagedInitFilter;
    //pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtDeviceSelfManagedInitDummy;
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

NTSTATUS OpenPDO(DEVICE_CONTEXT* pDeviceContext,
    WDFIOTARGET hidTarget)
{
    NTSTATUS status;
    WDF_IO_TARGET_OPEN_PARAMS   openParams;

    KdPrint(("TailLight: Try to open PDO: %wZ\n", pDeviceContext->PdoName));

    //
    // Open it up, write access only!
    //
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
        &openParams,
        &pDeviceContext->PdoName,
        FILE_WRITE_ACCESS);

    //
    // We will let the framework to respond automatically to the pnp
    // state changes of the target by closing and opening the handle.
    //
    openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

    status = WdfIoTargetOpen(hidTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoTargetOpen failed 0x%x\n", status));

    }

    return status;
}

void EvtSetBlackCompletionRoutine(_In_
    WDFREQUEST Request,
    _In_
    WDFIOTARGET Target,
    _In_
    PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_
    WDFCONTEXT Context)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    TRACE_FN_ENTRY
    TRACE_FN_EXIT
}


NTSTATUS SetBlack(WDFDEVICE device)
{
    WDF_MEMORY_DESCRIPTOR inputDescriptor;
    PCHAR               report = NULL;
    NTSTATUS            status = STATUS_FAILED_DRIVER_ENTRY;
    WDFREQUEST          request = NULL;
    WDFIOTARGET         hidTarget = NULL;

    WDFMEMORY InBuffer = 0;
    BYTE* pInBuffer = nullptr;

    PAGED_CODE();

    KdPrint(("TailLight: %s\n", __func__));

    // Set up a WDF memory object for the IOCTL request
    WDF_OBJECT_ATTRIBUTES mem_attrib = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&mem_attrib);

    WDF_REQUEST_SEND_OPTIONS sendOptions = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS |
        WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(1));

    /*status = WdfIoTargetCreate(device,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hidTarget);
     if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfDeviceGetIoTarget failed 0x%x\n", status));
        return status;
    } */

    hidTarget = WdfDeviceGetIoTarget(device);

    //
    // Create a report to send to the device.
    //
    report = (PCHAR)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, 
        sizeof(TailLightReport), 
        POOL_TAG_TAILLIGHT_REPORT);

    if (report == NULL) {
        KdPrint(("TailLight: Can't create Tail Light report.\n"));
        goto ExitAndFree;
    }
    
    TailLightReport* pTailLightReport = (TailLightReport*)report;
    pTailLightReport->Blue = 0;
    pTailLightReport->Green = 0;
    pTailLightReport->Red = 0;
    
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor,
        report,
        sizeof(TailLightReport));

    status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES,
        hidTarget,
        &request);

    WdfRequestSetCompletionRoutine(request,
        EvtSetBlackCompletionRoutine,
        WDF_NO_CONTEXT);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfRequestCreate failed 0x%x\n", status));
        goto ExitAndFree;
    }

    mem_attrib.ParentObject = request; // auto-delete with request
    
    status = WdfMemoryCreate(&mem_attrib, 
        NonPagedPool, 
        POOL_TAG, 
        sizeof(TailLightReport), 
        &InBuffer, 
        (void**)&pInBuffer);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfMemoryCreate failed: 0x%x\n", status));
        return status;
    }
    RtlCopyMemory(pInBuffer, &report, sizeof(report));

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
        return status;
    }

    BOOLEAN ret = WdfRequestSend(request, hidTarget, &sendOptions);
    TRACE_REQUEST_BOOL(ret)
    if (ret == FALSE) {
        status = WdfRequestGetStatus(request);
        TRACE_REQUEST_FAILURE(status)
        goto ExitAndFree;
    }

ExitAndFree:
    if (report != NULL) {
        ExFreePool(report);
        report = NULL;
    } 

    if (request != NULL) {
        WdfObjectDelete(request);
        request = NULL;
    }

    TRACE_FN_EXIT

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

    RequestFlags incomingRequestFlags = (RequestFlags)WdfRequestGetInformation(Request);
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
    KdPrint(("TailLight: Request Flags: 0x%x\n", incomingRequestFlags));


    switch (IoControlCode) {
    case IOCTL_HID_SET_FEATURE: // 0xb0191
        // Ensure that we send this driver-owned request down in all failure cases.
        //
        // Now get the capabilities.
        //

        //RtlZeroMemory(&caps, sizeof(HIDP_CAPS));

        //status = HidP_GetCaps(preparsedData, &caps);

        /*if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: HidP_GetCaps failed 0x%x, punting\n",
                status));
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
            break;
        }*/

        //
        // Create a report to send to the device.
        //
        packet = (TailLightReport*)ExAllocatePool2(
            POOL_FLAG_NON_PAGED, sizeof(TailLightReport), POOL_TAG);
        if (packet == NULL) {
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
            break;
        }

        status = SetFeatureFilter(device, Request, *packet, InputBufferLength);
        if (!NT_SUCCESS(status)) {
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

        // TODO: Replace with better name
        WdfRequestSetCompletionRoutine(Request,
            EvtSetBlackCompletionRoutine,
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

    if (packet != nullptr) {
        ExFreePool(packet);
        packet = NULL;
    }

    if (inBuffer != 0) {
        WdfObjectDelete(inBuffer);
        inBuffer = 0;
    }
}
