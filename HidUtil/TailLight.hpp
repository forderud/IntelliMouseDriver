#pragma once


bool TestTailLight(hid::Device& dev, COLORREF color) {
    {
        // query current color (doesn't work)
        auto report = dev.GetReport<TailLightReport>(HidP_Feature);
        wprintf(L"Current color: %u\n", report.GetColor());
    }

    {
        // update color
        TailLightReport report;
        report.SetColor(color);
        bool ok = dev.SetFeature(report);
        if (!ok) {
            printf("ERROR: Set TailLightReport failure.\n");
            assert(ok);
            return false;
        }
        wprintf(L"SUCCESS: Tail-light color updated.\n");
    }

    return true;
}
