#pragma once

#include <ntddk.h>
#include <wdf.h>
#include "trace.h"


struct BUFFER_CONTENT {
    LIST_ENTRY  BufferLink;
    SIZE_T      BufferLength;
    UCHAR       BufferStart; // variable-size structure, first byte of last field
};


struct WRITE_BUFFER_TO_READ_REQUEST_QUEUE {
    LIST_ENTRY WriteBufferQueue; // write data comes in, keep it here to complete a read request
    WDFQUEUE   ReadBufferQueue; // read request comes in, stays here til a matching write buffer arrives
    WDFSPINLOCK qsync;
};

/** C++ spin lock RAII wrapper. */
class SpinLock {
public:
    SpinLock(WDFSPINLOCK lock) : m_lock(lock) {
        WdfSpinLockAcquire(m_lock);
    }
    ~SpinLock() {
        WdfSpinLockRelease(m_lock);
    }

private:
    WDFSPINLOCK m_lock;
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
