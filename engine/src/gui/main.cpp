#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <strsafe.h>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <wrl.h>
#include <wrl/client.h>

#include "WebView2.h"
#include "json.hpp"

#include "../dsp/Parameters.h"
#include "../apo/SharedParams.h"
#include "../apo/SharedStatus.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace Microsoft::WRL;

// System Tray ID & Custom messages
#define ID_TRAY_APP_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1002
#define ID_TRAY_OPEN 1003
#define IDI_APP_ICON 101

namespace wab::gui {

// Duplicate mapping functions
float boostPercentToPreampDb(float percent) {
    float p = std::max(0.0f, std::min(500.0f, percent));
    if (p <= 0.0f) return -80.0f;
    if (p <= 100.0f) return 20.0f * log10f(p / 100.0f);
    float over = (p - 100.0f) / 400.0f;
    return over * 18.0f;
}

float preampDbToBoostPercent(float db) {
    if (db <= -80.0f) return 0.0f;
    if (db <= 0.0f) {
        return 100.0f * powf(10.0f, db / 20.0f);
    }
    float over = db / 18.0f;
    return 100.0f + over * 400.0f;
}

class StatusReader {
public:
    ~StatusReader() { close(); }

    bool open() {
        if (view_) return true;
        wchar_t path[MAX_PATH];
        if (!SharedStatus::resolvePath(path, MAX_PATH)) return false;

        hFile_ = CreateFileW(path, GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) { hFile_ = nullptr; return false; }

        hMap_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, 0,
                                   sizeof(StatusBlock), nullptr);
        if (!hMap_) { close(); return false; }
        view_ = reinterpret_cast<StatusBlock*>(
            MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, sizeof(StatusBlock)));
        if (!view_) { close(); return false; }
        return true;
    }

    bool read(StatusBlock& out) const {
        if (!view_) return false;
        for (int i = 0; i < 4; ++i) {
            const uint32_t s1 = view_->seq; MemoryBarrier();
            if (s1 == 0) {
                YieldProcessor();
                continue;
            }
            out = *view_;                   MemoryBarrier();
            const uint32_t s2 = view_->seq;
            if (s1 == s2 && out.magic == kStatusMagic) return true;
        }
        return out.magic == kStatusMagic;
    }

    void close() {
        if (view_) { UnmapViewOfFile(view_); view_ = nullptr; }
        if (hMap_) { CloseHandle(hMap_); hMap_ = nullptr; }
        if (hFile_) { CloseHandle(hFile_); hFile_ = nullptr; }
    }

private:
    HANDLE hFile_ = nullptr;
    HANDLE hMap_ = nullptr;
    StatusBlock* view_ = nullptr;
};

class MainWindow {
public:
    MainWindow() = default;
    ~MainWindow() {
        removeTrayIcon();
    }

    bool create(HINSTANCE hInst) {
        hInst_ = hInst;

        WNDCLASSEXW wcx = { sizeof(WNDCLASSEXW) };
        wcx.style = CS_HREDRAW | CS_VREDRAW;
        wcx.lpfnWndProc = wndProc;
        wcx.hInstance = hInst;
        wcx.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APP_ICON));
        wcx.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcx.lpszClassName = L"SonaraWebView2Class";

        RegisterClassExW(&wcx);

        // Standard dimensions matching original Electron app window: 980x660
        RECT r = { 0, 0, 980, 660 };
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

        hWnd_ = CreateWindowExW(
            0, L"SonaraWebView2Class", L"Sonara Control Panel",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
            nullptr, nullptr, hInst, this
        );

        if (!hWnd_) return false;

        // Load parameters
        loadParams();

        // Open status channel
        statusReader_.open();

        // Setup background host auto-spawn
        startHost();

        // Setup tray icon
        setupTrayIcon();

        // Initialize WebView2
        initWebView();

        // Start timer for posting levels to WebView2 (50ms)
        SetTimer(hWnd_, 1, 50, nullptr);

        // Start timer to check for engine status changes (3000ms)
        SetTimer(hWnd_, 2, 3000, nullptr);

        return true;
    }

    void show() {
        ShowWindow(hWnd_, SW_SHOW);
        UpdateWindow(hWnd_);
    }

private:
    static LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        MainWindow* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->handleMessage(hWnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_SIZE:
            if (webviewController_) {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                webviewController_->put_Bounds(bounds);
            }
            return 0;

        case WM_TIMER:
            if (wParam == 1) {
                postEngineLevels();
            } else if (wParam == 2) {
                checkEngineStatus();
            }
            return 0;

        case WM_TRAYICON:
            if (LOWORD(lParam) == WM_LBUTTONDBLCLK || LOWORD(lParam) == WM_RBUTTONUP) {
                if (LOWORD(lParam) == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Open Control Panel");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit Sonara");
                    
                    SetForegroundWindow(hWnd);
                    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);
                    DestroyMenu(hMenu);
                } else {
                    ShowWindow(hWnd, SW_RESTORE);
                    SetForegroundWindow(hWnd);
                }
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TRAY_EXIT) {
                removeTrayIcon();
                PostQuitMessage(0);
            } else if (LOWORD(wParam) == ID_TRAY_OPEN) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            break;

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE); // Hide to tray on close
            return 0;

        case WM_DESTROY:
            removeTrayIcon();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    void loadParams() {
        wab::SharedParams paramsReader;
        if (paramsReader.open()) {
            wab::dsp::Parameters p;
            if (paramsReader.read(p)) {
                params_ = p;
                boostPercent_ = preampDbToBoostPercent(p.preampDb);
            }
            paramsReader.close();
        }
    }

    bool saveParams() {
        params_.preampDb = boostPercentToPreampDb(boostPercent_);

        static uint32_t seq = 0;
        seq = (seq + 1);
        if (seq == 0) seq = 1;

        wchar_t path[MAX_PATH];
        if (!SharedParams::resolvePath(path, MAX_PATH)) return false;

        HANDLE hFile = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        // Write with seq = 0 (tear protection)
        dsp::Parameters temp = params_;
        temp.seq = 0;

        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, &temp, sizeof(temp), &bytesWritten, nullptr)) {
            CloseHandle(hFile);
            return false;
        }

        // Seek back to seq offset (8 bytes)
        LARGE_INTEGER li;
        li.QuadPart = 8;
        SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN);

        // Write final sequence
        if (!WriteFile(hFile, &seq, sizeof(seq), &bytesWritten, nullptr)) {
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);
        return true;
    }

    void startHost() {
        StatusBlock sb;
        if (statusReader_.read(sb)) {
            if (GetTickCount64() - sb.heartbeatMs < 2000) {
                isEngineActive_ = true;
                return; // Already running and alive
            }
        }
        isEngineActive_ = false;

        wchar_t guiPath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, guiPath, MAX_PATH)) {
            wchar_t* pSlash = wcsrchr(guiPath, L'\\');
            if (pSlash) {
                *pSlash = L'\0';
                wchar_t hostPath[MAX_PATH];
                StringCchPrintfW(hostPath, MAX_PATH, L"%s\\SonaraHost.exe", guiPath);

                if (GetFileAttributesW(hostPath) != INVALID_FILE_ATTRIBUTES) {
                    STARTUPINFOW si = { sizeof(si) };
                    PROCESS_INFORMATION pi = {};
                    si.cb = sizeof(si);
                    si.dwFlags = STARTF_USESHOWWINDOW;
                    si.wShowWindow = SW_HIDE;

                    if (CreateProcessW(hostPath, nullptr, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        isEngineActive_ = true;
                    }
                }
            }
        }
    }

    void checkEngineStatus() {
        bool prevInstalled = isEngineInstalled_;
        bool prevActive = isEngineActive_;

        // check installation
        wchar_t guiPath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, guiPath, MAX_PATH)) {
            wchar_t* pSlash = wcsrchr(guiPath, L'\\');
            if (pSlash) {
                *pSlash = L'\0';
                wchar_t hostPath[MAX_PATH];
                StringCchPrintfW(hostPath, MAX_PATH, L"%s\\SonaraHost.exe", guiPath);
                isEngineInstalled_ = (GetFileAttributesW(hostPath) != INVALID_FILE_ATTRIBUTES);
            }
        }

        // check if running
        StatusBlock sb;
        if (statusReader_.read(sb)) {
            isEngineActive_ = (GetTickCount64() - sb.heartbeatMs < 2000);
            activeDeviceName_ = sb.activeDevice;
        } else {
            isEngineActive_ = false;
        }

        if (isEngineInstalled_ != prevInstalled || isEngineActive_ != prevActive) {
            pushEngineStatus();
        }
    }

    void pushEngineStatus() {
        if (!webview_) return;

        nlohmann::json d = {
            {"type", "engine-status"},
            {"data", {
                {"installed", isEngineInstalled_},
                {"active", isEngineActive_}
            }}
        };
        postWebMessage(d);
    }

    void postEngineLevels() {
        if (!webview_ || !isEngineActive_) return;

        StatusBlock sb;
        if (statusReader_.read(sb)) {
            nlohmann::json d = {
                {"type", "engine-levels"},
                {"data", {
                    {"rmsLeft", sb.rmsLeft},
                    {"rmsRight", sb.rmsRight},
                    {"peakLeft", sb.peakLeft},
                    {"peakRight", sb.peakRight},
                    {"sampleRate", sb.sampleRate},
                    {"channels", sb.channels},
                    {"activeDevice", sb.activeDevice}
                }}
            };

            // Copy rawSamples to JSON
            std::vector<float> samples(256);
            for (size_t i = 0; i < 256; ++i) {
                samples[i] = sb.rawSamples[i];
            }
            d["data"]["rawSamples"] = samples;

            postWebMessage(d);
        }
    }

    void postWebMessage(const nlohmann::json& jsonMsg) {
        if (!webview_) return;
        std::string s = jsonMsg.dump();
        std::wstring ws(s.begin(), s.end());
        webview_->PostWebMessageAsJson(ws.c_str());
    }

    void runElevatedPS(const wchar_t* psScript, nlohmann::json& responseData) {
        // Run PowerShell script elevated
        wchar_t guiPath[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, guiPath, MAX_PATH)) return;
        wchar_t* pSlash = wcsrchr(guiPath, L'\\');
        if (!pSlash) return;
        *pSlash = L'\0';

        wchar_t scriptPath[MAX_PATH];
        StringCchPrintfW(scriptPath, MAX_PATH, L"%s\\%s", guiPath, psScript);

        wchar_t dllPath[MAX_PATH];
        StringCchPrintfW(dllPath, MAX_PATH, L"%s\\SonaraAPO.dll", guiPath);

        wchar_t args[1024];
        if (wcscmp(psScript, L"install-engine.ps1") == 0) {
            StringCchPrintfW(args, 1024, L"-NoProfile -ExecutionPolicy Bypass -File \"%s\" \"%s\"", scriptPath, dllPath);
        } else {
            StringCchPrintfW(args, 1024, L"-NoProfile -ExecutionPolicy Bypass -File \"%s\"", scriptPath);
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas"; // Request UAC elevation
        sei.lpFile = L"powershell.exe";
        sei.lpParameters = args;
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;

        if (ShellExecuteExW(&sei)) {
            WaitForSingleObject(sei.hProcess, 60000); // Wait up to 60s
            CloseHandle(sei.hProcess);
        }

        checkEngineStatus();
        responseData["installed"] = isEngineInstalled_;
        responseData["active"] = isEngineActive_;
    }

    void handleWebMessage(const std::wstring& webMessageJson) {
        try {
            // Convert wstring to string
            std::string s(webMessageJson.begin(), webMessageJson.end());
            auto msg = nlohmann::json::parse(s);

            std::string type = msg["type"];

            if (type == "get-status") {
                int reqId = msg["reqId"];
                checkEngineStatus();

                nlohmann::json resp = {
                    {"type", "response"},
                    {"reqId", reqId},
                    {"data", {
                        {"installed", isEngineInstalled_},
                        {"active", isEngineActive_},
                        {"license", {
                            {"tier", "free"},
                            {"maxBoostPercent", 500},
                            {"launch", true}
                        }}
                    }}
                };
                postWebMessage(resp);
            }
            else if (type == "set-params") {
                auto data = msg["data"];

                if (data.contains("enabled")) params_.enabled = data["enabled"] ? 1 : 0;
                if (data.contains("boostPercent")) boostPercent_ = data["boostPercent"];
                if (data.contains("limiterOn")) params_.limiterOn = data["limiterOn"] ? 1 : 0;

                if (data.contains("bass")) params_.bass = data["bass"];
                if (data.contains("clarity")) params_.clarity = data["clarity"];
                if (data.contains("ambience")) params_.ambience = data["ambience"];
                if (data.contains("surround")) params_.surround = data["surround"];
                if (data.contains("dynamic")) params_.dynamic = data["dynamic"];

                if (data.contains("eqBands")) {
                    auto bands = data["eqBands"];
                    for (size_t i = 0; i < 10 && i < bands.size(); ++i) {
                        params_.eqBands[i].gain = bands[i]["gain"];
                        params_.eqBands[i].freq = bands[i]["freq"];
                        params_.eqBands[i].q = bands[i]["q"];
                        params_.eqBands[i].type = bands[i]["type"];
                    }
                }

                saveParams();
            }
            else if (type == "install-engine") {
                int reqId = msg["reqId"];
                nlohmann::json responseData;
                runElevatedPS(L"install-engine.ps1", responseData);

                nlohmann::json resp = {
                    {"type", "response"},
                    {"reqId", reqId},
                    {"data", responseData}
                };
                postWebMessage(resp);
                pushEngineStatus();
            }
            else if (type == "uninstall-engine") {
                int reqId = msg["reqId"];
                nlohmann::json responseData;
                runElevatedPS(L"uninstall-engine.ps1", responseData);

                nlohmann::json resp = {
                    {"type", "response"},
                    {"reqId", reqId},
                    {"data", responseData}
                };
                postWebMessage(resp);
                pushEngineStatus();
            }
            else if (type == "activate-license") {
                int reqId = msg["reqId"];
                // Always valid under LAUNCH_FREE = true
                nlohmann::json resp = {
                    {"type", "response"},
                    {"reqId", reqId},
                    {"data", {
                        {"ok", true},
                        {"tier", "free"},
                        {"maxBoostPercent", 500},
                        {"launch", true}
                    }}
                };
                postWebMessage(resp);
            }
            else if (type == "deactivate-license") {
                int reqId = msg["reqId"];
                nlohmann::json resp = {
                    {"type", "response"},
                    {"reqId", reqId},
                    {"data", {
                        {"tier", "free"},
                        {"maxBoostPercent", 500},
                        {"launch", true}
                    }}
                };
                postWebMessage(resp);
            }
            else if (type == "open-buy") {
                ShellExecuteW(nullptr, L"open", L"https://sonara.app/buy", nullptr, nullptr, SW_SHOWNORMAL);
            }
            else if (type == "toggle-autostart") {
                bool enable = msg["enable"];
                HKEY hKey;
                if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                    if (enable) {
                        wchar_t path[MAX_PATH];
                        GetModuleFileNameW(nullptr, path, MAX_PATH);
                        std::wstring quoted = L"\"" + std::wstring(path) + L"\"";
                        RegSetValueExW(hKey, L"Sonara", 0, REG_SZ, reinterpret_cast<const BYTE*>(quoted.c_str()), (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
                    } else {
                        RegDeleteValueW(hKey, L"Sonara");
                    }
                    RegCloseKey(hKey);
                }
            }
        } catch (...) {}
    }

    void initWebView() {
        // Locate built React dist path
        wchar_t guiPath[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, guiPath, MAX_PATH)) return;
        wchar_t* pSlash = wcsrchr(guiPath, L'\\');
        if (!pSlash) return;
        *pSlash = L'\0';

        // Local React app compiled files path (placed alongside the executable or in parent workspace)
        wchar_t distPath[MAX_PATH];
        StringCchPrintfW(distPath, MAX_PATH, L"%s\\dist", guiPath);

        // Fallback for development structure:
        if (GetFileAttributesW(distPath) == INVALID_FILE_ATTRIBUTES) {
            StringCchPrintfW(distPath, MAX_PATH, L"%s\\..\\..\\app\\dist", guiPath);
        }

        distPath_ = distPath;

        if (GetFileAttributesW(distPath_.c_str()) == INVALID_FILE_ATTRIBUTES) {
            wchar_t msg[512];
            StringCchPrintfW(msg, 512, L"React dist directory not found at:\n%s", distPath_.c_str());
            MessageBoxW(hWnd_, msg, L"Sonara Error", MB_ICONERROR | MB_OK);
        }

        // Determine a writable User Data Folder in %LOCALAPPDATA%\Sonara\WebView2
        wchar_t localAppData[MAX_PATH] = { 0 };
        wchar_t udfPath[MAX_PATH] = { 0 };
        wchar_t* pUdfPath = nullptr;
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
            StringCchPrintfW(udfPath, MAX_PATH, L"%s\\Sonara\\WebView2", localAppData);
            // Create directories
            wchar_t baseDir[MAX_PATH];
            StringCchPrintfW(baseDir, MAX_PATH, L"%s\\Sonara", localAppData);
            CreateDirectoryW(baseDir, nullptr);
            CreateDirectoryW(udfPath, nullptr);
            pUdfPath = udfPath;
        }

        // Initialize WebView2 Environment
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, pUdfPath, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result)) {
                        wchar_t msg[256];
                        StringCchPrintfW(msg, 256, L"Failed to create WebView2 environment. HRESULT: 0x%08X", result);
                        MessageBoxW(hWnd_, msg, L"Sonara Error", MB_ICONERROR | MB_OK);
                        return result;
                    }
                    webviewEnv_ = env;

                    // Create CoreWebView2Controller
                    env->CreateCoreWebView2Controller(
                        hWnd_,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (FAILED(result)) {
                                    wchar_t msg[256];
                                    StringCchPrintfW(msg, 256, L"Failed to create WebView2 controller. HRESULT: 0x%08X", result);
                                    MessageBoxW(hWnd_, msg, L"Sonara Error", MB_ICONERROR | MB_OK);
                                    return result;
                                }
                                webviewController_ = controller;
                                webviewController_->get_CoreWebView2(&webview_);

                                // Set bounds
                                RECT bounds;
                                GetClientRect(hWnd_, &bounds);
                                webviewController_->put_Bounds(bounds);

                                // Map virtual domain name https://sonara.local to our local dist folder
                                ComPtr<ICoreWebView2_3> webview3;
                                if (SUCCEEDED(webview_->QueryInterface(IID_PPV_ARGS(&webview3)))) {
                                    webview3->SetVirtualHostNameToFolderMapping(
                                        L"sonara.local",
                                        distPath_.c_str(),
                                        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
                                    );
                                }

                                // Inject JavaScript IPC Bridge Preload script
                                std::wstring preloadScript = L" \
                                window.api = { \
                                    _pendingResolves: {}, \
                                    _nextReqId: 1, \
                                    getStatus: () => { \
                                        return new Promise((resolve) => { \
                                            const id = window.api._nextReqId++; \
                                            window.api._pendingResolves[id] = resolve; \
                                            window.chrome.webview.postMessage({ type: 'get-status', reqId: id }); \
                                        }); \
                                    }, \
                                    activateLicense: (key) => { \
                                        return new Promise((resolve) => { \
                                            const id = window.api._nextReqId++; \
                                            window.api._pendingResolves[id] = resolve; \
                                            window.chrome.webview.postMessage({ type: 'activate-license', reqId: id, key: key }); \
                                        }); \
                                    }, \
                                    deactivateLicense: () => { \
                                        return new Promise((resolve) => { \
                                            const id = window.api._nextReqId++; \
                                            window.api._pendingResolves[id] = resolve; \
                                            window.chrome.webview.postMessage({ type: 'deactivate-license', reqId: id }); \
                                        }); \
                                    }, \
                                    installEngine: () => { \
                                        return new Promise((resolve) => { \
                                            const id = window.api._nextReqId++; \
                                            window.api._pendingResolves[id] = resolve; \
                                            window.chrome.webview.postMessage({ type: 'install-engine', reqId: id }); \
                                        }); \
                                    }, \
                                    uninstallEngine: () => { \
                                        return new Promise((resolve) => { \
                                            const id = window.api._nextReqId++; \
                                            window.api._pendingResolves[id] = resolve; \
                                            window.chrome.webview.postMessage({ type: 'uninstall-engine', reqId: id }); \
                                        }); \
                                    }, \
                                    setParams: (partial) => { \
                                        window.chrome.webview.postMessage({ type: 'set-params', data: partial }); \
                                    }, \
                                    openBuy: () => { \
                                        window.chrome.webview.postMessage({ type: 'open-buy' }); \
                                    }, \
                                    toggleAutostart: (enable) => { \
                                        window.chrome.webview.postMessage({ type: 'toggle-autostart', enable: enable }); \
                                    }, \
                                    _engineStatusCallbacks: [], \
                                    onEngineStatus: (cb) => { window.api._engineStatusCallbacks.push(cb); }, \
                                    _engineLevelsCallbacks: [], \
                                    onEngineLevels: (cb) => { window.api._engineLevelsCallbacks.push(cb); }, \
                                    _licenseStatusCallbacks: [], \
                                    onLicenseStatus: (cb) => { window.api._licenseStatusCallbacks.push(cb); }, \
                                    _hotkeyCallbacks: [], \
                                    onHotkey: (cb) => { window.api._hotkeyCallbacks.push(cb); } \
                                }; \
                                window.chrome.webview.addEventListener('message', (event) => { \
                                    const msg = event.data; \
                                    if (msg.type === 'response') { \
                                        const resolve = window.api._pendingResolves[msg.reqId]; \
                                        if (resolve) { \
                                            delete window.api._pendingResolves[msg.reqId]; \
                                            resolve(msg.data); \
                                        } \
                                    } else if (msg.type === 'engine-status') { \
                                        window.api._engineStatusCallbacks.forEach(cb => cb(msg.data)); \
                                    } else if (msg.type === 'engine-levels') { \
                                        window.api._engineLevelsCallbacks.forEach(cb => cb(msg.data)); \
                                    } else if (msg.type === 'license-status') { \
                                        window.api._licenseStatusCallbacks.forEach(cb => cb(msg.data)); \
                                    } else if (msg.type === 'hotkey') { \
                                        window.api._hotkeyCallbacks.forEach(cb => cb(msg.data)); \
                                    } \
                                }); \
                                ";

                                webview_->AddScriptToExecuteOnDocumentCreated(
                                    preloadScript.c_str(),
                                    Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
                                        [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; }
                                    ).Get()
                                );

                                // Listen to WebMessages from React (JSON)
                                webview_->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            LPWSTR message = nullptr;
                                            if (SUCCEEDED(args->get_WebMessageAsJson(&message)) && message) {
                                                handleWebMessage(message);
                                                CoTaskMemFree(message);
                                            }
                                            return S_OK;
                                        }
                                    ).Get(),
                                    nullptr
                                );

                                // Disable default context menu
                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(webview_->get_Settings(&settings))) {
                                    settings->put_AreDefaultContextMenusEnabled(FALSE);
                                    settings->put_AreDevToolsEnabled(TRUE); // Enable dev tools for testing
                                }

                                // Navigate to the virtual host
                                webview_->Navigate(L"https://sonara.local/index.html");

                                return S_OK;
                            }
                        ).Get()
                    );

                    return S_OK;
                }
            ).Get()
        );
        if (FAILED(hr)) {
            wchar_t msg[256];
            StringCchPrintfW(msg, 256, L"CreateCoreWebView2EnvironmentWithOptions failed synchronously. HRESULT: 0x%08X", hr);
            MessageBoxW(hWnd_, msg, L"Sonara Error", MB_ICONERROR | MB_OK);
        }
    }

    void setupTrayIcon() {
        NOTIFYICONDATAW nid = { sizeof(NOTIFYICONDATAW) };
        nid.hWnd = hWnd_;
        nid.uID = ID_TRAY_APP_ICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(hInst_, MAKEINTRESOURCE(IDI_APP_ICON));
        StringCchCopyW(nid.szTip, 128, L"Sonara Audio Enhancer");

        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    void removeTrayIcon() {
        NOTIFYICONDATAW nid = { sizeof(NOTIFYICONDATAW) };
        nid.hWnd = hWnd_;
        nid.uID = ID_TRAY_APP_ICON;
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }

    HWND hWnd_ = nullptr;
    HINSTANCE hInst_ = nullptr;
    std::wstring distPath_;

    // WebView2 interfaces
    ComPtr<ICoreWebView2Environment> webviewEnv_;
    ComPtr<ICoreWebView2Controller> webviewController_;
    ComPtr<ICoreWebView2> webview_;

    // Local engine state
    dsp::Parameters params_;
    float boostPercent_ = 100.0f;

    StatusReader statusReader_;
    bool isEngineInstalled_ = true;
    bool isEngineActive_ = false;
    std::string activeDeviceName_;
};

} // namespace wab::gui

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    wab::gui::MainWindow win;
    if (!win.create(hInstance)) {
        return 1;
    }
    win.show();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
