#include "Misc.h"
#include "Driver.h"
#include "Device.h"
#include "usbdevice.h"

#include "Misc.tmh"


static VOID _WQQCancelRequest(IN WDFQUEUE Queue, IN WDFREQUEST  Request)
{
    UNREFERENCED_PARAMETER(Queue);
    WdfRequestComplete(Request, STATUS_CANCELLED);
}


static VOID _WQQCancelUSBRequest(IN WDFQUEUE Queue, IN WDFREQUEST  Request)
{
    UNREFERENCED_PARAMETER(Queue);
    LogInfo(TRACE_DEVICE, "Canceling request %p", Request);
    UdecxUrbCompleteWithNtStatus(Request, STATUS_CANCELLED);
}


NTSTATUS WRQueueInit(_In_ WDFDEVICE parent, _Inout_ WRITE_BUFFER_TO_READ_REQUEST_QUEUE* pQ, _In_ BOOLEAN bUSBReqQueue)
{
    memset(pQ, 0, sizeof(*pQ));

    WDF_IO_QUEUE_CONFIG queueConfig = {};
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
    // when a request gets canceled, this is where we want to do the completion
    queueConfig.EvtIoCanceledOnQueue = (bUSBReqQueue ? _WQQCancelUSBRequest : _WQQCancelRequest );

    NTSTATUS status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &(pQ->qsync));
    if (!NT_SUCCESS(status) )  {
        pQ->qsync = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "Unable to create spinlock, err= %!STATUS!", status);
        goto Error;
    }

    InitializeListHead( &(pQ->WriteBufferQueue) );

    status = WdfIoQueueCreate(parent, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &(pQ->ReadBufferQueue));

    if (!NT_SUCCESS(status))  {
        pQ->ReadBufferQueue = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "Unable to create rd queue, err= %!STATUS!", status );
        goto Error;
    }

    return status;

Error: // free anything done half-way
    WRQueueDestroy(pQ);
    return status;
}

VOID WRQueueDestroy(_Inout_ WRITE_BUFFER_TO_READ_REQUEST_QUEUE* pQ)
{
    if (pQ->qsync == NULL)  {
        return; // init has not even started
    }

    {
        // clean up the entire list
        SpinLock lock(pQ->qsync);

        LIST_ENTRY* e = nullptr;
        while ((e = RemoveHeadList(&(pQ->WriteBufferQueue))) != (&(pQ->WriteBufferQueue))) {
            BUFFER_CONTENT* pWriteEntry = CONTAINING_RECORD(e, BUFFER_CONTENT, BufferLink);
            ExFreePool(pWriteEntry);
        }
    }

    WdfObjectDelete(pQ->ReadBufferQueue);
    pQ->ReadBufferQueue = NULL;
 
    WdfObjectDelete(pQ->qsync);
    pQ->qsync = NULL;
}
