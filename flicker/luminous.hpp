#pragma once
#include <atlbase.h>
#include <windows.h>
#include <wbemcli.h>


class Luminous {
public:
    Luminous();
    ~Luminous();

    bool Set(COLORREF Color);
    bool Get(COLORREF*Color);

private:
    CComPtr<IWbemServices> m_wbemServices;
    CComPtr<IWbemClassObject> m_wbemClassObject;
};
