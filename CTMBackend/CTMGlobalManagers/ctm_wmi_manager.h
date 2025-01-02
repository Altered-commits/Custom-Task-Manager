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
#include "../ctm_logger.h"

using Microsoft::WRL::ComPtr;

class CTMWMIManager
{
public:
    static CTMWMIManager& GetInstance()
    {
        static CTMWMIManager wmiManager;
        return wmiManager;
    }

    //--------------------GETTER FUNCTION FOR WMI SERVICES--------------------
    IWbemServices* GetServices();

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
    const char* wmiCleanupFunctionName = "CTMWMIManager::CleanupCOM";
    //Just to make sure we are not cleaning up twice
    bool hasCleanedUpResources = false;

private: //WMI variables
    ComPtr<IWbemLocator>  pLocator;
    ComPtr<IWbemServices> pServices;
};

#endif