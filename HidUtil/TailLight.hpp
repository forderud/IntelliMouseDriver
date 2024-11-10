#pragma once
#include <Windows.h>
#include <Hidsdi.h>
#include "../TailLight/TailLight.h"


bool MiscTestTailLight(hid::Device& dev) {
#if 0
    {
        auto& caps = dev.caps;
        printf("Device capabilities:\n");
        printf("  Usage=0x%04X, UsagePage=0x%04X\n", caps.Usage, caps.UsagePage);
        printf("  InputReportByteLength=%u, OutputReportByteLength=%u, FeatureReportByteLength=%u, NumberLinkCollectionNodes=%u\n", caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, caps.NumberLinkCollectionNodes);
        printf("  NumberInputButtonCaps=%u, NumberInputValueCaps=%u, NumberInputDataIndices=%u\n", caps.NumberInputButtonCaps, caps.NumberInputValueCaps, caps.NumberInputDataIndices);
        printf("  NumberOutputButtonCaps=%u, NumberOutputValueCaps=%u, NumberOutputDataIndices=%u\n", caps.NumberOutputButtonCaps, caps.NumberOutputValueCaps, caps.NumberOutputDataIndices);
        printf("  NumberFeatureButtonCaps=%u, NumberFeatureValueCaps=%u, NumberFeatureDataIndices=%u\n", caps.NumberFeatureButtonCaps, caps.NumberFeatureValueCaps, caps.NumberFeatureDataIndices);
    }
#endif

    {
        USHORT valueCapsLen = dev.caps.NumberFeatureValueCaps;
        std::vector<HIDP_VALUE_CAPS> valueCaps(valueCapsLen, {});
        NTSTATUS status = HidP_GetValueCaps(HidP_Feature, valueCaps.data(), &valueCapsLen, dev.preparsed);
        assert(status == HIDP_STATUS_SUCCESS);
        for (auto& elm : valueCaps)
            printf("ReportID: 0x%X\n", elm.ReportID);
    }

    {
        // TEST: Read input report
        std::vector<BYTE> report = dev.GetInput(0x27); // ReportID 39
        assert(report[0] == 0xB2);
        // the rest of inputBuf is still empty
    }

    return true;
}
