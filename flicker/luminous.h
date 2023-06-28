#pragma once
#include <atlbase.h>
#include <windows.h>
#include <wbemcli.h>


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
