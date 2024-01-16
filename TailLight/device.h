#pragma once

#include "debug.h"
#include "inc\kmcodesegs.h"

#define POOL_TAG_TAILLIGHT_REPORT 'TLrp'
#define POOL_TAG_WORK_ITEM_CONTEXT 'TLt2'


/** Driver-specific struct for storing instance-specific data. */
typedef struct _DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    ULONG          TailLight; ///< last written color
} DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

EVT_WDF_DRIVER_DEVICE_ADD 
PAGED_CODE_SEG 
EvtDriverDeviceAdd;