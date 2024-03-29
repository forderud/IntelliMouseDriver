/*++
    This module contains the common declarations shared by driver
    and user applications.

Environment:
    user and kernel
--*/
#pragma once

// Define an Interface Guid so that apps can find the device and talk to it.
DEFINE_GUID(GUID_DEVINTERFACE_UDE_BACKCHANNEL,
    0x18b07599, 0x2515, 0x4c18, 0xb9, 0xe3, 0x6e, 0x16, 0xf8, 0x2a, 0xb4, 0x7f);



#define IOCTL_INDEX_UDEFX2C   0x900
#define FILE_DEVICE_UDEFX2C   65600U

#define IOCTL_UDEFX2_GENERATE_INTERRUPT  CTL_CODE(FILE_DEVICE_UDEFX2C,     \
                                                  IOCTL_INDEX_UDEFX2C + 5,     \
                                                  METHOD_BUFFERED,         \
                                                  FILE_READ_ACCESS)

#pragma pack(push, 1)
struct MOUSE_INPUT_REPORT {
    UINT8 Buttons;
    INT8 X;
    INT8 Y;
    INT8 Wheel;
};
#pragma pack(pop)
static_assert(sizeof(MOUSE_INPUT_REPORT) == 4, "MOUSE_INPUT_REPORT size mismatch"); // must match report size in g_HIDMouseUsbReportDescriptor
