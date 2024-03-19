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


std::wstring GetDevicePath(_In_  GUID InterfaceGuid) {
    const ULONG searchScope = CM_GET_DEVICE_INTERFACE_LIST_PRESENT; // only currently 'live' device interfaces

    ULONG deviceInterfaceListLength = 0;
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&deviceInterfaceListLength, &InterfaceGuid, NULL, searchScope);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list size.\n", cr);
        return {};
    }
    if (deviceInterfaceListLength <= 1) {
        printf("Error: No active device interfaces found.\nIs the sample driver loaded?");
        return {};
    }

    std::wstring deviceInterfaceList(deviceInterfaceListLength, L'\0');
    cr = CM_Get_Device_Interface_ListW(&InterfaceGuid, NULL, deviceInterfaceList.data(), deviceInterfaceListLength, searchScope);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list.\n", cr);
        return {};
    }

    PWSTR nextInterface = deviceInterfaceList.data() + wcslen(deviceInterfaceList.data()) + 1;
    if (*nextInterface != UNICODE_NULL) {
        printf("Warning: More than one device interface instance found. \nSelecting first matching device.\n\n");
    }

    std::wstring result(deviceInterfaceList.data());
    return result;
}


static bool GenerateMouseReport(HANDLE dev, USHORT code) {
    // ask virtual USB device to generate a mouse event
    MOUSE_INPUT_REPORT event = {};
    switch (code) {
    case 32: // space
        event.Buttons = 0x01;
        break;
    case 72: // up
        event.Y = -10;
        break;
    case 75: // left
        event.X = -10;
        break;
    case 77: // right
        event.X = +10;
        break;
    case 80: // down
        event.Y = +10;
        break;
    }

    ULONG index = 0;
    if (!DeviceIoControl(dev,
        IOCTL_UDEFX2_GENERATE_INTERRUPT,
        &event,                // Ptr to InBuffer
        sizeof(event),         // Length of InBuffer
        NULL,                  // Ptr to OutBuffer
        0,                     // Length of OutBuffer
        &index,                // BytesReturned
        0)) {                  // Ptr to Overlapped structure

        DWORD err = GetLastError();
        printf("DeviceIoControl failed with error 0x%x\n", err);
        return false;
    }

    printf("DeviceIoControl SUCCESS , returned bytes=%d\n", index);
    return true;
}

int main() {
    printf("About to open device\n"); fflush(stdout);

    std::wstring completeDeviceName = GetDevicePath(GUID_DEVINTERFACE_UDE_BACKCHANNEL);
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

    printf("Device open.\n");

    {
        BYTE buffer[16] = {};
        BOOL ok = FALSE;

        // test write operation
        DWORD bytesWritten = 0;
        ok = WriteFile(deviceHandle.Get(), &buffer, sizeof(buffer), &bytesWritten, nullptr);
        if (!ok) {
            printf("WriteFile failed, %d", GetLastError());
            return -1;
        }

#if 0
        // test read operation
        // Blocked with "BCHAN Mission request xxxxxxxx pended" event)
        DWORD bytesRead = 0;
        ok = ReadFile(deviceHandle.Get(), &buffer, sizeof(buffer), &bytesRead, nullptr);
        if (!ok) {
            printf("ReadFile failed, %d", GetLastError());
            return -1;
        }
#endif
    }

    printf("Use arrow keys to generate mouse input reports for cursor movement and SPACE to click. Press ESC or Q to quit..\n"); fflush(stdout);
    for (;;) {
        wint_t code = _getwch();

        if (code == 224) { // prefix for arrow key codes
            code = _getwch(); // actual key code
            GenerateMouseReport(deviceHandle.Get(), code);
            continue;
        }

        if (code == 32) { // spacebar
            GenerateMouseReport(deviceHandle.Get(), code); // left button down
            GenerateMouseReport(deviceHandle.Get(), 0);    // left button up
            continue;
        }
        
        if ((code == 27) || (code == 113)) // ESC or 'Q'
            break;
    }

    return 0;
}
