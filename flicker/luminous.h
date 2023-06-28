#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <tchar.h>
#include <wchar.h>
#include <initguid.h>
#include <wbemcli.h>

#define NAME_SPACE TEXT("root\\WMI")
#define CLASS_NAME  TEXT("FireflyDeviceInformation")
#define PROPERTY_NAME TEXT("TailLight")

class CLuminous {
public:
    CLuminous();
    virtual ~CLuminous(VOID);

    BOOL Set(_In_ COLORREF Color);
    BOOL Get(_In_ COLORREF*Color);

private:
    IWbemServices* m_pIWbemServices = nullptr;
    IWbemClassObject* m_pIWbemClassObject = nullptr;
    BOOL m_bCOMInitialized = false;
};


IWbemServices *ConnectToNamespace (
                                _In_ LPTSTR chNamespace);
IWbemClassObject *GetInstanceReference (
                                    IWbemServices *pIWbemServices,
                                    _In_ LPTSTR lpClassName);

BSTR AnsiToBstr (
    _In_ LPTSTR lpSrc,
    _In_ int nLenSrc);
