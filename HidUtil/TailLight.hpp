#pragma once
#include <Windows.h>
#include <Hidsdi.h>
#include "../TailLight/TailLight.h"


bool GetTailLight(HANDLE hid_dev, COLORREF & color) {
    std::vector<BYTE> featureReport(41, 0); // 41 observed in WireShark
    featureReport[0] = 0x27; // 39 observed in WireShark (with type 1=Input report)
    featureReport[1] = 1; // observed in WireShark
    featureReport[2] = 1; // observed in WireShark
    featureReport[3] = 0; // observed in WireShark
    featureReport[4] = 0x29; // observed in WireShark
    featureReport[5] = 0; // observed in WireShark
    BOOLEAN ok = HidD_GetFeature(hid_dev, featureReport.data(), (ULONG)featureReport.size());
    if (!ok) {
        DWORD err = GetLastError();
        printf("ERROR: HidD_GetFeature failure (err %d).\n", err);
        assert(ok);
        return false;
    }

    color = 0;
    return true;

}


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

    TailLightReport featureReport;
    featureReport.SetColor(color);

    BOOLEAN ok = HidD_SetFeature(hid_dev, &featureReport, (ULONG)sizeof(featureReport));
    if (!ok) {
        DWORD err = GetLastError();
        printf("ERROR: HidD_SetFeature failure (err %d).\n", err);
        assert(ok);
        return false;
    }

    {
        // TEST: Read input report
        std::vector<BYTE> inputBuf(caps.InputReportByteLength, (BYTE)0);
        inputBuf[0] = 0x27; // ReportID 39
        ok = HidD_GetInputReport(hid_dev, inputBuf.data(), (ULONG)inputBuf.size());
        assert(ok);
        assert(inputBuf[1] == 0xB2);
        // the rest of inputBuf is still empty
    }

    return true;
}
