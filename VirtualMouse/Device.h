/*++
    This file contains the device definitions.
--*/
#pragma once

#include "public.h"
#include "Misc.h"


#define USB_HOST_DEVINTERFACE_REF_STRING L"GUID_DEVINTERFACE_USB_HOST_CONTROLLER"

#define MAX_SUFFIX_SIZE                         (9 * sizeof(WCHAR)) // all ULONGs fit in 9 characters

#define BASE_DEVICE_NAME                        L"\\Device\\MBIMUDEClient"
#define BASE_SYMBOLIC_LINK_NAME                 L"\\DosDevices\\MBIMUDEClient"
//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//


// controller context 
struct UDECX_USBCONTROLLER_CONTEXT {
    LIST_ENTRY ControllerListEntry;
    KEVENT ResetCompleteEvent;
    BOOLEAN AllowOnlyResetInterrupts;
    WDFQUEUE DefaultQueue;
    WRITE_BUFFER_TO_READ_REQUEST_QUEUE missionRequest;
    WRITE_BUFFER_TO_READ_REQUEST_QUEUE missionCompletion;

    PUDECXUSBDEVICE_INIT  ChildDeviceInit;
    UDECXUSBDEVICE        ChildDevice;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(UDECX_USBCONTROLLER_CONTEXT, GetUsbControllerContext);


struct REQUEST_CONTEXT {
	UINT32 unused;
};
WDF_DECLARE_CONTEXT_TYPE(REQUEST_CONTEXT);



// Function to initialize the device and its callbacks
NTSTATUS
UDEFX2CreateDevice(
    _Inout_ PWDFDEVICE_INIT WdfDeviceInit
    );


EVT_WDF_DEVICE_PREPARE_HARDWARE                 ControllerWdfEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE                 ControllerWdfEvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY                         ControllerWdfEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT                          ControllerWdfEvtDeviceD0Exit;
EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED ControllerWdfEvtDeviceD0EntryPostInterruptsEnabled;
EVT_WDF_DEVICE_D0_EXIT_PRE_INTERRUPTS_DISABLED  ControllerWdfEvtDeviceD0ExitPreInterruptsDisabled;
EVT_WDF_OBJECT_CONTEXT_CLEANUP                  ControllerWdfEvtCleanupCallback;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL              ControllerEvtIoDeviceControl;


EVT_UDECX_WDF_DEVICE_QUERY_USB_CAPABILITY         ControllerEvtUdecxWdfDeviceQueryUsbCapability;

NTSTATUS
ControllerCreateWdfDeviceWithNameAndSymLink(
	_Inout_	PWDFDEVICE_INIT * WdfDeviceInit,
	_In_	PWDF_OBJECT_ATTRIBUTES WdfDeviceAttributes,
	_Out_	WDFDEVICE * WdfDevice
);
