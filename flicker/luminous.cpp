#include "luminous.h"
#include <stdexcept>


// The function converts an ANSI string into BSTR and returns it in an
// allocated memory. The memory must be freed by the caller using free()
// function. If nLenSrc is -1, the string is null terminated.
BSTR AnsiToBstr(_In_ WCHAR* lpSrc, _In_ int nLenSrc)
{
    if (lpSrc == NULL) {
        nLenSrc = 0;
    }
    else {
        if (nLenSrc == -1) {
            size_t temp = wcslen(lpSrc);
            if (temp > INT_MAX - 1) {
                return NULL;
            }
            nLenSrc = (int)temp + 1;
        }
    }

    return SysAllocStringLen(lpSrc, nLenSrc);
}

// The function connects to the namespace specified by the user.
IWbemServices* ConnectToNamespace(_In_ LPTSTR chNamespace)
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
    BSTR bstrNamespace = AnsiToBstr(chNamespace, -1);

    if (!bstrNamespace) {
        _tprintf(TEXT("Out of memory.\n"));
        pIWbemLocator->Release();
        return NULL;
    }

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

    // Done with Namespace.
    SysFreeString(bstrNamespace);

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
    _In_ LPTSTR lpClassName)
{
    IWbemClassObject* pInst = NULL;
    IEnumWbemClassObject* pEnumInst;
    BSTR                 bstrClassName;
    BOOL                 bFound;
    ULONG                ulCount;
    HRESULT              hResult;


    bstrClassName = AnsiToBstr(lpClassName, -1);

    if (!bstrClassName) {
        _tprintf(TEXT("Out of memory.\n"));
        return NULL;
    }

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
    SysFreeString(bstrClassName);

    return pInst;
}


CLuminous::CLuminous() {
    // Initialize COM library. Must be done before invoking any
    // other COM function.
    HRESULT hResult = CoInitialize( NULL );

    if ( FAILED (hResult)) {
        _tprintf( TEXT("Error %lx: Failed to initialize COM library\n"), hResult );
        throw std::runtime_error("CoInitialize failure");

    }

    m_pIWbemServices = ConnectToNamespace( NAME_SPACE);
    if (!m_pIWbemServices ) {
        _tprintf( TEXT("Could not connect name.\n") );
        throw std::runtime_error("ConnectToNamespace failure");
    }

     m_pIWbemClassObject = GetInstanceReference( m_pIWbemServices, CLASS_NAME);
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
    VARIANT     varPropVal;
    VariantInit( &varPropVal );

    BSTR bstrPropertyName = AnsiToBstr( PROPERTY_NAME, -1 );

    if ( !bstrPropertyName ) {
        _tprintf( TEXT("Error out of memory.\n") );
        VariantClear( &varPropVal );
        return false;
    }

    // Get the property value.
    bool     bRet= false;
    CIMTYPE  cimType;
    HRESULT hResult = m_pIWbemClassObject->Get(
                             bstrPropertyName,
                             0,
                             &varPropVal,
                             &cimType,
                             NULL );

    if ( hResult != WBEM_S_NO_ERROR ) {
        _tprintf( TEXT("Error %lX: Failed to read property value of %s.\n"),
                        hResult, PROPERTY_NAME);
    } else {
        if((varPropVal.vt == VT_I4) || (varPropVal.vt == VT_UI4)) {
            *Color = varPropVal.uintVal;
            bRet = true;
        }
    }

    if(bstrPropertyName) {
        SysFreeString( bstrPropertyName );
    }

    VariantClear( &varPropVal );

    return bRet;
}

bool CLuminous::Set(COLORREF Color) {
    bool bRet = false;

    VARIANT     varPropVal;
    VariantInit( &varPropVal );

    LPTSTR      lpProperty = PROPERTY_NAME;
    BSTR bstrPropertyName = AnsiToBstr( lpProperty, -1 );

    if ( !bstrPropertyName ) {
        _tprintf( TEXT("Error out of memory.\n") );
        goto End ;
    }

    // Get the property value.
    CIMTYPE     cimType;
    HRESULT hResult = m_pIWbemClassObject->Get(
                             bstrPropertyName,
                             0,
                             &varPropVal,
                             &cimType,
                             NULL );

    if ( hResult != WBEM_S_NO_ERROR ) {
        _tprintf( TEXT("Error %lX: Failed to read property value of %s.\n"),
                            hResult, lpProperty );
        goto End;
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
                                    lpProperty );
            } else {
                bRet = true;
            }
        }
        else {
            _tprintf( TEXT("Error %lX: Failed to set property value of %s.\n"),
            hResult, lpProperty );
        }
    }

End:
    if(bstrPropertyName) {
        SysFreeString( bstrPropertyName );
    }

    VariantClear( &varPropVal );

    return bRet;
}
