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
