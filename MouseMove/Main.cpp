#include <Windows.h>
#include <cfgmgr32.h>
#include <strsafe.h>
#include <initguid.h>
#include <wrl/wrappers/corewrappers.h>
#include "../VirtualMouse/Public.h"
#include <iostream>
#include <vector>

#pragma comment(lib, "mincore.lib") // for CM_Get_Device_Interface_List..

// RAII wrapper of file HANDLE objects
using FileHandle = Microsoft::WRL::Wrappers::FileHandle;


std::wstring GetDevicePath(_In_  LPGUID InterfaceGuid)
{
    ULONG deviceInterfaceListLength = 0;
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(
        &deviceInterfaceListLength,
        InterfaceGuid,
        NULL,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list size.\n", cr);
        return {};
    }

    if (deviceInterfaceListLength <= 1) {
        printf("Error: No active device interfaces found.\n"
            " Is the sample driver loaded?");
        return {};
    }

    std::vector<WCHAR> deviceInterfaceList(deviceInterfaceListLength, L'\0');

    cr = CM_Get_Device_Interface_ListW(
        InterfaceGuid,
        NULL,
        deviceInterfaceList.data(),
        deviceInterfaceListLength,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list.\n", cr);
        return {};
    }

    PWSTR nextInterface = deviceInterfaceList.data() + wcslen(deviceInterfaceList.data()) + 1;
    if (*nextInterface != UNICODE_NULL) {
        printf("Warning: More than one device interface instance found. \n"
            "Selecting first matching device.\n\n");
    }

    std::wstring result(deviceInterfaceList.data());
    return result;
}


int main() {
    printf("About to open device\n"); fflush(stdout);

    std::wstring completeDeviceName = GetDevicePath((LPGUID)&GUID_DEVINTERFACE_UDE_BACKCHANNEL);
    if (completeDeviceName.empty()) {
        printf("Unable to find virtual controller device!\n"); fflush(stdout);
        return -2;
    }

    printf("DeviceName = (%S)\n", completeDeviceName.c_str()); fflush(stdout);

    FileHandle deviceHandle(CreateFileW(completeDeviceName.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        NULL, // default security
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL));
    if (!deviceHandle.IsValid()) {
        printf("Failed to open the device, error - %d", GetLastError()); fflush(stdout);
        return -1;
    }

    printf("Device open, will generate interrupt...\n"); fflush(stdout);

    // ask virtual USB device to generate a mouse event
    MOUSE_INPUT_REPORT event = {};
    event.X = 20;
    event.Y = 20;

    ULONG index = 0;
    if (!DeviceIoControl(deviceHandle.Get(),
        IOCTL_UDEFX2_GENERATE_INTERRUPT,
        &event,                // Ptr to InBuffer
        sizeof(event),         // Length of InBuffer
        NULL,                  // Ptr to OutBuffer
        0,                     // Length of OutBuffer
        &index,                // BytesReturned
        0)) {                  // Ptr to Overlapped structure

        DWORD code = GetLastError();
        printf("DeviceIoControl failed with error 0x%x\n", code);
        return -1;
    }

    printf("DeviceIoControl SUCCESS , returned bytes=%d\n", index);
    return 0;
}
