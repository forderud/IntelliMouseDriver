// Where they are described.
#define MOFRESOURCENAME L"FireflyWMI"

// Initialize the FireFly drivers WMI support
#ifdef __cplusplus
extern "C"
#endif
NTSTATUS WmiInitialize(
    WDFDEVICE       Device,
    DEVICE_CONTEXT* DeviceContext
    );

#ifdef __cplusplus
extern "C"
#endif
EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;

#ifdef __cplusplus
extern "C"
#endif
EVT_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;

#ifdef __cplusplus
extern "C"
#endif
EVT_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;
