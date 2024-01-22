#include "driver.h"
#include "debug.h"

NTSTATUS DumpTarget(WDFIOTARGET target, WDFMEMORY& memory)
{
    // sets the filobject in the stack location.
    WDF_OBJECT_ATTRIBUTES attributes = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    NTSTATUS status = WdfIoTargetAllocAndQueryTargetProperty(target,
        DevicePropertyPhysicalDeviceObjectName,
        NonPagedPoolNx,
        &attributes,
        &memory);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfDeviceAllocAndQueryProperty failed 0x%x\n", status));
        return STATUS_UNSUCCESSFUL;
    }

    UNICODE_STRING targetName;

    size_t bufferLength = 0;
    targetName.Buffer = (WCHAR*)WdfMemoryGetBuffer(memory, &bufferLength);
    if (targetName.Buffer == NULL)
        return STATUS_UNSUCCESSFUL;

    targetName.MaximumLength = (USHORT)bufferLength;
    targetName.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);

    KdPrint(("TailLight: target name: %wZ\n", targetName));

    return STATUS_SUCCESS;
}