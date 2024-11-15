#pragma once


bool SetTailLightRGB(hid::Device& dev, COLORREF color) {
    {
        // query current color (doesn't work)
        auto report = dev.GetReport<TailLightReport>(HidP_Feature);
        wprintf(L"Current color: %u\n", report.GetColor());
    }

    {
        // update color
        TailLightReport report;
        report.SetColor(color);
        bool ok = dev.SetReport(HidP_Feature, report);
        if (!ok) {
            wprintf(L"ERROR: Set TailLightReport failure.\n");
            assert(ok);
            return false;
        }
        wprintf(L"SUCCESS: Tail-light color updated.\n");
    }

    return true;
}

bool SetTailLightBool(hid::Device& dev, bool enable) {
    constexpr uint16_t TAILLIGHT_PAGE = 0xFF;
    constexpr uint16_t TAILLIGHT_FEATURE = 0x02;

    std::vector<CHAR> report(dev.caps.FeatureReportByteLength, (BYTE)0);

    USAGE usage = TAILLIGHT_FEATURE;
    ULONG usageLength = 1;

    if (enable) {
        NTSTATUS res = HidP_SetUsages(HidP_Feature,
            TAILLIGHT_PAGE,
            0,
            &usage, // pointer to the usage list
            &usageLength, // number of usages in the usage list
            dev.preparsed,
            report.data(),
            dev.caps.FeatureReportByteLength);
        assert(res == HIDP_STATUS_SUCCESS); res;
    }

    bool ok = dev.SetReport(HidP_Feature, report);
    assert(ok);
    return ok;
}
