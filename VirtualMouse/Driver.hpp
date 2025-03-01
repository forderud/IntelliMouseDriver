/*++
    This file contains the driver definitions.
--*/

#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <wdfusb.h>
#include <usbdlib.h>
#include <ude/1.0/UdeCx.h>
#include <initguid.h>
#include <usbioctl.h>

#include "device.hpp"
#include "trace.h"


#define POOL_TAG 'UDEX'

// WDFDRIVER Events
extern "C"
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD UDEFX2EvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP UDEFX2EvtDriverContextCleanup;
