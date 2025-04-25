#pragma once
#include "driver.hpp"


/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    void*          NotificationHandle; // opaque value to identify PnP notification registration
    WDFWMIINSTANCE WmiInstance;
    WDFTIMER       SelfTestTimer;
    KEVENT         SelfTestCompleted;
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)
