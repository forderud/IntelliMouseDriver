#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <wdfusb.h>
#include <usbdlib.h>
#include <ude/1.0/UdeCx.h>
#include <initguid.h>
#include <usbioctl.h>

#include "trace.h"


// device context
struct USB_CONTEXT {
    WDFDEVICE             ControllerDevice;
    UDECXUSBENDPOINT      UDEFX2ControlEndpoint;
    UDECXUSBENDPOINT      UDEFX2InterruptInEndpoint;
    BOOLEAN               IsAwake;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(USB_CONTEXT, GetUsbDeviceContext);



// ----- descriptor constants/strings/indexes
#define g_InterruptEndpointAddress 0x86  // high-order bit=1 mean IN


extern const UCHAR g_HIDMouseUsbReportDescriptor[];
extern const USHORT g_HIDMouseUsbReportDescriptor_len;

// ------------------------------------------------

NTSTATUS
Usb_Initialize(
    _In_ WDFDEVICE WdfControllerDevice
);


NTSTATUS
Usb_ReadDescriptorsAndPlugIn(
    _In_ WDFDEVICE WdfControllerDevice
);

NTSTATUS
Usb_Disconnect(
    _In_ WDFDEVICE WdfDevice
);

VOID
Usb_Destroy(
    _In_ WDFDEVICE WdfDevice
);

// Private functions
NTSTATUS
UsbCreateEndpointObj(
    _In_   UDECXUSBDEVICE    WdfUsbChildDevice,
    _In_   UCHAR             epAddr,
    _Out_  UDECXUSBENDPOINT *pNewEpObjAddr
);

EVT_UDECX_USB_DEVICE_ENDPOINTS_CONFIGURE              UsbDevice_EvtUsbDeviceEndpointsConfigure;
EVT_UDECX_USB_DEVICE_D0_ENTRY                         UsbDevice_EvtUsbDeviceLinkPowerEntry;
EVT_UDECX_USB_DEVICE_D0_EXIT                          UsbDevice_EvtUsbDeviceLinkPowerExit;
EVT_UDECX_USB_ENDPOINT_RESET                          UsbEndpointReset;
