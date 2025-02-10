#include "HID.hpp"
#include "../TailLight/TailLight.h"
#include "TailLight.hpp"


void PrintReport(const std::vector<BYTE>& report) {
    wprintf(L"  Data: {");
    for (BYTE elm : report)
        wprintf(L" %02x,", elm);
    wprintf(L"}\n");
}

int main(int argc, char* argv[]) {
    hid::Scan::Criterion crit;
    //crit.VendorID = 0x045E;  // Microsoft
    //crit.ProductID = 0x082A; // Pro IntelliMouse
    //crit.ProductID = 0x0047; // IntelliMouse Explorer 3.0 (access denied)
    crit.Usage = 0x0004;     //
    crit.UsagePage = 0x0084;

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

#if 0
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
#endif
        wprintf(L"Available FEATURE reports:\n");
        std::vector<HIDP_VALUE_CAPS> valueCaps = dev.GetValueCaps(HidP_Feature);
        for (auto& elm : valueCaps) {
            wprintf(L"  ReportID: %#04x\n", elm.ReportID);
            PrintReport(dev.GetReport(HidP_Feature, elm.ReportID));
        }
        wprintf(L"\n");

        SetTailLightRGB(dev);
    }

    return 0;
}
