#ifndef CTM_WMI_MANAGER_HPP
#define CTM_WMI_MANAGER_HPP

//Windows stuff
#include <windows.h>
#include <comutil.h>
#include <comdef.h>
#include <WbemCli.h>
#include <wrl/client.h>
//My stuff
#include "ctm_critical_resource_guard.h"
#include "../CTMPureHeaderFiles/ctm_logger.h"
//Stdlib stuff
#include <cstring>

//Basically unique_ptr but for COM objects
using Microsoft::WRL::ComPtr;

//Simple VARIANT wrapper with RAII destruction as <atlcomcli.h> is not supported on MinGW
class CTMVariant : public tagVARIANT
{
public:
    CTMVariant()  { VariantInit(this); }
    ~CTMVariant() { VariantClear(this); }
};

class CTMWMIManager
{
public:
    static CTMWMIManager& GetInstance()
    {
        static CTMWMIManager wmiManager;
        return wmiManager;
    }

    //--------------------GETTER FUNCTION FOR WMI--------------------
    IWbemServices*        GetServices();
    IEnumWbemClassObject* GetEnumeratorFromQuery(PCWSTR);

private: //Constructors and Destructors
    CTMWMIManager();
    ~CTMWMIManager();

    //No need for copy or move operations
    CTMWMIManager(const CTMWMIManager&)            = delete;
    CTMWMIManager& operator=(const CTMWMIManager&) = delete;
    CTMWMIManager(CTMWMIManager&&)                 = delete;
    CTMWMIManager& operator=(CTMWMIManager&&)      = delete;

private: //Functions to initialize WMI and COM
    bool InitializeCOM();
    bool InitializeWMI();

private: //Resource guard
    CTMCriticalResourceGuard& resourceGuard = CTMCriticalResourceGuard::GetInstance();
    //Just a unique name for cleanup function
    const char* wmiCleanupFunctionName = "CTMWMIManager::CleanupCOMAndWMI";
    //Just to make sure we are not cleaning up twice
    bool hasCleanedUpResources = false;

private: //WMI variables
    ComPtr<IWbemLocator>  pLocator;
    ComPtr<IWbemServices> pServices;
};

#endif