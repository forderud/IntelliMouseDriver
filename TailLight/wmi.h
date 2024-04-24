// Where they are described.
#define MOFRESOURCENAME L"TailLightWMI"

#define DPF_SOMETHING_FAILED( instance, something ) KdPrint(("TailLight: %s: " #something " failed 0x%x(" ## #instance ## ")\n", __func__, status));

#define IF_FAILED_RETURN_STATUS( instance, something ) \
    if (!NT_SUCCESS(status)) {  \
        DPF_SOMETHING_FAILED( instance, something )   \
        return status;          \
    }


struct COLOR_CONTROL {
    ULONG Colors[2] = { 0xFF00, 0x0 };
    ULONG RemainingColors = 0;
};

#define REMAINING_COLORS_COUNT sizeof(COLOR_CONTROL::Colors) / \
                               sizeof(*COLOR_CONTROL::Colors)

WDF_DECLARE_CONTEXT_TYPE(COLOR_CONTROL);

// Initialize WMI provider
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device);

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;

EVT_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;

EVT_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;
VOID FakeBISTTimerProc(_In_ WDFTIMER timer);