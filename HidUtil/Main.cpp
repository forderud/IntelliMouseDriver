#include "HID.hpp"
#include "../TailLight/TailLight.h"
#include "TailLight.hpp"


int main(int argc, char* argv[]) {
    if (argc < 4) {
        wprintf(L"IntelliMouse tail-light shifter.\n");
        wprintf(L"Usage: \"HidUtil.exe <red> <green> <blue>\" (example: \"HidUtil.exe 0 0 255\").\n");
        return -1;
    }

    BYTE red = (BYTE)atoi(argv[1]);
    BYTE green = (BYTE)atoi(argv[2]);
    BYTE blue = (BYTE)atoi(argv[3]);

    hid::Query::Criterion crit;
    //crit.VendorID = 0x045E;  // Microsoft
    //crit.ProductID = 0x082A; // Pro IntelliMouse
    crit.Usage = 0x0212;     //
    crit.UsagePage = 0xFF07; // Vendor-defined range (FF00-FFFF)

    wprintf(L"Searching for matching HID devices...\n");
    std::vector<hid::Device> matches = hid::Query::FindDevices(crit);
    if (matches.empty()) {
        wprintf(L"No matching devices found.\n");
        return -3;
    }

    for (hid::Device& dev : matches) {
        wprintf(L"Accessing %s\n", dev.devName.c_str());
        // dev.PrintCaps();

        wprintf(L"Available input reports:\n");
        std::vector<HIDP_VALUE_CAPS> valueCaps = dev.GetValueCaps(HidP_Input);
        for (auto& elm : valueCaps)
            wprintf(L"  ReportID: %#04x\n", elm.ReportID);

        wprintf(L"Available output reports:\n");
        valueCaps = dev.GetValueCaps(HidP_Output);
        for (auto& elm : valueCaps)
            wprintf(L"  ReportID: %#04x\n", elm.ReportID);

        wprintf(L"Available feature reports:\n");
        valueCaps = dev.GetValueCaps(HidP_Feature);
        for (auto& elm : valueCaps)
            wprintf(L"  ReportID: %#04x\n", elm.ReportID);

#if 0
        for (auto& elm : valueCaps) {
            // TEST: Input report
            std::vector<BYTE> input = dev.GetReport(HidP_Feature, elm.ReportID);
            input;
        }
#endif

        TestTailLight(dev, RGB(red, green, blue));
    }

    return 0;
}
