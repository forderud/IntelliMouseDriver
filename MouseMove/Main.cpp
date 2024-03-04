#include <Windows.h>
#include <cfgmgr32.h>
#include <strsafe.h>
#include <initguid.h>
#include "../VirtualMouse/Public.h"
#include <iostream>

#pragma comment(lib, "mincore.lib") // for CM_Get_Device_Interface_List..

#define MAX_DEVPATH_LENGTH 256


_Success_(return)
BOOL
GetDevicePath(
    _In_  LPGUID InterfaceGuid,
    _Out_writes_z_(BufLen) PWCHAR DevicePath,
    _In_ size_t BufLen
)
{
    CONFIGRET cr = CR_SUCCESS;
    PWSTR deviceInterfaceList = NULL;
    ULONG deviceInterfaceListLength = 0;
    PWSTR nextInterface;
    HRESULT hr = E_FAIL;
    BOOL bRet = TRUE;

    cr = CM_Get_Device_Interface_List_Size(
        &deviceInterfaceListLength,
        InterfaceGuid,
        NULL,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list size.\n", cr);
        goto clean0;
    }

    if (deviceInterfaceListLength <= 1) {
        bRet = FALSE;
        printf("Error: No active device interfaces found.\n"
            " Is the sample driver loaded?");
        goto clean0;
    }

    deviceInterfaceList = (PWSTR)malloc(deviceInterfaceListLength * sizeof(WCHAR));
    if (deviceInterfaceList == NULL) {
        bRet = FALSE;
        printf("Error allocating memory for device interface list.\n");
        goto clean0;
    }
    ZeroMemory(deviceInterfaceList, deviceInterfaceListLength * sizeof(WCHAR));

    cr = CM_Get_Device_Interface_List(
        InterfaceGuid,
        NULL,
        deviceInterfaceList,
        deviceInterfaceListLength,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list.\n", cr);
        goto clean0;
    }

    nextInterface = deviceInterfaceList + wcslen(deviceInterfaceList) + 1;
    if (*nextInterface != UNICODE_NULL) {
        printf("Warning: More than one device interface instance found. \n"
            "Selecting first matching device.\n\n");
    }

    hr = StringCchCopy(DevicePath, BufLen, deviceInterfaceList);
    if (FAILED(hr)) {
        bRet = FALSE;
        printf("Error: StringCchCopy failed with HRESULT 0x%x", hr);
        goto clean0;
    }

clean0:
    if (deviceInterfaceList != NULL) {
        free(deviceInterfaceList);
    }
    if (CR_SUCCESS != cr) {
        bRet = FALSE;
    }

    return bRet;
}

_Check_return_
_Ret_notnull_
_Success_(return != INVALID_HANDLE_VALUE)
HANDLE
OpenDevice(
    _In_ LPCGUID pguid
)

/*++
Routine Description:

    Called by main() to open an instance of our device.

Arguments:

    pguid - Device interface

Return Value:

    Device handle on success else INVALID_HANDLE_VALUE

--*/

{
    HANDLE hDev;
    WCHAR completeDeviceName[MAX_DEVPATH_LENGTH];

    if (!GetDevicePath(
        (LPGUID)pguid,
        completeDeviceName,
        sizeof(completeDeviceName) / sizeof(completeDeviceName[0])))
    {
        return  INVALID_HANDLE_VALUE;
    }

    printf("DeviceName = (%S)\n", completeDeviceName); fflush(stdout);

    hDev = CreateFile(completeDeviceName,
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        NULL, // default security
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open the device, error - %d", GetLastError()); fflush(stdout);
    }
    else {
        printf("Opened the device successfully.\n"); fflush(stdout);
    }

    return hDev;
}

int main()
{
    printf("About to open device\n"); fflush(stdout);

    HANDLE deviceHandle = OpenDevice((LPGUID)&GUID_DEVINTERFACE_UDE_BACKCHANNEL);

    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find virtual controller device!\n"); fflush(stdout);

        return FALSE;

    }

    printf("Device open, will generate interrupt...\n"); fflush(stdout);

    DEVICE_INTR_FLAGS value = 0;
    ULONG           index = 0;

    if (!DeviceIoControl(deviceHandle,
        IOCTL_UDEFX2_GENERATE_INTERRUPT,
        &value,                // Ptr to InBuffer
        sizeof(value),         // Length of InBuffer
        NULL,                  // Ptr to OutBuffer
        0,                     // Length of OutBuffer
        &index,                // BytesReturned
        0)) {                  // Ptr to Overlapped structure

        DWORD code = GetLastError();
        printf("DeviceIoControl failed with error 0x%x\n", code);
    }
    else
    {
        printf("DeviceIoControl SUCCESS , returned bytes=%d\n", index);
    }

    CloseHandle(deviceHandle);

}
