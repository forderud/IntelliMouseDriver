#include "device.hpp"
#include <Hidport.h>
#include "vfeature.hpp"
#include "wmi.hpp"



VOID PollInputReportsTimer(_In_ WDFTIMER  Timer) {
    DebugEnter();

    WDFDEVICE Device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
    NT_ASSERTMSG("PollInputReports Device NULL\n", Device);

    NTSTATUS status = SetFeatureColor(Device, 0);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: SetFeatureColor failure 0x%x"), status);
        return;
    }

    status = PollInputReports(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: PollInputReports failure NTSTATUS=0x%x"), status);
        return;
    }

    DebugExit();
}

NTSTATUS EvtSelfManagedIoInit(WDFDEVICE Device) {
    // Initialize tail-light to black to have control over HW state
    WDF_TIMER_CONFIG timerCfg = {};
    WDF_TIMER_CONFIG_INIT(&timerCfg, PollInputReportsTimer);

    WDF_OBJECT_ATTRIBUTES attribs = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
    attribs.ParentObject = Device;
    attribs.ExecutionLevel = WdfExecutionLevelPassive; // required to access HID functions

    WDFTIMER timer = nullptr;
    NTSTATUS status = WdfTimerCreate(&timerCfg, &attribs, &timer);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfTimerCreate failed 0x%x"), status);
        return status;
    }

    BOOLEAN inQueue = WdfTimerStart(timer, 0); // no wait
    NT_ASSERTMSG("TailLight: timer already in queue", !inQueue);
    UNREFERENCED_PARAMETER(inQueue);

    return status;
}


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

    {
        // register PnP callbacks (must be done before WdfDeviceCreate)
        WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
        PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtSelfManagedIoInit;
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);
    }

    // configure buffered IO
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

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

    // Initialize WMI provider
    NTSTATUS status = WmiInitialize(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: Error initializing WMI 0x%x"), status);
        return status;
    }

    KeInitializeEvent(&deviceContext->SelfTestCompleted, SynchronizationEvent, FALSE);

    return status;
}
