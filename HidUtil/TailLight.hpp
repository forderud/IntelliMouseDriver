#pragma once
#include <Windows.h>
#include <Hidsdi.h>
#include "../TailLight/TailLight.h"


bool MiscTestTailLight(hid::Device& dev) {
    {
        // TEST: Read input report
        std::vector<BYTE> report = dev.GetInput(0x27); // ReportID 39
        assert(report[0] == 0xB2);
        // the rest of inputBuf is still empty
    }

    return true;
}
