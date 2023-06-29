#include "luminous.h"
#include <stdexcept>

const wchar_t NAME_SPACE[] = L"root\\WMI";
const wchar_t CLASS_NAME[] = L"FireflyDeviceInformation";
const wchar_t PROPERTY_NAME[] = L"TailLight";

// The function converts an ANSI string into BSTR and returns it in an
// allocated memory. The memory must be freed by the caller.
BSTR AnsiToBstr(_In_ const wchar_t* lpSrc) {
    UINT nLenSrc = 0;

    if (lpSrc) {
        size_t temp = wcslen(lpSrc);
        if (temp > INT_MAX - 1) {
            return NULL;
        }
        nLenSrc = (UINT)temp + 1;
    }

    return SysAllocStringLen(lpSrc, nLenSrc);
}

// The function connects to the namespace specified by the user.
IWbemServices* ConnectToNamespace(_In_ const wchar_t* chNamespace)
{
    IWbemServices* pIWbemServices = NULL;
    IWbemLocator* pIWbemLocator = NULL;

    //
    // Create an instance of WbemLocator interface.
    HRESULT hResult = CoCreateInstance(
        CLSID_WbemLocator,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pIWbemLocator);

    if (hResult != S_OK) {
        _tprintf(TEXT("Error %lX: Could not create instance of IWbemLocator interface.\n"),
            hResult);
        return NULL;
    }

    // Namespaces are passed to COM in BSTRs.
    CComBSTR bstrNamespace = AnsiToBstr(chNamespace);

    // Using the locator, connect to COM in the given namespace.
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

        pIWbemLocator->Release();
        return NULL;
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

        pIWbemLocator->Release();
        pIWbemServices->Release();
        return NULL;
    }

    pIWbemLocator->Release();
    return pIWbemServices;
}

// The function returns an interface pointer to the instance given its
// list-index.
IWbemClassObject* GetInstanceReference(
    IWbemServices* pIWbemServices,
    _In_ const wchar_t* lpClassName)
{
    IWbemClassObject* pInst = NULL;
    IEnumWbemClassObject* pEnumInst;
    BOOL                 bFound;
    ULONG                ulCount;
    HRESULT              hResult;

    CComBSTR bstrClassName = AnsiToBstr(lpClassName);

    // Get Instance Enumerator Interface.
    pEnumInst = NULL;

    hResult = pIWbemServices->CreateInstanceEnum(
        bstrClassName,          // Name of the root class.
        WBEM_FLAG_SHALLOW |     // Enumerate at current root only.
        WBEM_FLAG_FORWARD_ONLY, // Forward-only enumeration.
        NULL,                   // Context.
        &pEnumInst);          // pointer to class enumerator

    if (hResult != WBEM_S_NO_ERROR || pEnumInst == NULL) {
        _tprintf(TEXT("Error %lX: Failed to get a reference")
            TEXT(" to instance enumerator.\n"), hResult);
    }
    else {
        // Get pointer to the instance.
        hResult = WBEM_S_NO_ERROR;
        bFound = FALSE;

        while ((hResult == WBEM_S_NO_ERROR) && (bFound == FALSE)) {
            hResult = pEnumInst->Next(
                2000,      // two seconds timeout
                1,         // return just one instance.
                &pInst,    // pointer to instance.
                &ulCount); // Number of instances returned.

            if (ulCount > 0) {

                bFound = TRUE;
                break;
            }
        }

        if (bFound == FALSE && pInst) {
            pInst->Release();
            pInst = NULL;
        }
    }

    // Done with the instance enumerator.
    if (pEnumInst) {
        pEnumInst->Release();
    }

    return pInst;
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
    CComBSTR bstrPropertyName = AnsiToBstr(PROPERTY_NAME);

    // Get the property value.
    CComVariant varPropVal;
    CIMTYPE  cimType = 0;
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

    *Color = varPropVal.uintVal;
    return true;
}

bool CLuminous::Set(COLORREF Color) {
    CComBSTR bstrPropertyName = AnsiToBstr(PROPERTY_NAME);

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
        _tprintf( TEXT("Error %lX: Failed to read property value of %s.\n"),
                            hResult, PROPERTY_NAME);
        return false;
    }

    if((varPropVal.vt == VT_I4) || (varPropVal.vt == VT_UI4)) {
        varPropVal.uintVal = Color;

        // Set the property value
        hResult = m_pIWbemClassObject->Put(
                                    bstrPropertyName,
                                    0,
                                    &varPropVal,
                                    cimType
                                    );

        if ( hResult == WBEM_S_NO_ERROR ) {
            hResult = m_pIWbemServices->PutInstance(
                                            m_pIWbemClassObject,
                                            WBEM_FLAG_UPDATE_ONLY,
                                            NULL,
                                            NULL );

            if ( hResult != WBEM_S_NO_ERROR ) {
                _tprintf( TEXT("Failed to save the instance,")
                                    TEXT(" %s will not be updated.\n"),
                                    PROPERTY_NAME);
            } else {
                return true;
            }
        }
        else {
            _tprintf( TEXT("Error %lX: Failed to set property value of %s.\n"),
            hResult, PROPERTY_NAME);
        }
    }

    return false;
}
