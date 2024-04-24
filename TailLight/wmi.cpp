#include "driver.h"



VOID FakeBISTTimerProc(_In_ WDFTIMER timer) {
    WDFDEVICE Device = static_cast<WDFDEVICE>(WdfTimerGetParentObject(timer));    
    COLOR_CONTROL* pColorController = WdfObjectGet_COLOR_CONTROL(timer);
    NTSTATUS status = SetFeatureColor(Device, pColorController->Colors[pColorController->RemainingColors]);
    if (!NT_SUCCESS(status)) {
        DPF_SOMETHING_FAILED(instance, something); }

    if (pColorController->RemainingColors) {
        pColorController->RemainingColors = pColorController->RemainingColors - 1;
        
        BOOLEAN timerInQueue = WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_SEC(1));
        NT_ASSERTMSG("Previous active timer overwritten", !timerInQueue);
    }
    else {
        pColorController->RemainingColors = REMAINING_COLORS_COUNT;
    }
}

NTSTATUS FakeBIST(_In_ WDFDEVICE Device) {
/*++
    
 Routine Description: Performs a fake Built-In Self Test (BIST)

--*/
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    WDFTIMER* pTimer = &WdfObjectGet_DEVICE_CONTEXT(Device)->FakeBISTTimer;
    auto pColorController = WdfObjectGet_COLOR_CONTROL(*pTimer);

    // FakeBIST is called in an arbitrary thread. If more than one thread is in
    // FakeBIST at the same time, then the call to SetFeatureColor will result
    // in an invalid taillight color sequence. Simply return that the device is
    // busy using an invalid COLOR_CONTROL::RemainingColors value to signify
    // that the hardware is free. Otherwise we can queue up the request or 
    // block. Both of those mechanisms would require notifying that the
    // hardware is busy and cancelation.
    // 
    // TODO: Maybe a function to get the last BIST test result and time.
    if (pColorController->RemainingColors != REMAINING_COLORS_COUNT) {
        return STATUS_DEVICE_BUSY;
    }

    InterlockedDecrement((LONG*) & pColorController->RemainingColors);

    // Indicate that we are "starting" the test.
    status = SetFeatureColor(Device, 0xFF);

    pTimer = &WdfObjectGet_DEVICE_CONTEXT(Device)->FakeBISTTimer;
    IF_FAILED_RETURN_STATUS(2, "SetFeatureColor")

    #pragma warning(suppress : 6387) // *pTimer initialized in AddDevice
    BOOLEAN timerInQueue = WdfTimerStart(*pTimer, WDF_REL_TIMEOUT_IN_SEC(1));
    NT_ASSERTMSG("Previous active timer overwritten", !timerInQueue);

    return status;
}

static NTSTATUS EvtWmiBISTInstanceExecuteMethod(
    _In_    WDFWMIINSTANCE WmiReportInstance,
    _In_    ULONG MethodId,
    _In_    ULONG InBufferSize,
    _In_    ULONG OutBufferSize,
    _Inout_ PVOID Buffer,
    _Out_   PULONG BufferUsed
) {
    UNREFERENCED_PARAMETER(InBufferSize);
    UNREFERENCED_PARAMETER(OutBufferSize);
    UNREFERENCED_PARAMETER(Buffer);

    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    switch (MethodId) {
    case BIST:
            status = FakeBIST(WdfWmiInstanceGetDevice(WmiReportInstance));
            *BufferUsed = 0;
        break;

    default:
        break;
    }

    KdPrint(("TailLight: Returning 0x%x from BIST launch.\n", status));
    return status;
}


// Register our GUID and Datablock generated from the TailLight.mof file.
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device)
{
    DECLARE_CONST_UNICODE_STRING(mofRsrcName, MOFRESOURCENAME);

    NTSTATUS status = WdfDeviceAssignMofResourceName(Device, &mofRsrcName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: Error in WdfDeviceAssignMofResourceName %x\n", status));
        return status;
    }

    WDF_WMI_PROVIDER_CONFIG providerConfig = {};
    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, &TailLightDeviceInformation_GUID);
    providerConfig.MinInstanceBufferSize = sizeof(TailLightDeviceInformation);

    WDF_WMI_INSTANCE_CONFIG instanceConfig = {};
    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig, &providerConfig);
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceQueryInstance = EvtWmiInstanceQueryInstance;
    instanceConfig.EvtWmiInstanceSetInstance = EvtWmiInstanceSetInstance;
    instanceConfig.EvtWmiInstanceSetItem = EvtWmiInstanceSetItem;

    WDF_OBJECT_ATTRIBUTES woa = {};
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&woa, TailLightDeviceInformation);

    WDFWMIINSTANCE WmiLastCreatedInstance = 0;
    status = WdfWmiInstanceCreate(Device, &instanceConfig, &woa, &WmiLastCreatedInstance);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfWmiInstanceCreate of TailLightDeviceInformation error %x\n", status));
        return status;
    }

    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);
    deviceContext->WmiReportInstance = WmiLastCreatedInstance;

    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, &TailLightBIST_GUID);
    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig, &providerConfig);
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceExecuteMethod = EvtWmiBISTInstanceExecuteMethod;
    status = WdfWmiInstanceCreate(Device, 
                                  &instanceConfig, 
                                  WDF_NO_OBJECT_ATTRIBUTES, 
                                  &WmiLastCreatedInstance);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfWmiInstanceCreate of TailLightBIST error is %x\n", status));
        return status;
    }

    deviceContext->WmiBISTInstance = WmiLastCreatedInstance;

    return status;
}

NTSTATUS EvtWmiInstanceQueryInstance(
    _In_  WDFWMIINSTANCE WmiReportInstance,
    _In_  ULONG OutBufferSize,
    _Out_writes_bytes_to_(OutBufferSize, *BufferUsed)  PVOID OutBuffer,
    _Out_ PULONG BufferUsed
    )
{
    UNREFERENCED_PARAMETER(OutBufferSize); // mininum buffer size already checked by WDF

    KdPrint(("TailLight: WMI QueryInstance\n"));

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiReportInstance);
    RtlCopyMemory(/*dst*/OutBuffer, /*src*/pInfo, sizeof(*pInfo));
    *BufferUsed = sizeof(*pInfo);

    KdPrint(("TailLight: WMI QueryInstance completed\n"));
    return STATUS_SUCCESS;
}

NTSTATUS EvtWmiInstanceSetInstance(
    _In_  WDFWMIINSTANCE WmiReportInstance,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    UNREFERENCED_PARAMETER(InBufferSize); // mininum buffer size already checked by WDF

    KdPrint(("TailLight: WMI SetInstance\n"));

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiReportInstance);
    RtlCopyMemory(/*dst*/pInfo, /*src*/InBuffer, sizeof(*pInfo));

    // call SetFeatureColor to trigger tail-light update
    NTSTATUS status = SetFeatureColor(WdfWmiInstanceGetDevice(WmiReportInstance), pInfo->TailLight);

    KdPrint(("TailLight: WMI SetInstance completed\n"));
    return status;
}

NTSTATUS EvtWmiInstanceSetItem(
    _In_  WDFWMIINSTANCE WmiReportInstance,
    _In_  ULONG DataItemId,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    KdPrint(("TailLight: WMI SetItem\n"));

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiReportInstance);
    NTSTATUS status = STATUS_SUCCESS;

    if (DataItemId == TailLightDeviceInformation_TailLight_ID) {
        if (InBufferSize < TailLightDeviceInformation_TailLight_SIZE)
            return STATUS_BUFFER_TOO_SMALL;

        pInfo->TailLight = *(ULONG*)InBuffer;

        // call SetFeatureColor to trigger tail-light update
        status = SetFeatureColor(WdfWmiInstanceGetDevice(WmiReportInstance), pInfo->TailLight);
    } else {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    KdPrint(("TailLight: WMI SetItem completed\n"));
    return status;
}
