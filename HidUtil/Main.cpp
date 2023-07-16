#include "HID.hpp"
#include "TailLight.hpp"


int main(int argc, char* argv[]) {
    if (argc < 4) {
        wprintf(L"IntelliMouse tail-light shifter.\n");
        wprintf(L"Usage: \"HidUtil.exe <red> <green> <blue>\" (example: \"HidUtil.exe 0 0 255\").\n");
        return -1;
    }

    BYTE red = atoi(argv[1]);
    BYTE green = atoi(argv[2]);
    BYTE blue = atoi(argv[3]);

    HID::Query query;
    query.VendorID = 0x045E;  // Microsoft
    query.ProductID = 0x082A; // Pro IntelliMouse
    query.Usage = 0x0212;     //
    query.UsagePage = 0xFF07; //

    wprintf(L"Searching for matching HID devices...\n");
    auto matches = HID::FindDevices(query, false);
    for (HID::Match& match : matches) {
        wprintf(L"Updating %s\n", match.name.c_str());
        bool ok = UpdateTailLight(match.dev.Get(), match.report, match.caps, RGB(red, green, blue));
        if (!ok)
            return -2;

        wprintf(L"SUCCESS: Tail-light color updated.\n");
    }

    return 0;
}
