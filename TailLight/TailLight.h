#pragma once

/** Tail-light feature report as observed in USBPcap/Wireshark. */
#pragma pack(push, 1) // no padding
struct TailLightReport {
    /** ReportID values taken from https://github.com/forderud/HidBattery/blob/master/src/HIDPowerDevice.h */
    enum ReportType : UCHAR {
        ManufactureDate = 0x09,
        Temperature = 0x0A,
        Voltage = 0x0B,
        RemainingCapacity = 0x0c,
        CycleCount = 0x14,
    };

    TailLightReport() = default;

    TailLightReport(ReportType type) {
        ReportId = type;
    }

#ifdef _KERNEL_MODE
    bool IsValid() const {
        if (ReportId != 0x0A) {
            DebugPrint(DPFLTR_ERROR_LEVEL, "TailLight: TailLightReport: Unsupported report id %d\n", ReportId);
            return false;
        }

        return true;
    }
#endif

#ifdef _KERNEL_MODE
    bool SafetyCheck() {
        return true;
    }
#endif

    //report ID of the collection to which the control request is sent
    UCHAR  ReportId = 0;
    USHORT Value = 0;
};
#pragma pack(pop) // restore default settings
static_assert(sizeof(TailLightReport) == 3);
