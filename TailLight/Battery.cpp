#include "Battery.hpp"
#include "driver.h"


static void UpdateBatteryInformation(BATTERY_INFORMATION& bi, SharedState& state) {
    auto CycleCountBefore = bi.CycleCount;

    auto lock = state.Lock();
    bi.CycleCount = state.CycleCount;
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: UpdateBatteryInformation CycleCount before=%u, after=%u\n", CycleCountBefore, bi.CycleCount);
}

static void UpdateBatteryTemperature(ULONG& temp, SharedState& state) {
    auto TempBefore = temp;

    auto lock = state.Lock();
    temp = state.Temperature;

    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: UpdateBatteryTemperature before=%u, after=%u\n", TempBefore, temp);
}


void EvtIoDeviceControlBattFilterCompletion (_In_  WDFREQUEST Request, _In_  WDFIOTARGET Target, _In_  PWDF_REQUEST_COMPLETION_PARAMS Params, _In_  WDFCONTEXT Context) {
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    NTSTATUS status = Params->IoStatus.Status;
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("ERROR: TailLight: IoctlrequestCompletion status=0x%x"), status);
        WdfRequestComplete(Request, status);
        return;
    }

    const ULONG IoControlCode = Params->Parameters.Ioctl.IoControlCode;
    if (IoControlCode != IOCTL_BATTERY_QUERY_INFORMATION) {
        WdfRequestComplete(Request, status);
        return;
    }

    WDFDEVICE Device = WdfIoTargetGetDevice(Target);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    WDFMEMORY OutputMem = Params->Parameters.Ioctl.Output.Buffer;
    size_t OutputLength = Params->Parameters.Ioctl.Output.Length;

    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: IOCTL_BATTERY_QUERY_INFORMATION (OutputBufferLength=%Iu)\n", OutputLength);
    size_t inputSize = 0;
    auto* inputPtr = (BATTERY_QUERY_INFORMATION*)WdfMemoryGetBuffer(Params->Parameters.Ioctl.Input.Buffer, &inputSize);

    if (inputSize == sizeof(BATTERY_QUERY_INFORMATION)) {
        if ((inputPtr->InformationLevel == BatteryInformation) && (OutputLength == sizeof(BATTERY_INFORMATION))) {
            size_t memSize = 0;
            auto* report = (BATTERY_INFORMATION*)WdfMemoryGetBuffer(OutputMem, &memSize);
            NT_ASSERTMSG("BatteryInformation buffer size mismatch", memSize == OutputLength);
            UpdateBatteryInformation(*report, *context->Interface.State);
        }
        if ((inputPtr->InformationLevel == BatteryTemperature) && (OutputLength == sizeof(ULONG))) {
            size_t memSize = 0;
            auto* temp = (ULONG*)WdfMemoryGetBuffer(OutputMem, &memSize);
            NT_ASSERTMSG("BatteryTemperature buffer size mismatch", memSize == OutputLength);
            UpdateBatteryTemperature(*temp, *context->Interface.State);
        }
    }

    WdfRequestComplete(Request, status);
}

VOID EvtIoDeviceControlBattFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode
)
/*++
Routine Description:
    Callback function for IOCTL_BATTERY_xxx requests.

Arguments:
    Queue - A handle to the queue object that is associated with the I/O request

    Request - A handle to a framework request object.

    OutputBufferLength - The length, in bytes, of the request's output buffer,
            if an output buffer is available.

    InputBufferLength - The length, in bytes, of the request's input buffer, if
            an input buffer is available.

    IoControlCode - The driver or system defined IOCTL associated with the request
--*/
{
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControlBattFilter (IoControlCode=0x%x, InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", IoControlCode, InputBufferLength, OutputBufferLength);

    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);

#if 0
    WdfRequestSetCompletionRoutine(Request, EvtIoDeviceControlBattFilterCompletion, nullptr);

    // Forward the request down the driver stack
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), WDF_NO_SEND_OPTIONS);
#else
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), &options);
#endif
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}
