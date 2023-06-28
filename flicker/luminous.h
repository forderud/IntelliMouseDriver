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
};
