#pragma once
#include <Windows.h>
#include <Hidsdi.h>

/** Tail-light feature report as observed in USBPcap/Wireshark. */
struct TailLightReport {
    TailLightReport(ULONG Color) {
        Red = (Color) & 0xFF; // red;
        Green = (Color >> 8) & 0xFF; // green
        Blue = (Color >> 16) & 0xFF; // blue
    }

    bool IsValid() const {
        if (ReportId != 36) {// 0x24
            return false;
        }

        if ((Unknown1 != 0xB2) || (Unknown2 != 0x03)) {
            return false;
        }

        return true;
    }

    //report ID of the collection to which the control request is sent
    UCHAR    ReportId = 36; // (0x24)

    // control codes (user-defined)
    UCHAR   Unknown1 = 0xB2; // magic value
    UCHAR   Unknown2 = 0x03; // magic value

    UCHAR   Red = 0;
    UCHAR   Green = 0;
    UCHAR   Blue = 0;

    UCHAR  padding[67] = {};
};
static_assert(sizeof(TailLightReport) == 73);


bool UpdateTailLight(HANDLE hid_dev, PHIDP_PREPARSED_DATA reportDesc, HIDP_CAPS caps, COLORREF color) {
#if 0
    printf("Device capabilities:\n");
    printf("  Usage=0x%04X, UsagePage=0x%04X\n", caps.Usage, caps.UsagePage);
    printf("  InputReportByteLength=%u, OutputReportByteLength=%u, FeatureReportByteLength=%u, NumberLinkCollectionNodes=%u\n", caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, caps.NumberLinkCollectionNodes);
    printf("  NumberInputButtonCaps=%u, NumberInputValueCaps=%u, NumberInputDataIndices=%u\n", caps.NumberInputButtonCaps, caps.NumberInputValueCaps, caps.NumberInputDataIndices);
    printf("  NumberOutputButtonCaps=%u, NumberOutputValueCaps=%u, NumberOutputDataIndices=%u\n", caps.NumberOutputButtonCaps, caps.NumberOutputValueCaps, caps.NumberOutputDataIndices);
    printf("  NumberFeatureButtonCaps=%u, NumberFeatureValueCaps=%u, NumberFeatureDataIndices=%u\n", caps.NumberFeatureButtonCaps, caps.NumberFeatureValueCaps, caps.NumberFeatureDataIndices);
#endif

    if (caps.FeatureReportByteLength != 73)
        return false; // length mismatch

    HIDP_VALUE_CAPS     valueCaps = {};
    USHORT              ValueCapsLength = caps.NumberFeatureValueCaps;
    NTSTATUS status = HidP_GetValueCaps(HidP_Feature, &valueCaps, &ValueCapsLength, reportDesc);
    assert(status == HIDP_STATUS_SUCCESS);

    TailLightReport featureReport(color);

    BOOLEAN ok = HidD_SetFeature(hid_dev, &featureReport, (ULONG)sizeof(featureReport));
    if (!ok) {
        DWORD err = GetLastError();
        printf("ERROR: HidD_SetFeature failure (err %d).\n", err);
        assert(ok);
        return false;
    }

    return true;
}
