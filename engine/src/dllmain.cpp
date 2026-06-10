// dllmain.cpp - COM server entry points + self-registration for the
// self-contained Booster APO. regsvr32 BoosterAPO.dll registers the COM object
// AND the APO so Windows will load it into the audio engine.
#include <windows.h>
#include <unknwn.h>
#include <strsafe.h>
#include "apo/BoosterAPO.h"
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
    if (rclsid != CLSID_BoosterAPO) return CLASS_E_CLASSNOTAVAILABLE;
    CBoosterClassFactory* cf = new (std::nothrow) CBoosterClassFactory();
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

static const wchar_t* kClsidStr = L"{A1B2C3D4-E5F6-47A8-9B0C-1D2E3F4A5B6C}";

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
    //    HKLM\\...\\Audio\\AudioProcessingObjects\\{CLSID}
    StringCchPrintfW(sub, 256,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\"
        L"AudioProcessingObjects\\%s", kClsidStr);
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"FriendlyName", L"Sonara Engine");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"Copyright", L"Sonara");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MajorVersion", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MinorVersion", L"0");
    // APO flags (1 = APO_FLAG_SAMPLESPERFRAME_MUST_MATCH ... see WDK). Marked as
    // a software effect that can run in the global stream.
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"Flags", L"5");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MinInputConnections", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MaxInputConnections", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MinOutputConnections", L"1");
    setKeyStr(HKEY_LOCAL_MACHINE, sub, L"MaxOutputConnections", L"1");
    // NOTE: attaching the APO to a specific endpoint's FX property store
    // (PKEY_FX_*) is done per-device by scripts/install.ps1 because it depends
    // on which output device the user selects.
    return S_OK;
}

extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer() {
    wchar_t sub[256];
    StringCchPrintfW(sub, 256, L"CLSID\\%s", kClsidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, sub);
    StringCchPrintfW(sub, 256,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\"
        L"AudioProcessingObjects\\%s", kClsidStr);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, sub);
    return S_OK;
}
