#include "ctm_wmi_manager.h"

CTMWMIManager::CTMWMIManager()
{
    if(!InitializeCOM())
        return;
    
    if(!InitializeWMI())
        return;
    
    //Setup resource guard for uninitializing COM and WMI on unexpected shutdown of process
    resourceGuard.RegisterCleanupFunction(wmiCleanupFunctionName, [this](){
        //When stuff ain't RAII destructed, release ComPtrs manually
        pLocator.Reset();
        pServices.Reset();
        //Rest is just normal uninitialization
        CoUninitialize();
        hasCleanedUpResources = true;
    });
}

CTMWMIManager::~CTMWMIManager()
{
    //In cases where resource guard might get triggered but static classes like this one can still be destructed...
    //I'm just paranoid
    if(!hasCleanedUpResources)
    {
        //Heres the thing, normally ComPtr should destruct themselves. But when i don't do this manually, i get 0xC0000005 violation.
        //In short, i get access violation, if you (yes you the reader of this) figured this out, pls let me know
        pLocator.Reset();
        pServices.Reset();
        CoUninitialize();
        hasCleanedUpResources = true;
        
        //No need for resource guard anymore
        resourceGuard.UnregisterCleanupFunction(wmiCleanupFunctionName);
    }
    else
        CTM_LOG_WARNING("Resources were already cleaned up by the resource guard, not doing it again.");
}

//--------------------GETTER FUNCTION FOR WMI--------------------
IWbemServices* CTMWMIManager::GetServices()
{
    return pServices.Get();
}

IEnumWbemClassObject* CTMWMIManager::GetEnumeratorFromQuery(PCWSTR query)
{
    //Important variable :)
    IEnumWbemClassObject* pEnumerator = nullptr;

    //Get the WMI services which can be used to query stuff
    auto pServices = GetServices();
    //Check if its nullptr or not, if it is nullptr, then wmi wasnt initialized properly
    if(!pServices)
    {
        CTM_LOG_ERROR("WMI Services failed to initialize.");
        return nullptr;
    }

    //It isn't nullptr, query whatever u need
    HRESULT hres = pServices->ExecQuery(
            bstr_t{L"WQL"},                                        //Query language type. WQL is acronym for WMI Query Language
            bstr_t{query},
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, //Check: https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemservices-execquery#parameters
            NULL,                                                  //Context can be NULL
            &pEnumerator                                           //Pointer to data in short, just need to enumerate thru the data
        );

    //Just in case, cuz i am not using resource guard in this specific... so i can't really afford access violations
    if(FAILED(hres) || pEnumerator == nullptr)
    {
        CTM_LOG_ERROR("Failed to query WMI for processor info or Enumerator returned was nullptr.");
        return nullptr;
    }

    //Everything succeeded, return enumerator
    return pEnumerator;
}

//--------------------HELPER FUNCTIONS FOR BSTR CONVERSION--------------------
bool CTMWMIManager::WSToSWithEllipsisTruncation(PSTR dest, BSTR src, int destSize)
{
    //Either destination or source is nullptr, or destination size is < 4 (no space even for ellipsis truncation '...\0')
    if(!dest || !src || destSize < 4)
        return false;
    
    //Get the length of source string without null terminator
    std::size_t srcSize      = std::wcslen(src);
    bool        needsTrunc   = srcSize >= destSize;
    int         trueDestSize = needsTrunc ? destSize - 4 : destSize;
    
    //Convert wide char (in our case is BSTR) to 8 bit char
    int res = WideCharToMultiByte(
                        CP_UTF8,      //UTF8 encoding is used
                        0,            //NOTE: For the code page 65001 (UTF-8), dwFlags must be set to either 0 or WC_ERR_INVALID_CHARS
                        src,          //Source string
                        -1,           //Source string is null terminated so just use whatever the size of the source is
                        dest,         //Destination string
                        trueDestSize, //Destination buffer size after conditional checks
                        //For CP_UTF8, these parameters need to be NULL
                        NULL,
                        NULL
                    );
    
    //Conversion failed
    if(res == 0)
        return false;

    //Check if ellipsis is needed
    //If it does, then add ellipsis (...) at the end along with null terminator ('.', '.', '.' and '\0')
    if(needsTrunc)
        std::memcpy(dest + trueDestSize, "...", 4);

    return true;
}

//--------------------INITIALIZE WMI AND COM--------------------
bool CTMWMIManager::InitializeCOM()
{
    HRESULT hres;
    //We use apartment-threaded model cuz our GUI is in the same thread as the NON-GUI stuff
    //Read the 'Note' from this link of MSDN
    //https://learn.microsoft.com/en-us/windows/win32/api/objbase/ne-objbase-coinit#remarks
    hres = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if(FAILED(hres))
    {
        CTM_LOG_ERROR("Failed to initialize COM library.");
        return false;
    }

    //Then we initialize security stuff for COM
    hres = CoInitializeSecurity(
            NULL,                        //Default security settings
            -1,                          //Default authetication service
            NULL,                        //Above argument is -1, hence this should be NULL
            NULL,                        //Reserved, must be NULL
            RPC_C_AUTHN_LEVEL_DEFAULT,   //Just default authentication level is enough
            RPC_C_IMP_LEVEL_IMPERSONATE, //Read: https://learn.microsoft.com/en-us/windows/win32/com/com-impersonation-level-constants
            NULL,                        //Used by COM only when client calls CoInitializeSecurity
            EOAC_NONE,                   //No additional capabilities
            NULL                         //Reserved, must be NULL
        );
    
    if(FAILED(hres))
    {
        CTM_LOG_ERROR("Failed to initialize security for COM.");
        CoUninitialize();
        return false;
    }

    //Basic COM stuff has been setup
    return true;
}

bool CTMWMIManager::InitializeWMI()
{
    HRESULT hres;

    //Create an instance of WMI locator
    hres = CoCreateInstance(
            CLSID_WbemLocator,    //We need WbemLocator to connect to WMI
            NULL,                 //Check this: https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocreateinstance#parameters
            CLSCTX_INPROC_SERVER, //The object will run in the same process as the caller of the function (aka us, we called this function)
            IID_IWbemLocator,     //IID of the interface we want to use (IWbemLocator)
            reinterpret_cast<LPVOID*>(pLocator.GetAddressOf()) //Output: pointer to the interface
        );
    
    if(FAILED(hres))
    {
        CTM_LOG_ERROR("Failed to create an instance of WMI locator.");
        CoUninitialize(); //Uninitialize COM as there is no point in keeping it alive
        return false;
    }

    //Connect to the WMI namespace
    bstr_t wmiNamespace{L"ROOT\\CIMV2"};
    hres = pLocator->ConnectServer(
            wmiNamespace.GetBSTR(),  //Predefined WMI Namespace
            NULL,                    //Username, default access so we can use NULL
            NULL,                    //Password, default access so we can use NULL
            NULL,                    //Use default locale
            0,                       //Use default security flags
            NULL,                    //No special authority required
            NULL,                    //No additional context object is required
            pServices.GetAddressOf() //WMI Services pointer, query all the stuff from WMI thru this pointer
        );
    
    if(FAILED(hres))
    {
        CTM_LOG_ERROR("Failed to connect to WMI namespace.");
        //Release all COM and WMI resources allocated till now
        pLocator.Reset();
        CoUninitialize();
        return false;
    }

    //Set security stuff on WMI connection we just made above
    hres = CoSetProxyBlanket(
            pServices.Get(),        //The WMI connection
            RPC_C_AUTHN_WINNT,      //Use NTLMSSP, which i am not sure what it is
            RPC_C_AUTHZ_NONE,       //When using AUTHN_WINNT, we use AUTHZ_NONE as argument. Check: https://learn.microsoft.com/en-us/windows/win32/com/com-authorization-constants
            NULL,                   //Default principle name
            RPC_C_AUTHN_LEVEL_CALL, //Authenticates only at the beginning of each remote procedure call when the server receives the request
            RPC_C_IMP_LEVEL_IMPERSONATE, //For NTLMSSP, the value must be RPC_C_IMP_LEVEL_IMPERSONATE
            NULL,                   //According to MSDN: If this parameter is NULL, DCOM uses the current proxy identity
            EOAC_NONE               //...
        );

    if(FAILED(hres))
    {
        CTM_LOG_ERROR("Failed to setup proxy blanket on WMI connection.");
        //Release all COM and WMI resources allocated till now
        pLocator.Reset();
        pServices.Reset();
        CoUninitialize();
        return false;
    }

    //All set to go, we can now use WMI as we need
    return true;
}
