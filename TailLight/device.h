#pragma once

#include "debug.h"

#define MSINTELLIMOUSE_USBINTERFACE5_PREFIX L"\\??\\HID#VID_045E&PID_082A&MI_01&Col05"

#define NANOS_OF_100_TO_WAIT -1
#define MAX_SET_BLACK_TIMER_TICKS 99

#define BEGIN_WITH(x) { \
        auto &_ = x;
#define END_WITH() }

typedef struct _SET_BLACK_WORK_ITEM_CONTEXT {
    LONG        setBlackTimerTicksLeft = MAX_SET_BLACK_TIMER_TICKS;
    KEVENT      delayEvent;
    LARGE_INTEGER waitTime100Nanos;

    void Init() {
        TRACE_FN_ENTRY
        KeInitializeEvent(&delayEvent, NotificationEvent, FALSE);
        waitTime100Nanos.QuadPart = NANOS_OF_100_TO_WAIT;
        setBlackTimerTicksLeft = MAX_SET_BLACK_TIMER_TICKS;
    }

    NTSTATUS Wait() {
        TRACE_FN_ENTRY
        return KeWaitForSingleObject(&delayEvent,
            Executive,
            KernelMode,
            TRUE,
            &waitTime100Nanos);
    }

    void Cancel()
        /*++

     Routine Description:

        Ensures that the SetBlack loop is run down. If we're waiting then
        we'll exit. If we're not waiting, we may wait a little then exit.
        Less of the loop is executed this way.
     --*/
    {
        TRACE_FN_ENTRY
        InterlockedExchange(static_cast<CONST PLONG>(
            &setBlackTimerTicksLeft), 0);
        KeSetEvent(&delayEvent, IO_MOUSE_INCREMENT, FALSE);
    }
} SET_BLACK_WORK_ITEM_CONTEXT, *PSET_BLACK_WORK_ITEM_CONTEXT;

/** Driver-specific struct for storing instance-specific data. */
typedef struct _DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    ULONG          TailLight; ///< last written color

    // Useful for debugging. This way less need to hunt for the stack that
    // called IoCallDriver.
    PKTHREAD    previousThread;

    BOOLEAN fSetBlackSuccess;
    PSET_BLACK_WORK_ITEM_CONTEXT pSetBlackWorkItemContext;
    WDFWMIINSTANCE WmiInstance;
} DEVICE_CONTEXT;

template<typename T> inline void NukeWdfHandle(T& handle) {
    if (handle) {
        WdfObjectDelete(handle);
        handle = 0;
    }
}

#define RETURN_RESULT_IF_SET_OPERATION_NULL( var, op, result ) { \
    var = op; \
    if (var == NULL) return status; }

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

NTSTATUS PnpNotifyDeviceInterfaceChange(
    _In_ PVOID NotificationStructure,
    _Inout_opt_ PVOID Context);
