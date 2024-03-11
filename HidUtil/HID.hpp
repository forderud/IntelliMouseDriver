#pragma once
#include <windows.h>
#include <hidsdi.h>
#include <SetupAPI.h>
#include <wrl/wrappers/corewrappers.h>

#include <cassert>
#include <string>
#include <vector>

#pragma comment(lib, "hid.lib")
#pragma comment (lib, "Setupapi.lib")


/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PreparsedData {
public:
    PreparsedData() = default;

    PreparsedData(HANDLE hid_dev) {
        HidD_GetPreparsedData(hid_dev, &report);
    }
    ~PreparsedData() {
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

/** Human Interface Devices (HID) device search class. */
class HID {
public:
    // RAII wrapper of file HANDLE objects
    using FileHandle = Microsoft::WRL::Wrappers::FileHandle;

    struct Query {
        USHORT VendorID = 0;
        USHORT ProductID = 0;
        USHORT Usage = 0;
        USHORT UsagePage = 0;
    };

    struct Match {
        std::wstring name;
        FileHandle dev;
        PreparsedData report;
        HIDP_CAPS caps = {};
    };

    static std::vector<Match> FindDevices (const Query& query, bool verbose) {
        GUID hidguid = {};
        HidD_GetHidGuid(&hidguid);

        // Retrieve a list of all present USB devices with a device interface.
        HDEVINFO devInfoSet = SetupDiGetClassDevsW(&hidguid, NULL, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        assert(devInfoSet != INVALID_HANDLE_VALUE);

        std::vector<Match> results;
        for (DWORD devIdx = 0;; ++devIdx) {
            // get device information
            SP_DEVINFO_DATA devInfoData = {};
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            if (!SetupDiEnumDeviceInfo(devInfoSet, devIdx, &devInfoData))
                break;

            // list of hardware IDs 
            auto hw_id_list = GetHWIds(devInfoSet, &devInfoData);
            if (verbose) {
                for (const auto& hw_id :  hw_id_list)
                    printf("  HW ID: %ls\n", hw_id.c_str());
            }

            // get device interfaces
            SP_DEVICE_INTERFACE_DATA devIfData = {};
            devIfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
            BOOL ok = SetupDiEnumDeviceInterfaces(devInfoSet, NULL, &hidguid, devIdx, &devIfData);
            assert(ok);

            std::vector<BYTE> devIfcDetailDataBuf;
            SP_DEVICE_INTERFACE_DETAIL_DATA_W* devIfcDetailData = nullptr;
            {
                DWORD size = 0;
                SetupDiGetDeviceInterfaceDetailW(devInfoSet, &devIfData, NULL, 0, &size, NULL); // expected to fail

                devIfcDetailDataBuf.resize(size, (BYTE)0);
                devIfcDetailData = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)devIfcDetailDataBuf.data();
                devIfcDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

                // get details about a device interface
                ok = SetupDiGetDeviceInterfaceDetailW(devInfoSet, &devIfData, devIfcDetailData, size, &size, NULL);
                assert(ok);
            }

            auto result = CheckDevice(devIfcDetailData->DevicePath, query, verbose);
            if (!result.name.empty())
                results.push_back(std::move(result));
        }

        SetupDiDestroyDeviceInfoList(devInfoSet);

        return results;
    }

private:
    /** Get list of hardware IDs for a device. */
    static std::vector<std::wstring> GetHWIds(HDEVINFO devInfoSet, SP_DEVINFO_DATA* devInfoData) {
        // Get required size for device property
        DWORD type = 0;
        DWORD size = 0;
        SetupDiGetDeviceRegistryPropertyW(devInfoSet, devInfoData, SPDRP_HARDWAREID, &type, NULL, 0, &size); // expected to fail

        // Get SPDRP_HARDWAREID device property in REG_MULTI_SZ string format
        std::vector<wchar_t> hw_ids(size/sizeof(wchar_t), L'\0');
        BOOL ok = SetupDiGetDeviceRegistryPropertyW(devInfoSet, devInfoData, SPDRP_HARDWAREID, &type, (BYTE*)hw_ids.data(), size, NULL);
        assert(ok);

        return ParseMultiSz(hw_ids.data());
    }

    /** Parse REG_MULTI_SZ string into a list of strings. */
    static std::vector<std::wstring> ParseMultiSz(const wchar_t* ptr) {
        std::vector<std::wstring> result;
        // parse list of zero-terminated strings that are terminated by a null-pointer
        while (*ptr) {
            result.emplace_back(ptr);
            ptr += lstrlenW(ptr) + 1; // advance to next string
        }
        return result;
    }

    static Match CheckDevice(const wchar_t* deviceName, const Query& query, bool verbose) {
        FileHandle hid_dev(CreateFileW(deviceName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL));
        if (!hid_dev.IsValid()) {
            DWORD err = GetLastError();
            //assert(err != ERROR_ACCESS_DENIED); // (5) observed for already used devices
            //assert(err != ERROR_SHARING_VIOLATION); // (32)
            if (verbose)
                wprintf(L"WARNING: CreateFile failed: (err %d) for %ls\n", err, deviceName);

            return {};
        }

        HIDD_ATTRIBUTES attr = {};
        if (!HidD_GetAttributes(hid_dev.Get(), &attr))
            abort();

        PreparsedData reportDesc(hid_dev.Get());

        HIDP_CAPS caps = {};
        if (HidP_GetCaps(reportDesc, &caps) != HIDP_STATUS_SUCCESS)
            abort();

        if (verbose)
            wprintf(L"Device %ls (VendorID=%x, ProductID=%x, Usage=%x, UsagePage=%x)\n", deviceName, attr.VendorID, attr.ProductID, caps.Usage, caps.UsagePage);

        if (query.VendorID && (query.VendorID != attr.VendorID))
            return {};
        if (query.ProductID && (query.ProductID != attr.ProductID))
            return {};

        if (query.Usage && (query.Usage != caps.Usage))
            return {};
        if (query.UsagePage && (query.UsagePage != caps.UsagePage))
            return {};

        if (verbose)
            wprintf(L"  Found matching device with VendorID=%x, ProductID=%x\n", attr.VendorID, attr.ProductID);
#if 0
        wchar_t man_buffer[128] = L"<unknown>";
        HidD_GetManufacturerString(hid_dev.Get(), man_buffer, (ULONG)std::size(man_buffer)); // ignore errors
        wprintf(L"  Manufacturer: %ws\n", man_buffer);

        wchar_t prod_buffer[128] = L"<unknown>";
        HidD_GetProductString(hid_dev.Get(), prod_buffer, (ULONG)std::size(prod_buffer)); // ignore erorrs
        wprintf(L"  Product: %ws\n", prod_buffer);
#endif
        return {deviceName, std::move(hid_dev), std::move(reportDesc), caps};
    }
};
