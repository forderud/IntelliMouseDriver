#include "device.hpp"
#include <hidpddi.h> // for PHIDP_PREPARSED_DATA
#include <hidclass.h> // for HID_COLLECTION_INFORMATION
#include "eventlog.hpp"
#include "CppAllocator.hpp"
#include "vfeature.hpp"


/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PHIDP_PREPARSED_DATA_Wrap {
public:
    PHIDP_PREPARSED_DATA_Wrap(size_t size) {
        m_ptr = new BYTE[size];
    }
    ~PHIDP_PREPARSED_DATA_Wrap() {
        if (m_ptr) {
            delete[] m_ptr;
            m_ptr = nullptr;
        }
    }

    operator PHIDP_PREPARSED_DATA () const {
        return (PHIDP_PREPARSED_DATA)m_ptr;
    }

private:
    BYTE* m_ptr = nullptr;
};


NTSTATUS SetFeatureColor (
    _In_ WDFDEVICE Device,
    _In_ ULONG     Color
    )
/*++
    This routine sets the HID feature by sending HID ioctls to our device.
    These IOCTLs will be handled by HIDUSB and converted into USB requests
    and send to the device.
--*/
{
    DebugEnter();

#if 0
    WDFIOTARGET localTarget = WdfDeviceGetIoTarget(Device);
#endif
    WDFIOTARGET_Wrap pdoTarget;
    {
        // Use PDO for HID commands instead of local IO target to avoid 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) on IOCTL_HID_SET_FEATURE
        NTSTATUS status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &pdoTarget);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfIoTargetCreate failed 0x%x"), status);
            return status;
        }

        // open in shared read-write mode
        DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);
        WDF_IO_TARGET_OPEN_PARAMS openParams = {};
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &deviceContext->PdoName, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
        // We will let the framework to respond automatically to the pnp state changes of the target by closing and opening the handle.
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

        status = WdfIoTargetOpen(pdoTarget, &openParams);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfIoTargetOpen failed 0x%x"), status);
            return status;
        }
    }

    HID_COLLECTION_INFORMATION collectionInfo = {};
    {
        // populate "collectionInformation"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &collectionInfo, sizeof(HID_COLLECTION_INFORMATION));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_INFORMATION,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: IOCTL_HID_GET_COLLECTION_INFORMATION failed 0x%x"), status);
            return status;
        }

        //DebugPrint(DPFLTR_INFO_LEVEL, "TailLight: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInfo.ProductID, collectionInfo.VendorID, collectionInfo.VersionNumber, collectionInfo.DescriptorSize);
    }

    PHIDP_PREPARSED_DATA_Wrap preparsedData(collectionInfo.DescriptorSize);
    if (!preparsedData) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    {
        // populate "preparsedData"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, static_cast<PHIDP_PREPARSED_DATA>(preparsedData), collectionInfo.DescriptorSize);

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_DESCRIPTOR, // same as HidD_GetPreparsedData in user-mode
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: IOCTL_HID_GET_COLLECTION_DESCRIPTOR failed 0x%x"), status);
            return status;
        }
    }

    {
        // get capabilities
        HIDP_CAPS caps = {};
        NTSTATUS status = HidP_GetCaps(preparsedData, &caps);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        //DebugPrint(DPFLTR_INFO_LEVEL, "TailLight: Usage=%x, UsagePage=%x\n", caps.Usage, caps.UsagePage);

        if (caps.FeatureReportByteLength != sizeof(TailLightReport)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: FeatureReportByteLength mismatch (%u, %Iu)."), caps.FeatureReportByteLength, sizeof(TailLightReport));
            return status;
        }
    }

    {
#if 0
        // Get TailLightReport from device.
        TailLightReport report;

        // WARNING: Call succeeds but doesn't update the report due to a IntelliMouse HW issue
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &report, sizeof(report));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_FEATURE,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }

        report.Print("Previous color");
#endif
    }

    {
        // Create a report to send to the device.
        TailLightReport report;
        report.SetColor(Color);

        // send TailLightReport to device
        WDF_MEMORY_DESCRIPTOR inputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDesc, &report, sizeof(report));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_SET_FEATURE,
            &inputDesc, // input
            NULL, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            // IOCTL_HID_SET_FEATURE fails with 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) if using the local IO target (WdfDeviceGetIoTarget)
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: IOCTL_HID_SET_FEATURE failed 0x%x"), status);
            return status;
        }

        report.Print("New color");
    }

    DebugExit();
    return STATUS_SUCCESS;
}


NTSTATUS SetFeatureFilter(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request,
    _In_ size_t     InputBufferLength
)
/*++
Routine Description:
    Handles IOCTL_HID_SET_FEATURE for all the collection.
    For control collection (custom defined collection) it handles
    the user-defined control codes for sideband communication

Arguments:
    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.
--*/
{
    DebugEnter();
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

    if (InputBufferLength != sizeof(TailLightReport)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: SetFeatureFilter: Incorrect InputBufferLength"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    TailLightReport* report = nullptr;
    NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(TailLightReport), (void**)&report, NULL);
    if (!NT_SUCCESS(status) || !report) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfRequestRetrieveInputBuffer failed 0x%x, report=0x%p"), status, report);
        return status;
    }

    if (!report->IsValid()) {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        return STATUS_INVALID_PARAMETER;
    }

    // capture color before safety adjustments
    UCHAR r = report->Red;
    UCHAR g = report->Green;
    UCHAR b = report->Blue;
    report->Print("Requested color");

    // Enforce safety limits (sets color to RED on failure)
    if (!report->SafetyCheck()) {
        // log safety violation to Windows Event Viewer "System" log
        WCHAR color_requested[16] = {};
        swprintf_s(color_requested, L"%u,%u,%u", r, g, b);
        WCHAR color_adjusted[16] = {};
        swprintf_s(color_adjusted, L"%u,%u,%u", report->Red, report->Green, report->Blue);

        WriteToSystemLog(Device, TailLight_SAFETY, color_requested, color_adjusted);
        status =  STATUS_CONTENT_BLOCKED;
    }

    // update last written color
    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(deviceContext->WmiInstance);
    pInfo->TailLight = report->GetColor();

    DebugExitStatus(status);
    return status;
}
