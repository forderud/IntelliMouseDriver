#pragma once

/** Size should by 73 bytes */
struct HIDMINI_CONTROL_INFO {
    HIDMINI_CONTROL_INFO(IN UCHAR _ReportID, IN  ULONG Color) {
        ReportId = _ReportID; // Report ID 0x24 (36)

        Unknown1 = 0xB2; // magic value
        Unknown2 = 0x03; // magic value
        // tail-light color
        Red = (Color) & 0xFF; // red;
        Green = (Color >> 8) & 0xFF; // green
        Blue = (Color >> 16) & 0xFF; // blue
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

NTSTATUS Clamp_HIDMINI_CONTROL_INFO(HIDMINI_CONTROL_INFO* report);

NTSTATUS FireflySetFeature(
    IN  WDFDEVICE Device,
    IN  ULONG     Color
    );
