// dllmain.cpp - COM server entry points + self-registration for the
// self-contained Booster APO. regsvr32 BoosterAPO.dll registers the COM object
// AND the APO so Windows will load it into the audio engine.
#include <initguid.h>
#include <windows.h>
#include <unknwn.h>
#include <strsafe.h>
#include <olectl.h>
#include "apo/SonaraAPO.h"
#include "apo/ClassFactory.h"

LONG g_cDllRef = 0;
static HINSTANCE g_hInst = nullptr;

extern "C" BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInst = inst;
        DisableThreadLibraryCalls(inst);
    }
    return TRUE;
}

extern "C" HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (rclsid != CLSID_SonaraAPO) return CLASS_E_CLASSNOTAVAILABLE;
    CSonaraClassFactory* cf = new (std::nothrow) CSonaraClassFactory();
    if (!cf) return E_OUTOFMEMORY;
    const HRESULT hr = cf->QueryInterface(riid, ppv);
    cf->Release();
    return hr;
}

extern "C" HRESULT STDAPICALLTYPE DllCanUnloadNow() {
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}

// ---- Registry helpers ----
static LONG setKeyStr(HKEY root, const wchar_t* sub, const wchar_t* name, const wchar_t* val) {
    HKEY k; LONG r = RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr);
    if (r != ERROR_SUCCESS) return r;
    r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val,
                       (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r;
}

static const wchar_t* kClsidStr = L"{538B6BB6-27D6-4D50-A09D-6E1883A66888}";

extern "C" HRESULT STDAPICALLTYPE DllRegisterServer() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(g_hInst, path, MAX_PATH)) return HRESULT_FROM_WIN32(GetLastError());

    wchar_t sub[256];
    // 1) COM server registration.
    StringCchPrintfW(sub, 256, L"CLSID\\%s", kClsidStr);
    if (setKeyStr(HKEY_CLASSES_ROOT, sub, nullptr, L"Sonara Engine") != ERROR_SUCCESS)
        return SELFREG_E_CLASS;
    StringCchPrintfW(sub, 256, L"CLSID\\%s\\InprocServer32", kClsidStr);
    setKeyStr(HKEY_CLASSES_ROOT, sub, nullptr, path);
    setKeyStr(HKEY_CLASSES_ROOT, sub, L"ThreadingModel", L"Both");

    // 2) APO registration so the audio engine can enumerate the effect.
    //    Correct path: HKLM\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\{CLSID}
    StringCchPrintfW(sub, 256,
        L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\%s", kClsidStr);
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"FriendlyName", L"Sonara Engine");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"Copyright", L"Sonara");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MajorVersion", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MinorVersion", L"0");
    // APO flags (5 = stream effect, software effect)
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"Flags", L"5");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MinInputConnections", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MaxInputConnections", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MinOutputConnections", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MaxOutputConnections", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MaxInstances", L"4294967295");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"NumAPOInterfaces", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"APOInterface0", L"{F141E1E5-9EE0-4E18-AD93-C10A7D498F37}"); // IAudioProcessingObject
    return S_OK;
}

extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer() {
    wchar_t sub[256];
    StringCchPrintfW(sub, 256, L"CLSID\\%s", kClsidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, sub);
    StringCchPrintfW(sub, 256,
        L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\%s", kClsidStr);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, sub);
    return S_OK;
}
