#pragma once

static UCHAR TailLight_ReportID = 36; // 0x24

/** Size should by 73 bytes */
typedef struct _HIDMINI_CONTROL_INFO {
    //report ID of the collection to which the control request is sent
    UCHAR    ReportId; // 36 (0x24)

    // control codes (user-defined)
    UCHAR   Unknown1; // 0xB2
    UCHAR   Unknown2; // 0x03

    UCHAR   Red;
    UCHAR   Green;
    UCHAR   Blue;

    UCHAR  padding[67];
} HIDMINI_CONTROL_INFO;

void Init_HIDMINI_CONTROL_INFO(OUT HIDMINI_CONTROL_INFO* report, IN UCHAR ReportID, IN  ULONG Color);

#ifdef __cplusplus
extern "C"
#endif
NTSTATUS Clamp_HIDMINI_CONTROL_INFO(HIDMINI_CONTROL_INFO* report);

#ifdef __cplusplus
extern "C"
#endif
NTSTATUS FireflySetFeature(
    IN  WDFDEVICE Device,
    IN  ULONG     Color
    );
