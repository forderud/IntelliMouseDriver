#include "driver.hpp"


// Register our GUID and Datablock generated from the MouseMirror.mof file.
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device)
{
    DECLARE_CONST_UNICODE_STRING(mofRsrcName, MOFRESOURCENAME);

    NTSTATUS status = WdfDeviceAssignMofResourceName(Device, &mofRsrcName);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: Error in WdfDeviceAssignMofResourceName %x\n", status);
        return status;
    }

    WDF_WMI_PROVIDER_CONFIG providerConfig = {};
    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, &MouseMirrorDeviceInformation_GUID);
    providerConfig.MinInstanceBufferSize = sizeof(MouseMirrorDeviceInformation);

    WDF_WMI_INSTANCE_CONFIG instanceConfig = {};
    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig, &providerConfig);
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceQueryInstance = EvtWmiInstanceQueryInstance;
    instanceConfig.EvtWmiInstanceSetInstance = EvtWmiInstanceSetInstance;
    instanceConfig.EvtWmiInstanceSetItem = EvtWmiInstanceSetItem;

    WDF_OBJECT_ATTRIBUTES woa = {};
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&woa, MouseMirrorDeviceInformation);

    WDFWMIINSTANCE WmiInstance = 0;
    status = WdfWmiInstanceCreate(Device, &instanceConfig, &woa, &WmiInstance);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, "MouseMirror: WdfWmiInstanceCreate error %x\n", status);
        return status;
    }

    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);
    deviceContext->WmiInstance = WmiInstance;

    return status;
}

NTSTATUS EvtWmiInstanceQueryInstance(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG OutBufferSize,
    _Out_writes_bytes_to_(OutBufferSize, *BufferUsed)  PVOID OutBuffer,
    _Out_ PULONG BufferUsed
    )
{
    UNREFERENCED_PARAMETER(OutBufferSize); // mininum buffer size already checked by WDF

    DebugEnter();

    MouseMirrorDeviceInformation* pInfo = WdfObjectGet_MouseMirrorDeviceInformation(WmiInstance);
    RtlCopyMemory(/*dst*/OutBuffer, /*src*/pInfo, sizeof(*pInfo));
    *BufferUsed = sizeof(*pInfo);

    DebugExit();
    return STATUS_SUCCESS;
}

NTSTATUS EvtWmiInstanceSetInstance(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    UNREFERENCED_PARAMETER(InBufferSize); // mininum buffer size already checked by WDF

    DebugEnter();

    MouseMirrorDeviceInformation* pInfo = WdfObjectGet_MouseMirrorDeviceInformation(WmiInstance);
    RtlCopyMemory(/*dst*/pInfo, /*src*/InBuffer, sizeof(*pInfo));

    DebugExit();
    return STATUS_SUCCESS;
}

NTSTATUS EvtWmiInstanceSetItem(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG DataItemId,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    DebugEnter();

    MouseMirrorDeviceInformation* pInfo = WdfObjectGet_MouseMirrorDeviceInformation(WmiInstance);
    NTSTATUS status = STATUS_SUCCESS;

    if (DataItemId == MouseMirrorDeviceInformation_FlipLeftRight_ID) {
        if (InBufferSize < MouseMirrorDeviceInformation_FlipLeftRight_SIZE)
            return STATUS_BUFFER_TOO_SMALL;

        pInfo->FlipLeftRight = *(BOOLEAN*)InBuffer;
    } else if (DataItemId == MouseMirrorDeviceInformation_FlipUpDown_ID) {
        if (InBufferSize < MouseMirrorDeviceInformation_FlipUpDown_SIZE)
            return STATUS_BUFFER_TOO_SMALL;

        pInfo->FlipUpDown = *(BOOLEAN*)InBuffer;
    } else {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    DebugExitStatus(status);
    return status;
}
