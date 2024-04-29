#include "HID.hpp"
#include "TailLight.hpp"


int main(int argc, char* argv[]) {
    if (argc < 4) {
        wprintf(L"IntelliMouse tail-light shifter.\n");
        wprintf(L"Usage: \"HidUtil.exe <red> <green> <blue>\" (example: \"HidUtil.exe 0 0 255\").\n");
        return -1;
    }

    auto red = (BYTE)atoi(argv[1]);
    auto green = (BYTE)atoi(argv[2]);
    auto blue = (BYTE)atoi(argv[3]);

    HID::Query query;
    query.VendorID = 0x045E;  // Microsoft
    query.ProductID = 0x082A; // Pro IntelliMouse
    query.Usage = 0x0212;     //
    query.UsagePage = 0xFF07; //

    wprintf(L"Searching for matching HID devices...\n");
    auto matches = HID::FindDevices(query);
    if (matches.empty()) {
        wprintf(L"No matching devices found.\n");
        return -3;
    }

    for (HID::Match& match : matches) {
#if 0
        COLORREF cur_color = 0;
        if (GetTailLight(match.dev.Get(), cur_color)) {
            wprintf(L"Current color: %u\n", cur_color);
        }
#endif

        wprintf(L"Updating %s\n", match.name.c_str());
        bool ok = UpdateTailLight(match.dev.Get(), match.report, match.caps, RGB(red, green, blue));
        if (!ok)
            return -2;

        wprintf(L"SUCCESS: Tail-light color updated.\n");
    }

    return 0;
}
