#include "luminous.h"
#include <stdexcept>

const wchar_t NAME_SPACE[] = L"root\\WMI";
const wchar_t CLASS_NAME[] = L"FireflyDeviceInformation";
const wchar_t PROPERTY_NAME[] = L"TailLight";


// The function connects to the namespace specified by the user.
CComPtr<IWbemServices> ConnectToNamespace(_In_ const wchar_t* chNamespace) {
    // Create an instance of WbemLocator interface.
    CComPtr<IWbemLocator> pIWbemLocator;
    HRESULT hResult = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pIWbemLocator);

    if (hResult != S_OK) {
        _tprintf(TEXT("Error %lX: Could not create instance of IWbemLocator interface.\n"),
            hResult);
        return nullptr;
    }

    // Namespaces are passed to COM in BSTRs.
    CComBSTR bstrNamespace = chNamespace;

    // Using the locator, connect to COM in the given namespace.
    CComPtr<IWbemServices> pIWbemServices;
    hResult = pIWbemLocator->ConnectServer(
        bstrNamespace,
        NULL,   // NULL means current account, for simplicity.
        NULL,   // NULL means current password, for simplicity.
        0L,     // locale
        0L,     // securityFlags
        NULL,   // authority (domain for NTLM)
        NULL,   // context
        &pIWbemServices); // Returned IWbemServices.

    if (hResult != WBEM_S_NO_ERROR) {
        _tprintf(TEXT("Error %lX: Failed to connect to namespace %s.\n"),
            hResult, chNamespace);

        return nullptr;
    }

    // Switch the security level to IMPERSONATE so that provider(s)
    // will grant access to system-level objects, and so that
    // CALL authorization will be used.
    hResult = CoSetProxyBlanket(
        (IUnknown*)pIWbemServices, // proxy
        RPC_C_AUTHN_WINNT,        // authentication service
        RPC_C_AUTHZ_NONE,         // authorization service
        NULL,                     // server principle name
        RPC_C_AUTHN_LEVEL_CALL,   // authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE, // impersonation level
        NULL,                     // identity of the client
        0);                      // capability flags

    if (hResult != S_OK) {
        _tprintf(TEXT("Error %lX: Failed to impersonate.\n"), hResult);
        return nullptr;
    }

    return pIWbemServices;
}

// The function returns an interface pointer to the instance given its
// list-index.
CComPtr<IWbemClassObject> GetInstanceReference(IWbemServices* pIWbemServices, _In_ const wchar_t* lpClassName) {
    CComBSTR bstrClassName = lpClassName;

    // Get Instance Enumerator Interface.
    CComPtr<IEnumWbemClassObject> pEnumInst;
    HRESULT hResult = pIWbemServices->CreateInstanceEnum(
        bstrClassName,          // Name of the root class.
        WBEM_FLAG_SHALLOW |     // Enumerate at current root only.
        WBEM_FLAG_FORWARD_ONLY, // Forward-only enumeration.
        NULL,                   // Context.
        &pEnumInst);          // pointer to class enumerator

    CComPtr<IWbemClassObject> pInst;
    if (hResult != WBEM_S_NO_ERROR || pEnumInst == NULL) {
        _tprintf(TEXT("Error %lX: Failed to get a reference to instance enumerator.\n"), hResult);
        return nullptr;
    }

    // Get pointer to the instance.
    hResult = WBEM_S_NO_ERROR;
    while (hResult == WBEM_S_NO_ERROR) {
        ULONG ulCount = 0;
        hResult = pEnumInst->Next(
            2000,      // two seconds timeout
            1,         // return just one instance.
            &pInst,    // pointer to instance.
            &ulCount); // Number of instances returned.

        if (ulCount > 0)
            return pInst;
    }

    return nullptr;
}


CLuminous::CLuminous() {
    // Initialize COM library. Must be done before invoking any
    // other COM function.
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

     m_pIWbemClassObject = GetInstanceReference(m_pIWbemServices, CLASS_NAME);
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
    CComBSTR bstrPropertyName = PROPERTY_NAME;

    // Get the property value.
    CComVariant varPropVal;
    CIMTYPE  cimType = 0;
    HRESULT hResult = m_pIWbemClassObject->Get(bstrPropertyName, 0, &varPropVal, &cimType, NULL);

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
    CComBSTR bstrPropertyName = PROPERTY_NAME;

    // Get the property value.
    CComVariant  varPropVal;
    CIMTYPE     cimType = 0;
    HRESULT hResult = m_pIWbemClassObject->Get(
                             bstrPropertyName,
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
    hResult = m_pIWbemClassObject->Put(bstrPropertyName, 0, &varPropVal, cimType);

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
