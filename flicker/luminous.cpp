#include <cassert>
#include <stdexcept>
#include "luminous.hpp"

#pragma comment(lib, "wbemuuid.lib")


const wchar_t CLASS_NAME[] = L"TailLightDeviceInformation";
const wchar_t PROPERTY_NAME[] = L"TailLight";


// The function connects to the namespace specified by the user.
// DOC: https://learn.microsoft.com/en-us/windows/win32/wmisdk/example-creating-a-wmi-application
CComPtr<IWbemServices> ConnectToNamespace(_In_ const wchar_t* chNamespace) {
    CComPtr<IWbemLocator> wbemLocator;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&wbemLocator);
    if (hr != S_OK) {
        wprintf(L"Error %lX: Could not create instance of IWbemLocator interface.\n", hr);
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
        &wbemServices);
    if (hr != WBEM_S_NO_ERROR) {
        wprintf(L"Error %lX: Failed to connect to namespace %s.\n", hr, chNamespace);
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
        wprintf(L"Error %lX: Failed to impersonate.\n", hr);
        return nullptr;
    }

    return wbemServices;
}

// The function returns an interface pointer to the instance given its list-index.
CComPtr<IWbemClassObject> GetFirstInstance(IWbemServices& pIWbemServices, const wchar_t* lpClassName) {
    CComPtr<IEnumWbemClassObject> enumInst;
    HRESULT hr = pIWbemServices.CreateInstanceEnum(
        CComBSTR(lpClassName),  // Name of the root class.
        WBEM_FLAG_SHALLOW |     // Enumerate at current root only.
        WBEM_FLAG_FORWARD_ONLY, // Forward-only enumeration.
        NULL,                   // Context.
        &enumInst);          // pointer to class enumerator
    if (hr != WBEM_S_NO_ERROR || enumInst == NULL) {
        wprintf(L"Error %lX: Failed to get a reference to instance enumerator.\n", hr);
        return nullptr;
    }

    // Get pointer to the instance.
    hr = WBEM_S_NO_ERROR;
    while (hr == WBEM_S_NO_ERROR) {
        ULONG count = 0;
        CComPtr<IWbemClassObject> inst;
        hr = enumInst->Next(
            1000,      // one second timeout
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
    if (FAILED (hr)) {
        wprintf(L"Error %lx: Failed to initialize COM library\n", hr);
        throw std::runtime_error("CoInitialize failure");

    }

    m_wbem = ConnectToNamespace(L"root\\WMI"); // namespace for hardware drivers (https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wdm-provider));
    if (!m_wbem) {
        wprintf(L"Could not connect to namespace.\n");
        throw std::runtime_error("ConnectToNamespace failure");
    }

    m_instance = GetFirstInstance(*m_wbem, CLASS_NAME);
     if (!m_instance) {
        wprintf(L"Could not find the instance.\n");
        throw std::runtime_error("GetInstanceReference failure");
    }
}

Luminous::~Luminous() {
    m_instance.Release();
    m_wbem.Release();

    CoUninitialize();
}


bool Luminous::Get(COLORREF* Color) {
    if (!Color)
        return false;

    // Get the property value.
    CComVariant varPropVal;
    CIMTYPE  type = 0;
    HRESULT hr = m_instance->Get(CComBSTR(PROPERTY_NAME), 0, &varPropVal, &type, NULL);
    if (hr != WBEM_S_NO_ERROR) {
        wprintf(L"ERROR %lX: Failed to read %s property value.\n", hr, PROPERTY_NAME);
        return false;
    } 
    assert(type == CIM_UINT32);

    *Color = varPropVal.uintVal;
    return true;
}

bool Luminous::Set(COLORREF Color) {
    // Get the property value.
    CComVariant  varPropVal;
    CIMTYPE     type = 0;
    HRESULT hr = m_instance->Get(CComBSTR(PROPERTY_NAME), 0, &varPropVal, &type, NULL);
    if (hr != WBEM_S_NO_ERROR ) {
        wprintf(L"ERROR %lX: Failed to read %s property value.\n", hr, PROPERTY_NAME);
        return false;
    }
    assert(type == CIM_UINT32);

    varPropVal.uintVal = Color;

    // Set the property value
    hr = m_instance->Put(CComBSTR(PROPERTY_NAME), 0, &varPropVal, type);
    if (hr != WBEM_S_NO_ERROR) {
        wprintf(L"ERROR: %lX: Failed to set %s property value.\n", hr, PROPERTY_NAME);
        return false;
    }

    hr = m_wbem->PutInstance(m_instance, WBEM_FLAG_UPDATE_ONLY, NULL, NULL);
    if (hr != WBEM_S_NO_ERROR) {
        wprintf(L"ERROR: Failed to save the instance, %s will not be updated.\n", PROPERTY_NAME);
        return false;
    }
    
    return true;
}
