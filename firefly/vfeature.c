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
    This routine sets the HID feature by sending HID ioctls to our device.
    These IOCTLs will be handled by HIDUSB and converted into USB requests
    and send to the device.
--*/
{
    PAGED_CODE();

    KdPrint(("Firefly: FireflySetFeature\n"));

    // Preinit for error.
    PHIDP_PREPARSED_DATA preparsedData = NULL;
    WDFIOTARGET          hidTarget = NULL;
    
    NTSTATUS status = WdfIoTargetCreate(WdfObjectContextGetObject(DeviceContext), WDF_NO_OBJECT_ATTRIBUTES, &hidTarget);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: WdfIoTargetCreate failed 0x%x\n", status));
        return status;
    }

    // open in write-only mode
    WDF_IO_TARGET_OPEN_PARAMS openParams = {0};
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &DeviceContext->PdoName, FILE_WRITE_ACCESS);

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

    preparsedData = ExAllocatePool2(POOL_FLAG_NON_PAGED, collectionInformation.DescriptorSize, 'ffly');
    if (!preparsedData) {
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

    if (caps.FeatureReportByteLength != sizeof(HIDMINI_CONTROL_INFO)) {
        KdPrint(("FireFly: FeatureReportByteLength mismatch (%u, %u).\n", caps.FeatureReportByteLength, sizeof(HIDMINI_CONTROL_INFO)));
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

    // Create a report to send to the device.
    // Set feature report values (as observed in USBPcap/Wireshark)
    HIDMINI_CONTROL_INFO report = { 0 };
    Init_HIDMINI_CONTROL_INFO(&report, valueCaps.ReportID, Color);

    WDF_MEMORY_DESCRIPTOR inputDescriptor = {0};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor,
                                      &report,
                                      sizeof(report));
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

    if (hidTarget != NULL) {
        WdfObjectDelete(hidTarget);
    }

    KdPrint(("Firefly: FireflySetFeature completed\n"));

    return status;
}


void Init_HIDMINI_CONTROL_INFO(OUT HIDMINI_CONTROL_INFO* report, IN UCHAR ReportID, IN  ULONG Color) {
    report->ReportId = ReportID; // Report ID 0x24 (36)

    report->Unknown1 = 0xB2; // magic value
    report->Unknown2 = 0x03; // magic value
    // tail-light color
    report->Red   = (Color) & 0xFF; // red;
    report->Green = (Color >> 8) & 0xFF; // green
    report->Blue  = (Color >> 16) & 0xFF; // blue
}

NTSTATUS Clamp_HIDMINI_CONTROL_INFO(HIDMINI_CONTROL_INFO* report) {
    if ((report->Unknown1 != 0xB2) || (report->Unknown2 != 0x03)) {
        KdPrint(("FireFly: SetFeatureFilter: Unknown control Code 0x%x 0x%x\n", report->Unknown1, report->Unknown2));
        return STATUS_NOT_IMPLEMENTED;
    }

    // RGB check
    unsigned int color_sum = report->Red + report->Green + report->Blue;
    if (color_sum > 2 * 255) {
        KdPrint(("FireFly: Clamp_HIDMINI_CONTROL_INFO: Clamping color_sum 0x%x\n", color_sum));
        report->Red /= 2;
        report->Green /= 2;
        report->Blue /= 2;
    }

    return STATUS_SUCCESS;
}
