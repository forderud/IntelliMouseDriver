#include "driver.h"

/** Driver entry point.
    Initialize the framework and register driver event handlers. */
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    KdPrint(("MouseMirror: DriverEntry - WDF version built on %s %s\n", __DATE__, __TIME__));

    WDF_DRIVER_CONFIG params = {};
    WDF_DRIVER_CONFIG_INIT(/*out*/&params, EvtDriverDeviceAdd);
    params.DriverPoolTag = POOL_TAG;
    params.EvtDriverUnload = EvtDriverUnload;

    // Create the framework WDFDRIVER object, with the handle to it returned in Driver.
    NTSTATUS status = WdfDriverCreate(DriverObject, 
                             RegistryPath, 
                             WDF_NO_OBJECT_ATTRIBUTES, 
                             &params, 
                             WDF_NO_HANDLE); // [out]
    if (!NT_SUCCESS(status)) {
        // Framework will automatically cleanup on error Status return
        DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: Error Creating WDFDRIVER 0x%x\n", status);
    }

    return status;
}


/** Driver unload callback.
    Used to perform operations that must take place before the driver is unloaded.  */
VOID EvtDriverUnload(
    _In_ WDFDRIVER Driver
    )
{
    UNREFERENCED_PARAMETER(Driver);
    KdPrint(("MouseMirror: DriverUnload.\n"));
}
