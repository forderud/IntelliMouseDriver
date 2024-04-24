#pragma once
#include <Windows.h>
#include <dontuse.h>
#include <comutil.h>
#include <wbemcli.h>
#include <winrt/base.h>

HRESULT 
ConnectToWbem();

HRESULT
SetInterfaceSecurity(
    _In_     IUnknown* InterfaceObj,
    _In_opt_ PWSTR UserId,
    _In_opt_ PWSTR Password,
    _In_opt_ PWSTR DomainName
);

HRESULT
ExecuteMethod_NoArgs_ReturnsValue(
    _In_ winrt::com_ptr<IWbemServices>* pWbemServices,
    _In_ winrt::com_ptr<IWbemClassObject>* pClassObj,
    _In_ const BSTR InstancePath,
    _In_ const OLECHAR* psz,
    _Out_ HRESULT& wmiMethodRet
);

HRESULT
ExecuteBISTOnAllDevices(
    _In_     winrt::com_ptr<IWbemServices>* pWbemServices,
    _In_opt_ PWSTR UserId,
    _In_opt_ PWSTR Password,
    _In_opt_ PWSTR DomainName
);