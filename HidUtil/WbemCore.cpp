#include "Wbem.h"
#include "roapi.h"
#include <stdio.h>

#pragma comment(lib, "comsupp")
#pragma comment(lib, "wbemuuid")

// Blatantly stolen from the Toaster sample

HRESULT ConnectToWbem() {
    HRESULT status = S_OK;
    BOOLEAN initialized = FALSE;

    BSTR temp = NULL;
    BSTR wmiRoot = NULL;
    BSTR userIdString = NULL;
    BSTR passwordString = NULL;

    PWSTR ComputerName = NULL;
    PWSTR userId = NULL;
    PWSTR password = NULL;
    PWSTR domain = NULL;

    winrt::com_ptr<IWbemLocator> wbemLocator;
    winrt::com_ptr<IWbemServices> wbemServices;

    //
    // Initialize COM environment for multi-threaded concurrency.
    //
    status = Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
    if (FAILED(status)) {
        return status;
    }

    initialized = TRUE;

    //
    // Initialize the security layer and set the specified values as the
    // security default for the process.
    //
    status = CoInitializeSecurity(NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_PKT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        0);

    if (FAILED(status)) {
        goto exit;
    }

    //
    // Create a single uninitialized object associated with the class id
    // CLSID_WbemLocator.
    //
    status = CoCreateInstance(CLSID_WbemLocator,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        wbemLocator.put_void());

    if (FAILED(status)) {
        goto exit;
    }

    //
    // Construct the object path for the WMI namespace. For local access to the
    // WMI namespace, use a simple object path: "\\.\root\WMI". For access to
    // the WMI namespace on a remote computer, include the computer name in the
    // object path: "\\myserver\root\WMI".
    //
    if (ComputerName != NULL) {

        status = VarBstrCat(_bstr_t(L"\\\\"), _bstr_t(ComputerName), &temp);
        if (FAILED(status)) {
            goto exit;
        }

    }
    else {

        status = VarBstrCat(_bstr_t(L"\\\\"), _bstr_t(L"."), &temp);
        if (FAILED(status)) {
            goto exit;
        }
    }

    status = VarBstrCat(temp, _bstr_t(L"\\root\\WMI"), &wmiRoot);
    if (FAILED(status)) {
        goto exit;
    }

    SysFreeString(temp);
    temp = NULL;

    //
    // Construct the user id and password strings.
    //
    if (userId != NULL) {

        if (domain != NULL) {

            status = VarBstrCat(_bstr_t(domain), _bstr_t(L"\\"), &temp);
            if (FAILED(status)) {
                goto exit;
            }

            status = VarBstrCat(temp, _bstr_t(userId), &userIdString);
            if (FAILED(status)) {
                goto exit;
            }

            SysFreeString(temp);
            temp = NULL;

        }
        else {

            userIdString = SysAllocString(userId);
            if (userIdString == NULL) {
                status = E_OUTOFMEMORY;
                goto exit;
            }
        }

        passwordString = SysAllocString(password);
        if (passwordString == NULL) {
            status = E_OUTOFMEMORY;
            goto exit;
        }
    }

    //
    // Connect to the WMI server on this computer and, possibly, through it to another system.
    //
    status = wbemLocator->ConnectServer(wmiRoot,
                                        userIdString,
                                        passwordString,
                                        NULL,
                                        0,
                                        NULL,
                                        NULL,
                                        wbemServices.put());
    if (FAILED(status)) {
        if (status != WBEM_E_LOCAL_CREDENTIALS) {
            goto exit;
        }

        //
        // Use the identity inherited from the current process.
        //
        status = wbemLocator->ConnectServer(wmiRoot,
            NULL,
            NULL,
            NULL,
            0,
            NULL,
            NULL,
            wbemServices.put());
        if (FAILED(status)) {
            goto exit;
        }
    }

    //
    // Set authentication information for the interface.
    //
    status = SetInterfaceSecurity(wbemServices.get(), userId, password, domain);
    if (FAILED(status)) {
        goto exit;
    }

    //
    // Execute the methods in each instance of the desired class.
    //
    printf("\n1. Execute Methods in class ...\n");
    status = ExecuteBISTOnAllDevices(&wbemServices,
                                     userId,
                                     password,
                                     domain);
    if (FAILED(status)) {
        goto exit;
    }

exit:

    if (temp != NULL) {
        SysFreeString(temp);
    }

    if (userIdString != NULL) {
        SysFreeString(userIdString);
    }

    if (passwordString != NULL) {
        SysFreeString(passwordString);
    }

    if (wmiRoot != NULL) {
        SysFreeString(wmiRoot);
    }

    if (initialized == TRUE) {
        CoUninitialize();
    }

    if (FAILED(status)) {
        printf("FAILED with Status = 0x%08x\n", status);
    }
    return status;
}

HRESULT
SetInterfaceSecurity(
    _In_     IUnknown* InterfaceObj,
    _In_opt_ PWSTR UserId,
    _In_opt_ PWSTR Password,
    _In_opt_ PWSTR DomainName
)

/*++

Routine Description:

    Set the interface security to allow the server to impersonate the specified
    user.

Arguments:

    InterfaceObj - Pointer to interface for which the security settings need
        to be applied.

    UserId - Pointer to the user id information or NULL.

    Password - Pointer to password or NULL. If the user id is not specified,
        this parameter is ignored.

    DomainName - Pointer to domain name or NULL. If the user id is not specified,
        this parameter is ignored.

Return Value:

    HRESULT Status code.

--*/

{
    HRESULT    hr;

    COAUTHIDENTITY AuthIdentity;
    DWORD AuthnSvc;
    DWORD AuthzSvc;
    DWORD AuthnLevel;
    DWORD ImpLevel;
    DWORD Capabilities;
    PWSTR pServerPrinName = NULL;
    RPC_AUTH_IDENTITY_HANDLE   pAuthHndl = NULL;

    //
    // Get current authentication information for interface.
    //
    hr = CoQueryProxyBlanket(InterfaceObj,
        &AuthnSvc,
        &AuthzSvc,
        &pServerPrinName,
        &AuthnLevel,
        &ImpLevel,
        &pAuthHndl,
        &Capabilities);

    if (FAILED(hr)) {
        goto exit;
    }

    if (UserId == NULL) {

        AuthIdentity.User = NULL;
        AuthIdentity.UserLength = 0;
        AuthIdentity.Password = NULL;
        AuthIdentity.PasswordLength = 0;
        AuthIdentity.Domain = NULL;
        AuthIdentity.DomainLength = 0;

    }
    else {

        AuthIdentity.User = (USHORT*)UserId;
#pragma prefast(suppress:6387, "0 length UserId is valid")        
        AuthIdentity.UserLength = (ULONG)wcslen(UserId);
        AuthIdentity.Password = (USHORT*)Password;
#pragma prefast(suppress:6387, "0 length Password is valid")               
        AuthIdentity.PasswordLength = (ULONG)wcslen(Password);
        AuthIdentity.Domain = (USHORT*)DomainName;
        AuthIdentity.DomainLength = (DomainName == NULL) ? 0 : (ULONG)wcslen(DomainName);

    }

    AuthIdentity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

    //
    // Change authentication information for interface, providing the identity
    // information and "echoing back" everything else.
    //
    hr = CoSetProxyBlanket(InterfaceObj,
        AuthnSvc,
        AuthzSvc,
        pServerPrinName,
        AuthnLevel,
        ImpLevel,
        &AuthIdentity,
        Capabilities);

    if (FAILED(hr)) {
        goto exit;
    }

exit:
    return hr;
}