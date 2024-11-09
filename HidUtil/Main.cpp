#include "HID.hpp"
#include "../TailLight/TailLight.h"
#include "TailLight.hpp"


void PrintReport(const std::vector<BYTE>& report) {
    wprintf(L"  Data: {");
    for (BYTE elm : report)
        wprintf(L" %02x,", elm);
    wprintf(L"}\n");
}

struct StatusReport {
    BYTE reportId = 0x07; // reportId prefix

    uint8_t Charging : 1;                   // bit 0x00
    uint8_t Discharging : 1;                // bit 0x01
    uint8_t ACPresent : 1;                  // bit 0x02

    BYTE padding[2];
};

struct RemainingCapReport {
    BYTE reportId = 0x0C; // reportId prefix
    BYTE data[3] = {}; // 24 bit integer

    uint32_t Value() const {
        return (data[2] << 16) | (data[1] << 8) | data[0];
    }
};

struct ChemistryReport {
    BYTE reportId = 0x1F; // reportId prefix
    BYTE idx = 0; // string index
    BYTE padding[2] = {};
};


int main(int argc, char* argv[]) {
    if (argc < 4) {
        wprintf(L"IntelliMouse tail-light shifter.\n");
        wprintf(L"Usage: \"HidUtil.exe <red> <green> <blue>\" (example: \"HidUtil.exe 0 0 255\").\n");
        return -1;
    }

    BYTE red = (BYTE)atoi(argv[1]);
    BYTE green = (BYTE)atoi(argv[2]);
    BYTE blue = (BYTE)atoi(argv[3]);

    hid::Scan::Criterion crit;
    crit.Usage = 0x0004;     // UPS
    crit.UsagePage = 0x0084; // Power Device

    wprintf(L"Searching for matching HID devices...\n");
    std::vector<hid::Device> matches = hid::Scan::FindDevices(crit, false);
    if (matches.empty()) {
        wprintf(L"No matching devices found.\n");
        return -3;
    }

    for (hid::Device& dev : matches) {
        wprintf(L"Accessing %s:\n", dev.Name().c_str());
        //dev.PrintInfo();
        wprintf(L"\n");

        wprintf(L"Available INPUT reports:\n");
        std::vector<HIDP_VALUE_CAPS> valueCaps = dev.GetValueCaps(HidP_Input);
        for (auto& elm : valueCaps) {
            wprintf(L"  ReportID: %#04x\n", elm.ReportID);
            PrintReport(dev.GetReport(HidP_Input, elm.ReportID));
        }
        wprintf(L"Available OUTPUT reports:\n");
        valueCaps = dev.GetValueCaps(HidP_Output);
        for (auto& elm : valueCaps) {
            wprintf(L"  ReportID: %#04x\n", elm.ReportID);
            // cannot print output reports, since they're sent to the device
        }
        wprintf(L"Available FEATURE reports:\n");
        valueCaps = dev.GetValueCaps(HidP_Feature);
        for (auto& elm : valueCaps) {
            wprintf(L"  ReportID: %#04x\n", elm.ReportID);
            PrintReport(dev.GetReport(HidP_Feature, elm.ReportID));
        }
        wprintf(L"\n");

        if ((dev.caps.Usage == 0x0212) && (dev.caps.UsagePage == 0xFF07)) {
            // Pro IntelliMouse detected
            SetTailLightRGB(dev, RGB(red, green, blue));
        } else {
            auto status = dev.GetReport<StatusReport>(HidP_Feature);
            wprintf(L"Charging: %s\n", status.Charging ? L"yes" : L"no");

            auto fullCap = dev.GetReport<RemainingCapReport>(HidP_Feature);
            wprintf(L"Remaining capacity: %u AmpSec\n", fullCap.Value());

            auto chemIdx = dev.GetReport< ChemistryReport>(HidP_Feature);
            std::wstring chemStr = dev.GetString(chemIdx.idx);
            wprintf(L"Chemistry: %s\n", chemStr.c_str());
        }
    }

    return 0;
}
