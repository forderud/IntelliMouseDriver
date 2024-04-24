#include "wbem.h"
#include <stdio.h>

#define TAILLIGHT_WMI_BIST_CLASS    L"TailLightBIST"
#define TAILLIGHT_WMI_BIST_METHOD   L"BIST"

// Blatantly stolen from the Toaster samples.
// Just like most other driver code.
// From WmiExecute.cpp
// Updated to use WinRT and RAII

using namespace winrt;

HRESULT
ExecuteMethod_NoArgs_ReturnsValue(
    _In_ com_ptr<IWbemServices>* pWbemServices,
    _In_ com_ptr<IWbemClassObject>* pClassObj,
    _In_ const BSTR InstancePath,
    _In_ const OLECHAR* psz,
    _Out_ HRESULT& wmiMethodRet)
{
    HRESULT status;

    com_ptr<IWbemClassObject> inputParamsObj;
    com_ptr<IWbemCallResult>  resultControlObj;

    const BSTR methodName = SysAllocString(psz);
    VARIANT retVal;

    //
    // Get the input parameters class objects for the method.
    //
    status = (*pClassObj)->GetMethod(methodName, 0, inputParamsObj.put(), NULL);
    if (FAILED(status)) {
        goto exit;
    }

    //
    // Set the output variables values (i.e., return value for BIST).
    //
    retVal.vt = VT_I4;
    retVal.ulVal = wmiMethodRet;

    //
    // Call the method.
    //
    printf("\n");
    printf("  Start waiting for method : %ws to finish.\n", (wchar_t*)methodName);
    status = (*pWbemServices)->ExecMethod(InstancePath,
        methodName,
        0,
        NULL,
        NULL,
        NULL,
        resultControlObj.put());

    if (FAILED(status) || NULL == resultControlObj) {
        goto exit;
    }

    //
    // Get the result of the method call.
    //
    status = resultControlObj->GetCallStatus(4 * 1000, &wmiMethodRet);
    printf("     Return value...: 0x%x\n", wmiMethodRet);
exit:

    if (methodName != NULL) {
        SysFreeString(methodName);
    }

    return status;
}


HRESULT
ExecuteBISTOnAllDevices(
    _In_     com_ptr<IWbemServices>* pWbemServices,
    _In_opt_ PWSTR UserId,
    _In_opt_ PWSTR Password,
    _In_opt_ PWSTR DomainName
)
{
    HRESULT status = S_OK;

    com_ptr<IEnumWbemClassObject> enumerator;
    com_ptr<IWbemClassObject> classObj;
    com_ptr<IWbemClassObject> instanceObj;

    const BSTR className = SysAllocString(TAILLIGHT_WMI_BIST_CLASS);

    VARIANT pathVariable;
    _bstr_t instancePath;
    ULONG   nbrObjsSought = 1;
    ULONG   nbrObjsReturned;

    VariantInit(&pathVariable);

    //
    // Create an Enumeration object to enumerate the instances of the given class.
    //
    status = (*pWbemServices)->CreateInstanceEnum(className,
        WBEM_FLAG_SHALLOW | WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY,
        NULL,
        enumerator.put());
    if (FAILED(status)) {
        goto exit;
    }

    //
    // Set authentication information for the interface.
    //
    status = SetInterfaceSecurity(enumerator.get(), UserId, Password, DomainName);
    if (FAILED(status)) {
        goto exit;
    }

    //
    // Get the class object for the method definition.
    //
    status = (*pWbemServices)->GetObject(className, 0, NULL, classObj.put(), NULL);
    if (FAILED(status) || NULL == classObj) {
        goto exit;
    }

    do {

        //
        // Get the instance object for each instance of the class.
        //
        status = enumerator->Next(WBEM_INFINITE,
            nbrObjsSought,
            instanceObj.put(),
            &nbrObjsReturned);

        if (status == WBEM_S_FALSE) {
            status = S_OK;
            break;
        }

        if (FAILED(status)) {
            if (status == WBEM_E_INVALID_CLASS) {
                printf("ERROR: TailLight driver may not be active on the system.\n");
            }
            goto exit;
        }

        //
        // To obtain the object path of the object for which the method has to be
        // executed, query the "__PATH" property of the WMI instance object.
        //


        status = instanceObj->Get(_bstr_t(L"__PATH"), 0, &pathVariable, NULL, NULL);
        if (FAILED(status)) {
            goto exit;
        }

        instancePath = pathVariable.bstrVal;
        printf("Instance Path .: %ws\n", (wchar_t*)instancePath);

        HRESULT hr = E_FAIL;

        //
        // Execute the methods in this instance of the class.
        //
        status = ExecuteMethod_NoArgs_ReturnsValue(pWbemServices, 
                                                   &classObj, 
                                                   instancePath, 
                                                   TAILLIGHT_WMI_BIST_METHOD,
                                                   hr);
        if (FAILED(status)) {
            goto exit;
        }

    } while (!FAILED(status));

exit:

    if (className != NULL) {
        SysFreeString(className);
    }

    return status;
}