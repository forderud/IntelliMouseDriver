#include "luminous.h"
#include <stdexcept>

const wchar_t NAME_SPACE[] = L"root\\WMI";
const wchar_t CLASS_NAME[] = L"FireflyDeviceInformation";
const wchar_t PROPERTY_NAME[] = L"TailLight";


// The function connects to the namespace specified by the user.
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
    // will grant access to system-level objects, and so that
    // CALL authorization will be used.
    hr = CoSetProxyBlanket(
        (IUnknown*)wbemServices, // proxy
        RPC_C_AUTHN_WINNT,        // authentication service
        RPC_C_AUTHZ_NONE,         // authorization service
        NULL,                     // server principle name
        RPC_C_AUTHN_LEVEL_CALL,   // authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE, // impersonation level
        NULL,                     // identity of the client
        0);                      // capability flags

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


CLuminous::CLuminous() {
    // Initialize COM library. Must be done before invoking any other COM function.
    HRESULT hResult = CoInitialize(NULL);

    if ( FAILED (hResult)) {
        _tprintf( TEXT("Error %lx: Failed to initialize COM library\n"), hResult );
        throw std::runtime_error("CoInitialize failure");

    }

    m_pIWbemServices = ConnectToNamespace(NAME_SPACE);
    if (!m_pIWbemServices ) {
        _tprintf( TEXT("Could not connect name.\n") );
        throw std::runtime_error("ConnectToNamespace failure");
    }

     m_pIWbemClassObject = GetInstanceReference(*m_pIWbemServices, CLASS_NAME);
     if ( !m_pIWbemClassObject ) {
        _tprintf( TEXT("Could not find the instance.\n") );
        throw std::runtime_error("GetInstanceReference failure");
    }
}

CLuminous::~CLuminous() {
    m_pIWbemServices.Release();
    m_pIWbemClassObject.Release();

    CoUninitialize();
}


bool CLuminous::Get(COLORREF* Color) {
    if (!Color)
        return false;

    // Get the property value.
    CComVariant varPropVal;
    CIMTYPE  cimType = 0;
    HRESULT hResult = m_pIWbemClassObject->Get(CComBSTR(PROPERTY_NAME), 0, &varPropVal, &cimType, NULL);

    if (hResult != WBEM_S_NO_ERROR) {
        _tprintf( TEXT("Error %lX: Failed to read property value of %s.\n"), hResult, PROPERTY_NAME);
        return false;
    } 
    
    if ((varPropVal.vt != VT_I4) && (varPropVal.vt != VT_UI4)) {
        return false; // variant type mismatch
    }

    *Color = varPropVal.uintVal;
    return true;
}

bool CLuminous::Set(COLORREF Color) {
    // Get the property value.
    CComVariant  varPropVal;
    CIMTYPE     cimType = 0;
    HRESULT hResult = m_pIWbemClassObject->Get(
                             CComBSTR(PROPERTY_NAME),
                             0,
                             &varPropVal,
                             &cimType,
                             NULL );

    if ( hResult != WBEM_S_NO_ERROR ) {
        _tprintf( TEXT("Error %lX: Failed to read property value of %s.\n"), hResult, PROPERTY_NAME);
        return false;
    }

    if ((varPropVal.vt != VT_I4) && (varPropVal.vt != VT_UI4)) {
        return false; // variant type mismatch
    }

    varPropVal.uintVal = Color;

    // Set the property value
    hResult = m_pIWbemClassObject->Put(CComBSTR(PROPERTY_NAME), 0, &varPropVal, cimType);

    if (hResult != WBEM_S_NO_ERROR) {
        _tprintf(TEXT("Error %lX: Failed to set property value of %s.\n"), hResult, PROPERTY_NAME);
        return false;
    }

    hResult = m_pIWbemServices->PutInstance(m_pIWbemClassObject, WBEM_FLAG_UPDATE_ONLY, NULL, NULL);

    if (hResult != WBEM_S_NO_ERROR) {
        _tprintf( TEXT("Failed to save the instance, %s will not be updated.\n"), PROPERTY_NAME);
        return false;
    }
    
    return true;
}
