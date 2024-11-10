#include "HID.hpp"
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

        {
            // query current color (doesn't work)
            auto report = dev.GetFeature<TailLightReport>();
            wprintf(L"Current color: %u\n", report.GetColor());
        }

        {
            // update color
            TailLightReport report;
            report.SetColor(RGB(red, green, blue));
            bool ok = dev.SetFeature(report);
            if (!ok) {
                printf("ERROR: Set TailLightReport failure.\n");
                assert(ok);
                return false;
            }
            wprintf(L"SUCCESS: Tail-light color updated.\n");
        }

        MiscTestTailLight(dev);
    }

    return 0;
}
