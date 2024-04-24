#include "HID.hpp"
#include "TailLight.hpp"
#include "Wbem.h"

#define SWITCH_BIST "/bist"
#define CB_SWITCH_BIST_ONLY (sizeof(SWITCH_BIST) - 1)

void PrintUsage() {
    wprintf(L"IntelliMouse tail-light shifter .\n");
    wprintf(L"Usage:\n\"HidUtil.exe [/bist] [<red> <green> <blue>\""
        " (example: \"HidUtil.exe 0 0 255\" or\n \"HidUtil.exe /bist\"]).\n");
}

int main(int argc, char* argv[]) {

    BYTE red     = 0;
    BYTE green   = 0;
    BYTE blue    = 0;

    if (argc == 2 && lstrlenA(argv[1]) == CB_SWITCH_BIST_ONLY) {
        if (memcmp(argv[1], SWITCH_BIST, CB_SWITCH_BIST_ONLY) == 0) {
            if (SUCCEEDED(ConnectToWbem())) {
                return 0;
            }
            else {
                return -4;
            }
        }
        else {
            PrintUsage();
            return -1;
        }
    }
    else if (argc == 4) {
        red = (BYTE)atoi(argv[1]);
        green = (BYTE)atoi(argv[2]);
        blue = (BYTE)atoi(argv[3]);
    }
    else {
        PrintUsage();
        return -1;
    }

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
