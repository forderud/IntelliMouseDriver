#pragma once

/** Size should by 73 bytes */
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

    NTSTATUS Clamp() {
        if ((Unknown1 != 0xB2) || (Unknown2 != 0x03)) {
            KdPrint(("FireFly: TailLightReport::Clamp: Unknown control Code 0x%x 0x%x\n", Unknown1, Unknown2));
            return STATUS_UNSUCCESSFUL;
        }

        // RGB check
        unsigned int color_sum = Red + Green + Blue;
        if (color_sum > 2 * 255) {
            KdPrint(("FireFly: Clamp_HIDMINI_CONTROL_INFO: Clamping color_sum 0x%x\n", color_sum));
            Red /= 2;
            Green /= 2;
            Blue /= 2;
        }

        return STATUS_SUCCESS;
    }


    bool IsValid() const {
        return ReportId == 36; // 0x24
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

NTSTATUS FireflySetFeature(
    IN  WDFDEVICE Device,
    IN  ULONG     Color
    );
