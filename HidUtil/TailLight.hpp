#pragma once


bool SetTailLightRGB(hid::Device& dev, COLORREF color) {
    wprintf(L"Updating TailLight color:\n");
    {
        // query current color (doesn't work)
        auto report = dev.GetReport<TailLightReport>(HidP_Feature);
        report.Print(L"  Previous color (doesn't work):");
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
        wprintf(L"SUCCESS:\n");
        report.Print(L"  New color:");
    }

    return true;
}
