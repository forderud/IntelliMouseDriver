#pragma once

/** Tail-light set feature report values as observed in USBPcap/Wireshark. */
struct TailLightReport {
    TailLightReport(IN UCHAR _ReportID, IN  ULONG Color) {
        ReportId = _ReportID; // Report ID 0x24 (36)
        Unknown1 = 0xB2; // magic value
        Unknown2 = 0x03; // magic value
        // tail-light color
        Red = (Color) & 0xFF; // red;
        Green = (Color >> 8) & 0xFF; // green
        Blue = (Color >> 16) & 0xFF; // blue
    }

    bool IsValid() const {
        if (ReportId != 36) {// 0x24
            KdPrint(("FireFly: TailLightReport: Unsupported report id %d\n", ReportId));
            return false;
        }

        if ((Unknown1 != 0xB2) || (Unknown2 != 0x03)) {
            KdPrint(("FireFly: TailLightReport: Unknown control Code 0x%x 0x%x\n", Unknown1, Unknown2));
            return false;
        }

        return true;
    }

    void SafetyCheck () {
        // RGB check
        unsigned int color_sum = Red + Green + Blue;
        if (color_sum > 2*256) {
            KdPrint(("FireFly: Color saturation %u exceeded 512 threshold. Reseting color to RED to signal error\n", color_sum));
            Red = 255;
            Green = 0;
            Blue = 0;
        }
    }


    //report ID of the collection to which the control request is sent
    UCHAR    ReportId; // 36 (0x24)

    // control codes (user-defined)
    UCHAR   Unknown1; // 0xB2
    UCHAR   Unknown2; // 0x03

    UCHAR   Red;
    UCHAR   Green;
    UCHAR   Blue;

    UCHAR  padding[67];
};
static_assert(sizeof(TailLightReport) == 73);


NTSTATUS FireflySetFeature(
    IN  WDFDEVICE Device,
    IN  ULONG     Color
    );
