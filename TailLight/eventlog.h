#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "messages.h" // driver-specific MessageId values
#include "inc\kmcodesegs.h"

/** Write to the Windows Event Viewer "System" log.
    InsertionStr1 is a null-terminated string. */
PAGED_CODE_SEG
void WriteToSystemLog(WDFDEVICE Device, NTSTATUS MessageId, WCHAR* InsertionStr1, WCHAR* InsertionStr2);
