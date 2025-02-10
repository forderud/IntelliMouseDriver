#pragma once


bool SetTailLightRGB(hid::Device& dev) {
    {
        // query current color (doesn't work)
        auto report = dev.GetReport<TailLightReport>(HidP_Feature, TailLightReport::Temperature);
        report.Print("Current");
    }

    return true;
}
