#pragma once

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT EvtDeviceSelfManagedInitFilter;
NTSTATUS SetBlack(
    WDFDEVICE device); // TODO: Remove