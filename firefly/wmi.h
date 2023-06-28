// Where they are described.
#define MOFRESOURCENAME L"FireflyWMI"

// Initialize the FireFly drivers WMI support
NTSTATUS
WmiInitialize(
    WDFDEVICE       Device,
    PDEVICE_CONTEXT DeviceContext
    );

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;
EVT_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;
EVT_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;
