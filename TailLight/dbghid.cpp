#include "driver.h"
#include <Hidport.h>

#ifdef DBG
PCSTR GetIoctlString(ULONG hidIoctl, PSTR pszDef, size_t cbDefStringSize) {
	
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	switch (hidIoctl) {
	case IOCTL_HID_ACTIVATE_DEVICE:
		return "IOCTL_HID_ACTIVATE_DEVICE";
		break;
	case IOCTL_HID_DEACTIVATE_DEVICE:
		return "IOCTL_HID_DEACTIVATE_DEVICE";
		break;
	case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
		return "IOCTL_HID_GET_DEVICE_ATTRIBUTES";
		break;
	case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
		return "IOCTL_HID_GET_DEVICE_DESCRIPTOR";
		break;
	case IOCTL_HID_GET_REPORT_DESCRIPTOR:
		return "IOCTL_HID_GET_REPORT_DESCRIPTOR";
		break;
	case IOCTL_HID_GET_STRING:
		return "IOCTL_HID_GET_STRING";
		break;
	case IOCTL_HID_READ_REPORT:
		return "IOCTL_HID_READ_REPORT";
		break;
	case IOCTL_HID_GET_COLLECTION_INFORMATION:
		return "IOCTL_HID_GET_COLLECTION_INFORMATION";
		break;
	case IOCTL_HID_GET_INPUT_REPORT:
		return "IOCTL_HID_GET_INPUT_REPORT";
		break;
	case IOCTL_HID_SET_FEATURE:
		return "IOCTL_HID_SET_FEATURE";
		break;
	case IOCTL_HID_GET_COLLECTION_DESCRIPTOR:
		return "IOCTL_GET_COLLECTION_DESCRIPTOR";
		break;
	case IOCTL_HID_DEVICERESET_NOTIFICATION:
		return "IOCTL_HID_DEVICERESET_NOTIFICATION";
		break;
	default:
		status = RtlStringCbPrintfA(pszDef, cbDefStringSize, "Ioctl 0x%x", hidIoctl);

		if (NT_SUCCESS(status)) {
			return pszDef;
		}
		else {
			return "";
		}

		break;
	}
}
#endif