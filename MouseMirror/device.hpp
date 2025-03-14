#pragma once
#include "driver.hpp"
#include <kbdmou.h>


/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    WDFWMIINSTANCE WmiInstance;
    CONNECT_DATA   UpperConnectData; // callback to intercept mouse packets
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(MouseMirrorDeviceInformation)
