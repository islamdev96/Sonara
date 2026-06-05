#include <windows.h>
#include "BoosterAPO.h"

// Standard COM DLL entry points
extern "C" BOOL WINAPI DllMain(HINSTANCE const instance, DWORD const reason, LPVOID const reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// Used by COM to create instances of our Booster APO
extern "C" HRESULT STDAPICALLTYPE DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
    // Stub: In a real implementation, this would return an IClassFactory that creates CBoosterAPO.
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    
    if (rclsid == CLSID_BoosterAPO)
    {
        // Return a basic class factory
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

// Used by COM to determine if the DLL can be safely unloaded
extern "C" HRESULT STDAPICALLTYPE DllCanUnloadNow(void)
{
    // Stub: Return S_OK if no objects are active.
    return S_OK;
}

// Used by regsvr32 to register the DLL
extern "C" HRESULT STDAPICALLTYPE DllRegisterServer(void)
{
    return CBoosterAPO::Register();
}

// Used by regsvr32 to unregister the DLL
extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer(void)
{
    return CBoosterAPO::Unregister();
}
