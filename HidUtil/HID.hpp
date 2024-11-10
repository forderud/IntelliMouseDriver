#pragma once
#include <windows.h>
#include <cfgmgr32.h> // for CM_Get_Device_Interface_List
#include <Hidclass.h> // combine with INITGUID define
#include <hidsdi.h>
#include <wrl/wrappers/corewrappers.h>

#include <cassert>
#include <string>
#include <vector>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "mincore.lib")

namespace hid {

/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PreparsedData {
public:
    PreparsedData() = default;

    PreparsedData(HANDLE hid_dev) {
        Open(hid_dev);
    }
    ~PreparsedData() {
        Close();
    }

    BOOLEAN Open(HANDLE hid_dev) {
        Close();
        return HidD_GetPreparsedData(hid_dev, &report);
    }
    void Close() {
        if (report) {
            HidD_FreePreparsedData(report);
            report = nullptr;
        }
    }

    PreparsedData(PreparsedData&& obj) noexcept {
        std::swap(report, obj.report);
    }

    operator PHIDP_PREPARSED_DATA() const {
        return report;
    }

private:
    PHIDP_PREPARSED_DATA report = nullptr; // report descriptor for top-level collection
};

class Device {
public:
    Device() = default;

    Device(const wchar_t* deviceName) : devName(deviceName) {
        dev.Attach(CreateFileW(deviceName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL));
        if (!dev.IsValid()) {
            DWORD err = GetLastError(); err;
            //assert(err != ERROR_ACCESS_DENIED); // (5) observed for already used devices
            //assert(err != ERROR_SHARING_VIOLATION); // (32)
            //wprintf(L"WARNING: CreateFile failed: (err %d) for %ls\n", err, deviceName);
            return;
        }

        if (!HidD_GetAttributes(dev.Get(), &attr)) {
            DWORD err = GetLastError(); err;
            assert(err == ERROR_NOT_FOUND);
            dev.Close();
            return;
        }

        preparsed.Open(dev.Get());

        if (HidP_GetCaps(preparsed, &caps) != HIDP_STATUS_SUCCESS)
            abort();
    }

    bool IsValid() const {
        return dev.IsValid();
    }

    std::wstring GetManufacturerString() const {
        wchar_t man_buffer[128] = L"<unknown>";
        HidD_GetManufacturerString(dev.Get(), man_buffer, (ULONG)std::size(man_buffer)); // ignore errors
        return man_buffer;
    }

    std::wstring GetProductString() const {
        wchar_t prod_buffer[128] = L"<unknown>";
        HidD_GetProductString(dev.Get(), prod_buffer, (ULONG)std::size(prod_buffer)); // ignore erorrs
        return prod_buffer;
    }

    /** Get FEATURE report. */
    template <class T>
    T GetFeature() const {
        T report; // assume report ID prefix on first byte
        assert(sizeof(report) == caps.FeatureReportByteLength+1);

        BOOLEAN ok = HidD_GetFeature(dev.Get(), &report, sizeof(report));
        if (!ok) {
            DWORD err = GetLastError();
            printf("ERROR: HidD_GetFeature failure (err %d).\n", err);
            assert(ok);
            return {};
        }

        return report;
    }

    /** Set FEATURE report. */
    template <class T>
    bool SetFeature(const T& report) {
        assert(sizeof(report) == caps.FeatureReportByteLength + 1);
        BOOLEAN ok = HidD_SetFeature(dev.Get(), const_cast<void*>(static_cast<const void*>(&report)), sizeof(report));
        if (!ok) {
            DWORD err = GetLastError();
            printf("ERROR: HidD_SetFeature failure (err %d).\n", err);
            assert(ok);
            return {};
        }

        return ok;
    }

public:
    std::wstring devName;
    Microsoft::WRL::Wrappers::FileHandle dev;

    HIDD_ATTRIBUTES attr = {}; // VendorID, ProductID, VersionNumber
    PreparsedData preparsed; // opaque ptr
    HIDP_CAPS caps = {}; // Usage, UsagePage, report sizes
};

/** Human Interface Devices (HID) device search class. */
class Query {
public:
    struct Criterion {
        USHORT VendorID = 0;
        USHORT ProductID = 0;
        USHORT Usage = 0;
        USHORT UsagePage = 0;
    };

    static std::vector<Device> FindDevices (const Criterion& crit) {
        const ULONG searchScope = CM_GET_DEVICE_INTERFACE_LIST_PRESENT; // only currently 'live' device interfaces

        ULONG deviceInterfaceListLength = 0;
        CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&deviceInterfaceListLength, (GUID*)&GUID_DEVINTERFACE_HID, NULL, searchScope);
        assert(cr == CR_SUCCESS);

        // symbolic link name of interface instances
        std::wstring deviceInterfaceList(deviceInterfaceListLength, L'\0');
        cr = CM_Get_Device_Interface_ListW((GUID*)&GUID_DEVINTERFACE_HID, NULL, deviceInterfaceList.data(), deviceInterfaceListLength, searchScope);
        assert(cr == CR_SUCCESS);

        std::vector<Device> results;
        for (const wchar_t * currentInterface = deviceInterfaceList.c_str(); *currentInterface; currentInterface += wcslen(currentInterface) + 1) {
            auto result = CheckDevice(currentInterface, crit);
            if (result.IsValid())
                results.push_back(std::move(result));
        }

        return results;
    }

private:
    static Device CheckDevice(const wchar_t* deviceName, const Criterion& crit) {
        Device dev(deviceName);
        if (!dev.IsValid())
            return {};

        //wprintf(L"Device %ls (VendorID=%x, ProductID=%x, Usage=%x, UsagePage=%x)\n", deviceName, dev.attr.VendorID, dev.attr.ProductID, dev.caps.Usage, dev.caps.UsagePage);

        if (crit.VendorID && (crit.VendorID != dev.attr.VendorID))
            return Device();
        if (crit.ProductID && (crit.ProductID != dev.attr.ProductID))
            return Device();

        if (crit.Usage && (crit.Usage != dev.caps.Usage))
            return Device();
        if (crit.UsagePage && (crit.UsagePage != dev.caps.UsagePage))
            return Device();

        //wprintf(L"  Found matching device with VendorID=%x, ProductID=%x\n", dev.attr.VendorID, dev.attr.ProductID);
#if 0
        wprintf(L"  Manufacturer: %ws\n", dev.GetManufacturerString().c_str());
        wprintf(L"  Product: %ws\n", dev.GetProductString().c_str());
#endif
        return dev;
    }
};

} // namespace hid
