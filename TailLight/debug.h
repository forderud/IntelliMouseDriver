#pragma once

#pragma once

#define TRACE_FN_ENTRY KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "TailLight: Entry %s\n", __func__));
#define TRACE_FN_EXIT  KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "TailLight: Exit: %s\n", __func__));
#define TRACE_REQUEST_BOOL(ret)  KdPrint(("TailLight: WdfRequestSend returned bool: 0x%x\n", ret));
#define TRACE_REQUEST_FAILURE(status)  KdPrint(("TailLight: WdfRequestSend failed. Status=: 0x%x\n", ret));

NTSTATUS DumpTarget(WDFIOTARGET target, WDFMEMORY& memory);