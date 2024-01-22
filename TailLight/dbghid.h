#pragma once

#ifdef DBG
PCSTR GetIoctlString(ULONG hidIoctl, PSTR pszDef, size_t cbDefStringSize);

#define GET_IOCTL_STRING(hidIoctl, pszDef, cbDefStringSize) \
	GetIoctlString(hidIoctl, pszDef, cbDefStringSize);
#else
	#define GET_IOCTL_STRING(hidIoctl, pszDef, cbDefStringSize)
#endif
