/*++
    This file contains the driver entry points and callbacks.
--*/
#include "driver.hpp"
#include "driver.tmh"


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:
    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.
--*/
{
    // Initialize WPP Tracing
    WPP_INIT_TRACING(DriverObject, RegistryPath);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    WDF_OBJECT_ATTRIBUTES attributes = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = UDEFX2EvtDriverContextCleanup;

    WDF_DRIVER_CONFIG config = {};
    WDF_DRIVER_CONFIG_INIT(&config, UDEFX2EvtDeviceAdd);
    config.DriverPoolTag = POOL_TAG;

    NTSTATUS status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

NTSTATUS
UDEFX2EvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:
    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.
--*/
{
    UNREFERENCED_PARAMETER(Driver);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    NTSTATUS status = UDEFX2CreateDevice(DeviceInit);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

VOID
UDEFX2EvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
/*++
Routine Description:
    Free all the resources allocated in DriverEntry.

Arguments:
    DriverObject - handle to a WDF Driver object.
--*/
{
    UNREFERENCED_PARAMETER(DriverObject);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Stop WPP Tracing
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
