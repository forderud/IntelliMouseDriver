// The device context performs the same job as
// a WDM device extension in the driver framework
typedef struct _DEVICE_CONTEXT
{
    // Our WMI data generated from firefly.mof
    FireflyDeviceInformation WmiInstance;

    UNICODE_STRING PdoName;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(FireflyDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;
