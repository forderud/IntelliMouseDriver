#include "luminous.hpp"
#include <dontuse.h>
#include <Shlobj.h>
#include <memory>


COLORREF ToColor(bool val) {
    if (val)
        return RGB(0, 255, 0); // green
    else
        return RGB(255, 0, 0); // red
}

int main(_In_ ULONG argc, _In_reads_(argc) PCHAR argv[]) {
    bool  bAdjustLight = false;
    ULONG lightSetting = 0;

    if (argc == 2) {
        if (argv[1][0] == '-') {
            if ((argv[1][1] >= '0') && (argv[1][1] <= '2')) {
                bAdjustLight = true;
                lightSetting = (argv[1][1] - '0');
            }
        }
     }

    if  (!bAdjustLight) {
        wprintf(L"Usage: Flicker <-0 | -1 | -2>\n"
                L"     - 0 turns off light\n"
                L"     - 1 turns on light\n"
                L"     - 2 flashes light");
        exit(0);
    }

    auto luminous = std::make_unique<Luminous>();

    if (luminous == NULL) {
        wprintf(L"Problem creating Luminous\n");
        return 0;
    }

    if (bAdjustLight) {
        if (lightSetting < 2) {
            bool ok = luminous->Set(ToColor(lightSetting));
            if (ok)
                wprintf(L"Adjusted light to %x\n", lightSetting);
            else
                wprintf(L"Problem occured while adjusting light: %x\n", GetLastError());
            Sleep(1000); // 1 sec
        } else {
            int k=0;
            int j=1;
            for(int i = 500; (i>0)&&(j>0); i-=j) {
                j = (i*9/100);
                Sleep(i);
                if(!luminous->Set(ToColor(k))) {
                    wprintf(L"Set operation on Luminous failed.\n");
                    return 0;
                }
                k=1-k;
            }
            for(int i = 12; i<500; i+=j) {
                j = (i*9/100);
                Sleep(i);
                if(!luminous->Set(ToColor(k))) {
                    wprintf(L"Set operation on Luminous failed.\n");
                    return 0;
                }
                k=1-k;
            }
            if (k) {
                if(!luminous->Set(ToColor(k)))
                    wprintf(L"Set operation on Luminous failed.\n");
            }
        }
    }

    // set color back to black
    luminous->Set(RGB(0, 0, 0));

    return 0;
}
