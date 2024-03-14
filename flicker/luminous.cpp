#include "luminous.hpp"
#include <stdexcept>

#pragma comment(lib, "wbemuuid.lib")


const wchar_t NAMESPACE[] = L"root\\WMI"; // namespace for hardware drivers (https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wdm-provider)
const wchar_t CLASS_NAME[] = L"TailLightDeviceInformation";
const wchar_t PROPERTY_NAME[] = L"TailLight";


// The function connects to the namespace specified by the user.
// DOC: https://learn.microsoft.com/en-us/windows/win32/wmisdk/example-creating-a-wmi-application
CComPtr<IWbemServices> ConnectToNamespace(_In_ const wchar_t* chNamespace) {
    CComPtr<IWbemLocator> wbemLocator;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&wbemLocator);

    if (hr != S_OK) {
        _tprintf(TEXT("Error %lX: Could not create instance of IWbemLocator interface.\n"),
            hr);
        return nullptr;
    }

    // Using the locator, connect to COM in the given namespace.
    CComPtr<IWbemServices> wbemServices;
    hr = wbemLocator->ConnectServer(
        CComBSTR(chNamespace),
        NULL,   // current account
        NULL,   // current password
        0L,     // locale
        0L,     // securityFlags
        NULL,   // authority (domain for NTLM)
        NULL,   // context
        &wbemServices); // Returned IWbemServices.

    if (hr != WBEM_S_NO_ERROR) {
        _tprintf(TEXT("Error %lX: Failed to connect to namespace %s.\n"), hr, chNamespace);

        return nullptr;
    }

    // Switch the security level to IMPERSONATE so that provider(s)
    // will grant access to system-level objects, and so that CALL authorization will be used.
    hr = CoSetProxyBlanket(
        (IUnknown*)wbemServices, // proxy
        RPC_C_AUTHN_WINNT,        // authentication service
        RPC_C_AUTHZ_NONE,         // authorization service
        NULL,                     // server principle name
        RPC_C_AUTHN_LEVEL_CALL,   // authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE,// impersonation level
        NULL,                     // identity of the client
        EOAC_NONE);               // capability flags

    if (hr != S_OK) {
        _tprintf(TEXT("Error %lX: Failed to impersonate.\n"), hr);
        return nullptr;
    }

    return wbemServices;
}

// The function returns an interface pointer to the instance given its list-index.
CComPtr<IWbemClassObject> GetInstanceReference(IWbemServices& pIWbemServices, _In_ const wchar_t* lpClassName) {
    // Get Instance Enumerator Interface.
    CComPtr<IEnumWbemClassObject> enumInst;
    HRESULT hr = pIWbemServices.CreateInstanceEnum(
        CComBSTR(lpClassName),  // Name of the root class.
        WBEM_FLAG_SHALLOW |     // Enumerate at current root only.
        WBEM_FLAG_FORWARD_ONLY, // Forward-only enumeration.
        NULL,                   // Context.
        &enumInst);          // pointer to class enumerator

    if (hr != WBEM_S_NO_ERROR || enumInst == NULL) {
        _tprintf(TEXT("Error %lX: Failed to get a reference to instance enumerator.\n"), hr);
        return nullptr;
    }

    // Get pointer to the instance.
    hr = WBEM_S_NO_ERROR;
    while (hr == WBEM_S_NO_ERROR) {
        ULONG count = 0;
        CComPtr<IWbemClassObject> inst;
        hr = enumInst->Next(
            2000,      // two seconds timeout
            1,         // return just one instance.
            &inst,    // pointer to instance.
            &count); // Number of instances returned.

        if (count > 0)
            return inst;
    }

    return nullptr;
}


Luminous::Luminous() {
    // Initialize COM library. Must be done before invoking any other COM function.
    HRESULT hr = CoInitialize(NULL);

    if ( FAILED (hr)) {
        _tprintf( TEXT("Error %lx: Failed to initialize COM library\n"), hr);
        throw std::runtime_error("CoInitialize failure");

    }

    m_wbemServices = ConnectToNamespace(NAMESPACE);
    if (!m_wbemServices) {
        _tprintf( TEXT("Could not connect name.\n") );
        throw std::runtime_error("ConnectToNamespace failure");
    }

    m_wbemClassObject = GetInstanceReference(*m_wbemServices, CLASS_NAME);
     if ( !m_wbemClassObject) {
        _tprintf( TEXT("Could not find the instance.\n") );
        throw std::runtime_error("GetInstanceReference failure");
    }
}

Luminous::~Luminous() {
    m_wbemServices.Release();
    m_wbemClassObject.Release();

    CoUninitialize();
}


bool Luminous::Get(COLORREF* Color) {
    if (!Color)
        return false;

    // Get the property value.
    CComVariant varPropVal;
    CIMTYPE  cimType = 0;
    HRESULT hr = m_wbemClassObject->Get(CComBSTR(PROPERTY_NAME), 0, &varPropVal, &cimType, NULL);

    if (hr != WBEM_S_NO_ERROR) {
        _tprintf( TEXT("Error %lX: Failed to read property value of %s.\n"), hr, PROPERTY_NAME);
        return false;
    } 
    
    if ((varPropVal.vt != VT_I4) && (varPropVal.vt != VT_UI4)) {
        return false; // variant type mismatch
    }

    *Color = varPropVal.uintVal;
    return true;
}

bool Luminous::Set(COLORREF Color) {
    // Get the property value.
    CComVariant  varPropVal;
    CIMTYPE     cimType = 0;
    HRESULT hr = m_wbemClassObject->Get(
                             CComBSTR(PROPERTY_NAME),
                             0,
                             &varPropVal,
                             &cimType,
                             NULL );

    if (hr != WBEM_S_NO_ERROR ) {
        _tprintf( TEXT("Error %lX: Failed to read property value of %s.\n"), hr, PROPERTY_NAME);
        return false;
    }

    if ((varPropVal.vt != VT_I4) && (varPropVal.vt != VT_UI4)) {
        return false; // variant type mismatch
    }

    varPropVal.uintVal = Color;

    // Set the property value
    hr = m_wbemClassObject->Put(CComBSTR(PROPERTY_NAME), 0, &varPropVal, cimType);

    if (hr != WBEM_S_NO_ERROR) {
        _tprintf(TEXT("Error %lX: Failed to set property value of %s.\n"), hr, PROPERTY_NAME);
        return false;
    }

    hr = m_wbemServices->PutInstance(m_wbemClassObject, WBEM_FLAG_UPDATE_ONLY, NULL, NULL);

    if (hr != WBEM_S_NO_ERROR) {
        _tprintf( TEXT("Failed to save the instance, %s will not be updated.\n"), PROPERTY_NAME);
        return false;
    }
    
    return true;
}
