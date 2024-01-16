#pragma once
/** Tail-light feature report as observed in USBPcap/Wireshark. */

#include "kmcodesegs.h"

#define TAILLIGHT_SET_COLOR_REPORT_ID 36
#define TAILLIGHT_UNKNOWN_1 0xB2
#define TAILLIGHT_UNKNOWN_2 0X03

// Seems compiler is producing things correctly without changing the packing
struct TailLightReport {
    //report ID of the collection to which the control request is sent
    UCHAR    ReportId = 36; // (0x24)

    // control codes (user-defined)
    UCHAR   Unknown1 = 0xB2; // magic value
    UCHAR   Unknown2 = 0x03; // magic value

    UCHAR   Red = 0;
    UCHAR   Green = 0;
    UCHAR   Blue = 0;

    UCHAR  padding[67] = {};
};

static_assert(sizeof(TailLightReport) == 73, "TailLightReport is not of the needed size.");

PAGED_CODE_SEG
void Init(TailLightReport* pReport);

PAGED_CODE_SEG
ULONG GetColor(TailLightReport* pReport);

PAGED_CODE_SEG
void SetColor(TailLightReport* pReport, ULONG Color);

#ifdef _KERNEL_MODE
    PAGED_CODE_SEG
    bool IsValid(TailLightReport* pReport);

    PAGED_CODE_SEG
    bool SafetyCheck(TailLightReport* pReport);
#endif