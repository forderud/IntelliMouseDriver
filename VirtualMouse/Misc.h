#pragma once

#include <ntddk.h>
#include <wdf.h>
#include "trace.h"


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
