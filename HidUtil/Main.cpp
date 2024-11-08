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
    crit.VendorID = 0x045E;  // Microsoft
    crit.ProductID = 0x082A; // Pro IntelliMouse
    crit.Usage = 0x0212;     //
    crit.UsagePage = 0xFF07; //

    wprintf(L"Searching for matching HID devices...\n");
    std::vector<hid::Device> matches = hid::Query::FindDevices(crit);
    if (matches.empty()) {
        wprintf(L"No matching devices found.\n");
        return -3;
    }

    for (hid::Device& match : matches) {
#if 0
        COLORREF cur_color = 0;
        if (GetTailLight(match.dev.Get(), cur_color)) {
            wprintf(L"Current color: %u\n", cur_color);
        }
#endif

        wprintf(L"Updating %s\n", match.devName.c_str());
        bool ok = UpdateTailLight(match.dev.Get(), match.report, match.caps, RGB(red, green, blue));
        if (!ok)
            return -2;

        wprintf(L"SUCCESS: Tail-light color updated.\n");
    }

    return 0;
}
