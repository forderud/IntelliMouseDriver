#include "firefly.h"

#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

#include <hidpddi.h>
#include <hidclass.h>

#pragma warning(default:4201)
#pragma warning(default:4214)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FireflySetFeature)
#endif

NTSTATUS
FireflySetFeature(
    IN  DEVICE_CONTEXT* DeviceContext,
    IN  ULONG           Color
    )
/*++
Routine Description:
    This routine sets the HID feature by sending HID ioctls to our device.
    These IOCTLs will be handled by HIDUSB and converted into USB requests
    and send to the device.

Arguments:
    DeviceContext - Context for our device

    PageID  - UsagePage of the light control feature.

    FeatureId - Usage ID of the feature.

    EnanbleFeature - True to turn the light on, Falst to turn if off.

Return Value:
    NT Status code
--*/
{
    PAGED_CODE();

    // Preinit for error.
    PHIDP_PREPARSED_DATA preparsedData = NULL;
    PCHAR                report = NULL;
    WDFIOTARGET          hidTarget = NULL;
    
    NTSTATUS status = WdfIoTargetCreate(WdfObjectContextGetObject(DeviceContext), 
                            WDF_NO_OBJECT_ATTRIBUTES, 
                            &hidTarget);    
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: WdfIoTargetCreate failed 0x%x\n", status));        
        return status;
    }

    // open in write-only mode
    WDF_IO_TARGET_OPEN_PARAMS openParams = {0};
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
                                    &openParams,
                                    &DeviceContext->PdoName,
                                    FILE_WRITE_ACCESS);

    KdPrint(("Firefly: DeviceContext->PdoName: %wZ\n", DeviceContext->PdoName)); // outputs "\Device\00000083"

    // We will let the framework to respond automatically to the pnp
    // state changes of the target by closing and opening the handle.
    openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

    status = WdfIoTargetOpen(hidTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: WdfIoTargetOpen failed 0x%x\n", status));                
        goto ExitAndFree;
    }
    

    WDF_MEMORY_DESCRIPTOR      outputDescriptor = {0};
    HID_COLLECTION_INFORMATION collectionInformation = {0};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, // out (mapped to collectionInformation)
                                      (PVOID) &collectionInformation,
                                      sizeof(HID_COLLECTION_INFORMATION));

    // Now get the collection information for this device
    status = WdfIoTargetSendIoctlSynchronously(hidTarget,
                                  NULL,
                                  IOCTL_HID_GET_COLLECTION_INFORMATION,
                                  NULL,
                                  &outputDescriptor,
                                  NULL,
                                  NULL);

    KdPrint(("FireFly: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInformation.ProductID, collectionInformation.VendorID, collectionInformation.VersionNumber, collectionInformation.DescriptorSize));

    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: WdfIoTargetSendIoctlSynchronously1 failed 0x%x\n", status));                
        goto ExitAndFree;
    }

    preparsedData = (PHIDP_PREPARSED_DATA) ExAllocatePool2(
        POOL_FLAG_NON_PAGED, collectionInformation.DescriptorSize, 'ffly');

    if (preparsedData == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto ExitAndFree;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, // out (mapped to preparsedData)
                                      (PVOID) preparsedData,
                                      collectionInformation.DescriptorSize);

    status = WdfIoTargetSendIoctlSynchronously(hidTarget,
                                  NULL,
                                  IOCTL_HID_GET_COLLECTION_DESCRIPTOR, // same as HidD_GetPreparsedData in user-mode
                                  NULL,
                                  &outputDescriptor,
                                  NULL,
                                  NULL);

    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: WdfIoTargetSendIoctlSynchronously2 failed 0x%x\n", status));                
        goto ExitAndFree;
    }

    // Now get the capabilities.
    HIDP_CAPS caps = {0};
    RtlZeroMemory(&caps, sizeof(HIDP_CAPS));

    status = HidP_GetCaps(preparsedData, &caps);

    if (!NT_SUCCESS(status)) {
        goto ExitAndFree;
    }

    KdPrint(("FireFly: Usage=%x, UsagePage=%x\n", caps.Usage, caps.UsagePage));
    KdPrint(("FireFly: FeatureReportByteLength: %u\n", caps.FeatureReportByteLength));

    if (caps.FeatureReportByteLength != 73) {
        KdPrint(("FireFly: FeatureReportByteLength mismatch.\n"));
        goto ExitAndFree;
    }

    // Create a report to send to the device.
    report = (PCHAR) ExAllocatePool2(
        POOL_FLAG_NON_PAGED, caps.FeatureReportByteLength+1, 'ffly');

    if (report == NULL) {
        goto ExitAndFree;
    }

    // Start with a zeroed report. If we are disabling the feature, this might
    // be all we need to do.
    status = STATUS_SUCCESS;

    HIDP_VALUE_CAPS valueCaps = {0};
    USHORT ValueCapsLength = caps.NumberFeatureValueCaps;
    status = HidP_GetValueCaps(HidP_Feature, &valueCaps, &ValueCapsLength, preparsedData);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: HidP_GetValueCaps failed 0x%x\n", status));
        goto ExitAndFree;
    }

    // Set feature report values (as observed in USBPcap/Wireshark)
    report[0] = valueCaps.ReportID; // Report ID 0x24 (36)
    report[1] = 0xB2; // magic value
    report[2] = 0x03; // magic value
    // tail-light color
    report[3] = (Color)      & 0xFF; // red
    report[4] = (Color >> 8) & 0xFF; // green
    report[5] = (Color >> 16) & 0xFF; // blue

    WDF_MEMORY_DESCRIPTOR inputDescriptor = {0};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor,
                                      report,
                                      caps.FeatureReportByteLength+1);
    status = WdfIoTargetSendIoctlSynchronously(hidTarget,
                                  NULL,
                                  IOCTL_HID_SET_FEATURE,
                                  &inputDescriptor,
                                  NULL,
                                  NULL,
                                  NULL);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: WdfIoTargetSendIoctlSynchronously3 failed 0x%x\n", status)); 
        goto ExitAndFree;
    }

ExitAndFree:
    if (preparsedData != NULL) {
        ExFreePool(preparsedData);
        preparsedData = NULL;
    }

    if (report != NULL) {
        ExFreePool(report);
        report = NULL;
    }

    if (hidTarget != NULL) {
        WdfObjectDelete(hidTarget);
    }

    return status;
}
