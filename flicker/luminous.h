#pragma once
#include <atlbase.h>
#include <windows.h>
#include <wbemcli.h>

#define NAME_SPACE TEXT("root\\WMI")
#define CLASS_NAME  TEXT("FireflyDeviceInformation")
#define PROPERTY_NAME TEXT("TailLight")

class CLuminous {
public:
    CLuminous();
    virtual ~CLuminous();

    bool Set(COLORREF Color);
    bool Get(COLORREF*Color);

private:
    CComPtr<IWbemServices> m_pIWbemServices;
    CComPtr<IWbemClassObject> m_pIWbemClassObject;
    bool m_bCOMInitialized = false;
};


IWbemServices *ConnectToNamespace (
                                _In_ LPTSTR chNamespace);
IWbemClassObject *GetInstanceReference (
                                    IWbemServices *pIWbemServices,
                                    _In_ LPTSTR lpClassName);

BSTR AnsiToBstr (
    _In_ LPTSTR lpSrc,
    _In_ int nLenSrc);
