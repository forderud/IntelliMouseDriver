#ifdef _KERNEL_MODE
    #include <ntddk.h>
    #include <wdf.h>
    #define NTSTRSAFE_LIB
    //#include <ntstrsafe.h>
    #include <initguid.h>
#else
    #include <windows.h>
#endif

#include "inc\TailLight.h"

PAGED_CODE_SEG
void Init(TailLightReport* pReport) {
    pReport->ReportId = TAILLIGHT_SET_COLOR_REPORT_ID;
    pReport->Unknown1 = TAILLIGHT_UNKNOWN_1;
    pReport->Unknown2 = TAILLIGHT_UNKNOWN_2;
}

PAGED_CODE_SEG
void SetColor(TailLightReport* pReport, ULONG Color) {
    pReport->Red = (Color) & 0xFF; // red;
    pReport->Green = (Color >> 8) & 0xFF; // green
    pReport->Blue = (Color >> 16) & 0xFF; // blue
}

PAGED_CODE_SEG
ULONG GetColor(TailLightReport* pReport) {
    return (pReport->Blue << 16) | (pReport->Green << 8) | pReport->Red;
}

#ifdef _KERNEL_MODE
    PAGED_CODE_SEG
    bool IsValid(TailLightReport* pReport)  {
        if (pReport->ReportId != 36) {// 0x24
            KdPrint(("TailLight: TailLightReport: Unsupported report id %d\n", pReport->ReportId));
            return false;
        }

        if ((pReport->Unknown1 != 0xB2) || (pReport->Unknown2 != 0x03)) {
            KdPrint(("TailLight: TailLightReport: Unknown control Code 0x%x 0x%x\n", pReport->Unknown1, pReport->Unknown2));
            return false;
        }

        return true;
    }
#endif

#ifdef _KERNEL_MODE
    PAGED_CODE_SEG
    bool SafetyCheck(TailLightReport* pReport) {
        // RGB check
        unsigned int color_sum = pReport->Red + pReport->Green + pReport->Blue;
        if (color_sum > 2 * 256) {
            KdPrint(("TailLight: Color saturation %u exceeded 512 threshold. Reseting color to RED to signal error\n", color_sum));
            pReport->Red = 255;
            pReport->Green = 0;
            pReport->Blue = 0;
            return false;
        }

        return true;
    }
#endif