#pragma once

#include <ntddk.h>
#include <wdf.h>
#include "trace.h"


#define MINLEN(__a, __b)  ( ((__a) < (__b)) ? (__a) : (__b) )


struct BUFFER_CONTENT
{
    LIST_ENTRY  BufferLink;
    SIZE_T      BufferLength;
    UCHAR       BufferStart; // variable-size structure, first byte of last field
};


struct WRITE_BUFFER_TO_READ_REQUEST_QUEUE
{
    LIST_ENTRY WriteBufferQueue; // write data comes in, keep it here to complete a read request
    WDFQUEUE   ReadBufferQueue; // read request comes in, stays here til a matching write buffer arrives
    WDFSPINLOCK qsync;
};

NTSTATUS
WRQueueInit(
    _In_    WDFDEVICE parent,
    _Inout_ WRITE_BUFFER_TO_READ_REQUEST_QUEUE* pQ,
    _In_    BOOLEAN bUSBReqQueue
);

VOID
WRQueueDestroy(
    _Inout_ WRITE_BUFFER_TO_READ_REQUEST_QUEUE* pQ
);



NTSTATUS
WRQueuePushWrite(
    _In_ WRITE_BUFFER_TO_READ_REQUEST_QUEUE* pQ,
    _In_ PVOID wbuffer,
    _In_ SIZE_T wlen,
    _Out_ WDFREQUEST *rqReadToComplete
);

NTSTATUS
WRQueuePullRead(
    _In_  WRITE_BUFFER_TO_READ_REQUEST_QUEUE* pQ,
    _In_  WDFREQUEST rqRead,
    _Out_ PVOID rbuffer,
    _In_  SIZE_T rlen,
    _Out_ PBOOLEAN pbReadyToComplete,
    _Out_ PSIZE_T completedBytes
);
