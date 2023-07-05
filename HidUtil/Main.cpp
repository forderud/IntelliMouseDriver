#include "HID.hpp"


bool UpdateTailColor(HANDLE hid_dev, PHIDP_PREPARSED_DATA reportDesc, HIDP_CAPS caps, COLORREF color) {
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

    // increase size by 1 for report ID header
    std::vector<BYTE> featureReport(caps.FeatureReportByteLength + 1, (BYTE)0);

    // Set feature report values (as observed in USBPcap/Wireshark)
    featureReport[0] = valueCaps.ReportID; // ReportID 0x24 (36)
    featureReport[1] = 0xB2; // magic value
    featureReport[2] = 0x03; // magic value
    // tail-light color
    featureReport[3] = (color     )  & 0xFF; // red
    featureReport[4] = (color >> 8)  & 0xFF; // green
    featureReport[5] = (color >> 16) & 0xFF; // blue

    BOOLEAN ok = HidD_SetFeature(hid_dev, featureReport.data(), (ULONG)featureReport.size());
    if (!ok) {
        DWORD err = GetLastError();
        printf("ERROR: HidD_SetFeature failure (err %d).\n", err);
        assert(ok);
        return false;
    }

    return true;
}

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
        bool ok = UpdateTailColor(match.dev.Get(), match.report, match.caps, RGB(red, green, blue));
        if (!ok)
            return -2;

        wprintf(L"SUCCESS: Tail-light color updated.\n");
    }

    return 0;
}
