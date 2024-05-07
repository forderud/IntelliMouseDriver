#include "Misc.h"
#include "Driver.h"
#include "Device.h"
#include "usbdevice.h"
#include "USBCom.h"
#include "ucx/1.4/ucxobjects.h"
#include <Hidport.h>
#include "usbdevice.tmh"


// START ------------------ descriptor -------------------------------

DECLARE_CONST_UNICODE_STRING(g_ManufacturerStringEnUs, L"Microsoft");
#define g_ManufacturerIndex   0

DECLARE_CONST_UNICODE_STRING(g_ProductStringEnUs, L"UDE Client");
#define g_ProductIndex        0


const USHORT AMERICAN_ENGLISH = 0x0409;

const USB_STRING_DESCRIPTOR g_LanguageDescriptor = {
    sizeof(g_LanguageDescriptor), // bLength
    USB_STRING_DESCRIPTOR_TYPE,   // bDescriptorType
    AMERICAN_ENGLISH              // bString[1]
};


const USB_DEVICE_DESCRIPTOR g_UsbDeviceDescriptor = {
    sizeof(USB_DEVICE_DESCRIPTOR),   // Descriptor size
    USB_DEVICE_DESCRIPTOR_TYPE,      // Device descriptor type
    0x0110,                          // USB 1.1
    0x00,                            // Device class (interface-class defined)
    0x00,                            // Device subclass
    0x00,                            // Device protocol
    0x08,                            // Maxpacket size for EP0
    0x04F3,                          // Vendor ID (VID_04F3 -Elan Microelectronics)
    0x0235,                          // Product ID (PID_0235 - Optical Mouse)
    0x2458,                          // firmware revision
    g_ManufacturerIndex,             // Manufacture string index
    g_ProductIndex,                  // Product string index
    0x00,                            // Serial number string index
    0x01                             // Number of configurations
};

struct UsbDevDesc {
    USB_CONFIGURATION_DESCRIPTOR cfg;
    USB_INTERFACE_DESCRIPTOR intf;
    HID_DESCRIPTOR hid;
    USB_ENDPOINT_DESCRIPTOR ep;
};
static_assert(sizeof(UsbDevDesc) == sizeof(USB_CONFIGURATION_DESCRIPTOR) + sizeof(USB_INTERFACE_DESCRIPTOR) + sizeof(HID_DESCRIPTOR) + sizeof(USB_ENDPOINT_DESCRIPTOR)); // verify that struct is packed

const UsbDevDesc g_UsbConfigDescriptorSet = {
    {
        // Configuration Descriptor Type
        sizeof(USB_CONFIGURATION_DESCRIPTOR),// Descriptor Size
        USB_CONFIGURATION_DESCRIPTOR_TYPE,   // Configuration Descriptor Type
        sizeof(UsbDevDesc),                  // Length of this descriptor and all sub descriptors (34 bytes)
        0x01,                                // Number of interfaces
        0x01,                                // Configuration number
        0x00,                                // Configuration string index
        0xA0,                                // Config characteristics - Bus Powered, Remote Wakeup
        0x32,                                // Max power consumption of device (in 2mA unit) : 100 mA
    },
    {
        // Interface  descriptor
        sizeof(USB_INTERFACE_DESCRIPTOR),  // Descriptor size
        USB_INTERFACE_DESCRIPTOR_TYPE,     // Interface Association Descriptor Type
        0,                                 // bInterfaceNumber
        0,                                 // bAlternateSetting
        1,                                 // bNumEndpoints
        0x03,                              // bInterfaceClass (HID)
        0x01,                              // bInterfaceSubClass (Boot Interface)
        0x02,                              // bInterfaceProtocol (Mouse)
        0x00,                              // iInterface
    },
    {
        // HID Descriptor
        sizeof(HID_DESCRIPTOR), // Descriptor size
        HID_HID_DESCRIPTOR_TYPE,// bDescriptorType (HID)
        0x0111,                 // HID Class Spec Version
        0x00,                   // bCountryCode
        0x01,                   // bNumDescriptors
        0x22,                   // bDescriptorType (Report)
        0x003E,                 // wDescriptorLength
    },
    {
        // Interrupt IN endpoint descriptor
        sizeof(USB_ENDPOINT_DESCRIPTOR),// Descriptor size 
        USB_ENDPOINT_DESCRIPTOR_TYPE,   // Descriptor type
        g_InterruptEndpointAddress,     // Endpoint address and description
        USB_ENDPOINT_TYPE_INTERRUPT,    // bmAttributes - interrupt
        0x0004,                         // Max packet size = 4 bytes
        0x0A                            // Servicing interval for interrupt (10 ms/1 frame)
    }
};


// Interface 0 HID Report Descriptor Mouse
const UCHAR g_HIDMouseUsbReportDescriptor[] = {
    0x05, 0x01, // Usage Page (Generic Desktop)
    0x09, 0x02, // Usage(Mouse)
    0xA1, 0x01, // Collection(Application)
    0x09, 0x01, // Usage(Pointer)
    0xA1, 0x00, // Collection(Physical)
    0x05, 0x09, // Usage Page(Button)
    0x19, 0x01, // Usage Minimum(Button 1)
    0x29, 0x03, // Usage Maximum(Button 3)
    0x15, 0x00, // Logical Minimum(0)
    0x25, 0x01, // Logical Maximum(1)
    0x95, 0x03, // Report Count(3)
    0x75, 0x01, // Report Size(1)
    0x81, 0x02, // Input(Data, Var, Abs, NWrp, Lin, Pref, NNul, Bit)
    0x95, 0x05, // Report Count(5)
    0x75, 0x01, // Report Size(1)
    0x81, 0x03, // Input(Cnst, Var, Abs, NWrp, Lin, Pref, NNul, Bit)
    0x05, 0x01, // Usage Page(Generic Desktop)
    0x09, 0x30, // Usage(X)
    0x09, 0x31, // Usage(Y)
    0x15, 0x81, // Logical Minimum(-127)
    0x25, 0x7F, // Logical Maximum(127)
    0x75, 0x08, // Report Size(8)
    0x95, 0x02, // Report Count(2)
    0x81, 0x06, // Input(Data, Var, Rel, NWrp, Lin, Pref, NNul, Bit)
    0x09, 0x38, // Usage(Wheel)
    0x15, 0x81, // Logical Minimum(-127)
    0x25, 0x7F, // Logical Maximum(127)
    0x75, 0x08, // Report Size(8)
    0x95, 0x01, // Report Count(1)
    0x81, 0x06, // Input(Data, Var, Rel, NWrp, Lin, Pref, NNul, Bit)
    0xC0,       // End Collection
    0xC0        // End Collection
};

const USHORT g_HIDMouseUsbReportDescriptor_len = sizeof(g_HIDMouseUsbReportDescriptor);

// END ------------------ descriptor -------------------------------


NTSTATUS
Usb_Initialize(
    _In_ WDFDEVICE WdfDevice
)
{
    NTSTATUS                                status;
    UDECX_USB_DEVICE_STATE_CHANGE_CALLBACKS   callbacks;

    // Allocate per-controller private contexts used by other source code modules (I/O,
    // etc.)
    UDECX_USBCONTROLLER_CONTEXT* controllerContext = GetUsbControllerContext(WdfDevice);

    controllerContext->ChildDeviceInit = UdecxUsbDeviceInitAllocate(WdfDevice);

    if (controllerContext->ChildDeviceInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate UDECXUSBDEVICE_INIT %!STATUS!", status);
        return status;
    }

    // State changed callbacks
    UDECX_USB_DEVICE_CALLBACKS_INIT(&callbacks);

    callbacks.EvtUsbDeviceLinkPowerEntry = UsbDevice_EvtUsbDeviceLinkPowerEntry;
    callbacks.EvtUsbDeviceLinkPowerExit = UsbDevice_EvtUsbDeviceLinkPowerExit;

    UdecxUsbDeviceInitSetStateChangeCallbacks(controllerContext->ChildDeviceInit, &callbacks);

    // Set required attributes.
    UdecxUsbDeviceInitSetSpeed(controllerContext->ChildDeviceInit, UdecxUsbHighSpeed);

    UdecxUsbDeviceInitSetEndpointsType(controllerContext->ChildDeviceInit, UdecxEndpointTypeSimple);

    // Device descriptor
    status = UdecxUsbDeviceInitAddDescriptor(controllerContext->ChildDeviceInit, (PUCHAR)&g_UsbDeviceDescriptor, sizeof(g_UsbDeviceDescriptor));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // String descriptors
    status = UdecxUsbDeviceInitAddDescriptorWithIndex(controllerContext->ChildDeviceInit, (PUCHAR)&g_LanguageDescriptor, sizeof(g_LanguageDescriptor), 0);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = UdecxUsbDeviceInitAddStringDescriptor(controllerContext->ChildDeviceInit, &g_ManufacturerStringEnUs, g_ManufacturerIndex, AMERICAN_ENGLISH);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = UdecxUsbDeviceInitAddStringDescriptor(controllerContext->ChildDeviceInit, &g_ProductStringEnUs, g_ProductIndex, AMERICAN_ENGLISH);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Remaining init requires lower edge interaction.  Postpone to Usb_ReadDescriptorsAndPlugIn.

    // On failure in this function (or later but still before creating the UDECXUSBDEVICE),
    // UdecxUsbDeviceInit will be freed by Usb_Destroy.
    return status;
}


NTSTATUS
Usb_ReadDescriptorsAndPlugIn(
    _In_ WDFDEVICE WdfControllerDevice
)
{
    NTSTATUS                          status;
    PUSB_CONFIGURATION_DESCRIPTOR     pComputedConfigDescSet;
    USB_CONTEXT*                      deviceContext = NULL;
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS  pluginOptions;

    UDECX_USBCONTROLLER_CONTEXT* controllerContext = GetUsbControllerContext(WdfControllerDevice);
    pComputedConfigDescSet = NULL;

    // Compute configuration descriptor dynamically.
    pComputedConfigDescSet = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(g_UsbConfigDescriptorSet), POOL_TAG);

    if (pComputedConfigDescSet == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate %d bytes for temporary config descriptor %!STATUS!",
            sizeof(g_UsbConfigDescriptorSet), status);
        goto exit;
    }

    RtlCopyMemory(pComputedConfigDescSet, &g_UsbConfigDescriptorSet, sizeof(g_UsbConfigDescriptorSet));

    status = UdecxUsbDeviceInitAddDescriptor(controllerContext->ChildDeviceInit, (PUCHAR)pComputedConfigDescSet, sizeof(g_UsbConfigDescriptorSet));
    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    {
        // Create emulated USB device
        WDF_OBJECT_ATTRIBUTES attributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, USB_CONTEXT);

        status = UdecxUsbDeviceCreate(&controllerContext->ChildDeviceInit, &attributes, &(controllerContext->ChildDevice));
        if (!NT_SUCCESS(status)) {
            goto exit;
        }
    }

    {
        WDF_OBJECT_ATTRIBUTES attributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, IO_CONTEXT);

        status = WdfObjectAllocateContext(controllerContext->ChildDevice, &attributes, NULL);
        if (!NT_SUCCESS(status)) {
            LogError(TRACE_DEVICE, "Unable to allocate new context for WDF object %p", controllerContext->ChildDevice);
            goto exit;
        }
    }

    deviceContext = GetUsbDeviceContext(controllerContext->ChildDevice);

    // create link to parent
    deviceContext->ControllerDevice = WdfControllerDevice;


    LogInfo(TRACE_DEVICE, "USB device created, controller=%p, UsbDevice=%p", WdfControllerDevice, controllerContext->ChildDevice);

    deviceContext->IsAwake = TRUE;  // for some strange reason, it starts out awake!

    // Create static endpoints.
    status = UsbCreateEndpointObj(controllerContext->ChildDevice, USB_DEFAULT_ENDPOINT_ADDRESS, &(deviceContext->UDEFX2ControlEndpoint) );
    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    status = UsbCreateEndpointObj(controllerContext->ChildDevice, g_InterruptEndpointAddress, &(deviceContext->UDEFX2InterruptInEndpoint));
    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    // This begins USB communication and prevents us from modifying descriptors and simple endpoints.
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS_INIT(&pluginOptions);
    pluginOptions.Usb20PortNumber = 1;
    status = UdecxUsbDevicePlugIn(controllerContext->ChildDevice, &pluginOptions);

    LogInfo(TRACE_DEVICE, "Usb_ReadDescriptorsAndPlugIn ends successfully");

exit:
    // Free temporary allocation always.
    if (pComputedConfigDescSet != NULL) {
        ExFreePoolWithTag(pComputedConfigDescSet, POOL_TAG);
        pComputedConfigDescSet = NULL;
    }

    return status;
}

NTSTATUS
Usb_Disconnect(
    _In_  WDFDEVICE WdfDevice
)
{
    NTSTATUS status;
    IO_CONTEXT ioContextCopy;

    UDECX_USBCONTROLLER_CONTEXT* controllerCtx = GetUsbControllerContext(WdfDevice);

    {
        // Stop deferred processing
        IO_CONTEXT* pIoContext = WdfDeviceGetIoContext(controllerCtx->ChildDevice);

        // plus this queue will no longer accept incoming requests
        WdfIoQueuePurgeSynchronously(pIoContext->IntrDeferredQueue);

        ioContextCopy = *pIoContext;
    }

    status = UdecxUsbDevicePlugOutAndDelete(controllerCtx->ChildDevice);
    // Not deleting the queues that belong to the controller, as this
    // happens only in the last disconnect.  But if we were to connect again,
    // we would need to do that as the queues would leak.

    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE, "UdecxUsbDevicePlugOutAndDelete failed with %!STATUS!", status);
        return status;
    }

    WdfObjectDelete(ioContextCopy.IntrDeferredQueue);

    WdfIoQueuePurgeSynchronously(ioContextCopy.ControlQueue);
    WdfObjectDelete(ioContextCopy.ControlQueue);

    WdfIoQueuePurgeSynchronously(ioContextCopy.InterruptUrbQueue);
    WdfObjectDelete(ioContextCopy.InterruptUrbQueue);

    LogInfo(TRACE_DEVICE, "Usb_Disconnect ends successfully");
    return status;
}


VOID
Usb_Destroy(
    _In_ WDFDEVICE WdfDevice
)
{
    UDECX_USBCONTROLLER_CONTEXT* pControllerContext = GetUsbControllerContext(WdfDevice);

    // Free device init in case we didn't successfully create the device.
    if (pControllerContext != NULL && pControllerContext->ChildDeviceInit != NULL) {
        UdecxUsbDeviceInitFree(pControllerContext->ChildDeviceInit);
        pControllerContext->ChildDeviceInit = NULL;
    }
    LogError(TRACE_DEVICE, "Usb_Destroy ends successfully");

    return;
}


NTSTATUS
UsbCreateEndpointObj(
    _In_   UDECXUSBDEVICE    WdfUsbChildDevice,
    _In_   UCHAR             epAddr,
    _Out_  UDECXUSBENDPOINT *pNewEpObjAddr
)
{
    NTSTATUS                      status;
    WDFQUEUE                      epQueue;
    UDECX_USB_ENDPOINT_CALLBACKS  callbacks;
    PUDECXUSBENDPOINT_INIT        endpointInit;

    endpointInit = NULL;

    status = Io_RetrieveEpQueue(WdfUsbChildDevice, epAddr, &epQueue);

    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    endpointInit = UdecxUsbSimpleEndpointInitAllocate(WdfUsbChildDevice);
    if (endpointInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate endpoint init %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointInitSetEndpointAddress(endpointInit, epAddr);

    UDECX_USB_ENDPOINT_CALLBACKS_INIT(&callbacks, UsbEndpointReset);
    UdecxUsbEndpointInitSetCallbacks(endpointInit, &callbacks);

    status = UdecxUsbEndpointCreate(&endpointInit, WDF_NO_OBJECT_ATTRIBUTES, pNewEpObjAddr);
    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE, "UdecxUsbEndpointCreate failed for endpoint %x, %!STATUS!", epAddr, status);
        goto exit;
    }

    UdecxUsbEndpointSetWdfIoQueue( *pNewEpObjAddr,  epQueue);

exit:
    if (endpointInit != NULL) {
        NT_ASSERT(!NT_SUCCESS(status));
        UdecxUsbEndpointInitFree(endpointInit);
        endpointInit = NULL;
    }

    return status;
}


VOID
UsbEndpointReset(
    _In_ UDECXUSBENDPOINT UdecxUsbEndpoint,
    _In_ WDFREQUEST     Request
)
{
    UNREFERENCED_PARAMETER(UdecxUsbEndpoint);
    UNREFERENCED_PARAMETER(Request);
}



VOID
UsbDevice_EvtUsbDeviceEndpointsConfigure(
    _In_ UDECXUSBDEVICE                    UdecxUsbDevice,
    _In_ WDFREQUEST                        Request,
    _In_ PUDECX_ENDPOINTS_CONFIGURE_PARAMS Params
)
{
    UNREFERENCED_PARAMETER(UdecxUsbDevice);
    UNREFERENCED_PARAMETER(Params);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

NTSTATUS
UsbDevice_EvtUsbDeviceLinkPowerEntry(
    _In_ WDFDEVICE       UdecxWdfDevice,
    _In_ UDECXUSBDEVICE    UdecxUsbDevice )
{
    UNREFERENCED_PARAMETER(UdecxWdfDevice);

    USB_CONTEXT* pUsbContext = GetUsbDeviceContext(UdecxUsbDevice);
    IO_CONTEXT* pIoContext = WdfDeviceGetIoContext(UdecxUsbDevice);

    // this will result in all current requests being canceled
    LogInfo(TRACE_DEVICE, "About to re-start paused deferred queue");
    WdfIoQueueStart(pIoContext->IntrDeferredQueue);

    pUsbContext->IsAwake = TRUE;
    LogInfo(TRACE_DEVICE, "USB Device power ENTRY");

    return STATUS_SUCCESS;
}

NTSTATUS
UsbDevice_EvtUsbDeviceLinkPowerExit(
    _In_ WDFDEVICE UdecxWdfDevice,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _In_ UDECX_USB_DEVICE_WAKE_SETTING WakeSetting )
{
    UNREFERENCED_PARAMETER(UdecxWdfDevice);

    USB_CONTEXT* pUsbContext = GetUsbDeviceContext(UdecxUsbDevice);
    pUsbContext->IsAwake = FALSE;
    IO_CONTEXT* pIoContext = WdfDeviceGetIoContext(UdecxUsbDevice);

    // thi will result in all current requests being canceled
    LogInfo(TRACE_DEVICE, "About to purge deferred request queue");
    WdfIoQueuePurge(pIoContext->IntrDeferredQueue, NULL, NULL);

    LogInfo(TRACE_DEVICE, "USB Device power EXIT [wdfDev=%p, usbDev=%p], WakeSetting=%x", UdecxWdfDevice, UdecxUsbDevice, WakeSetting);
    return STATUS_SUCCESS;
}
