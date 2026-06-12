// main.cpp - Sonara User-mode WASAPI Loopback to Render Host
// Captures audio from a default loopback device (virtual cable),
// processes it through BoostEngine DSP (reading params.bin),
// renders it to a physical playback device (speakers),
// and writes status.bin heartbeat and RMS levels.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <timeapi.h>
#include <avrt.h>

#include "../dsp/BoostEngine.h"
#include "../apo/SharedParams.h"
#include "../apo/SharedStatus.h"

// System GUIDs
const GUID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const GUID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const GUID IID_IAudioClient = __uuidof(IAudioClient);
const GUID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const GUID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

// Helper to convert wide string to string
std::string WideToString(const wchar_t* wstr) {
    if (!wstr) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, nullptr, nullptr);
    return str;
}

// Format check helper
bool IsIeeeFloat(const WAVEFORMATEX* wfx) {
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* wfe = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        if (wfe->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) return true;
    }
    return false;
}

// CPolicyConfigClient COM interface definition for changing default audio endpoint
struct __declspec(uuid("870AF99C-171D-4F9E-AF0D-E63DF40C2BC9")) CPolicyConfigClient;
struct __declspec(uuid("F8679F50-850A-41CF-9C72-430F290290C8")) IPolicyConfig;

interface IPolicyConfig : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**);
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**);
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR);
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*);
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, void*, void*);
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, void*);
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*);
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*);
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*);
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*);
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR wzDeviceId, ERole role);
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT);
};

HRESULT SetDefaultAudioDevice(LPCWSTR wszDeviceId) {
    IPolicyConfig* pPolicyConfig = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigClient), nullptr, 
                                  CLSCTX_ALL, __uuidof(IPolicyConfig), 
                                  (LPVOID*)&pPolicyConfig);
    if (SUCCEEDED(hr) && pPolicyConfig) {
        hr = pPolicyConfig->SetDefaultEndpoint(wszDeviceId, eConsole);
        pPolicyConfig->SetDefaultEndpoint(wszDeviceId, eMultimedia);
        pPolicyConfig->SetDefaultEndpoint(wszDeviceId, eCommunications);
        pPolicyConfig->Release();
    }
    return hr;
}

LPWSTR g_pwszOriginalDefaultId = nullptr;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (g_pwszOriginalDefaultId) {
        SetDefaultAudioDevice(g_pwszOriginalDefaultId);
    }
    return FALSE; // Let default handler terminate
}

// Simple multi-channel linear resampler
struct Resampler {
    double sampleRateIn = 0.0;
    double sampleRateOut = 0.0;
    int channels = 0;
    std::vector<float> lastFrame;
    double phase = 0.0;
    bool active = false;

    void prepare(double srIn, double srOut, int ch) {
        sampleRateIn = srIn;
        sampleRateOut = srOut;
        channels = ch;
        lastFrame.assign(ch, 0.0f);
        phase = 0.0;
        active = (std::abs(srIn - srOut) > 0.1);
    }

    void process(const float* input, int inputFrames, std::vector<float>& output) {
        if (!active) {
            output.assign(input, input + inputFrames * channels);
            return;
        }

        double ratio = sampleRateIn / sampleRateOut;
        int estimatedOutputFrames = static_cast<int>(inputFrames / ratio) + 2;
        output.clear();
        output.reserve(estimatedOutputFrames * channels);

        int inIdx = -1;
        while (true) {
            int nextInIdx = inIdx + 1;
            const float* currentFrame = nullptr;
            const float* nextFrame = nullptr;

            if (inIdx < 0) {
                currentFrame = lastFrame.data();
            } else if (inIdx < inputFrames) {
                currentFrame = input + inIdx * channels;
            } else {
                break;
            }

            if (nextInIdx < 0) {
                nextFrame = lastFrame.data();
            } else if (nextInIdx < inputFrames) {
                nextFrame = input + nextInIdx * channels;
            } else {
                break;
            }

            float t = static_cast<float>(phase);
            for (int c = 0; c < channels; ++c) {
                float sample = currentFrame[c] * (1.0f - t) + nextFrame[c] * t;
                output.push_back(sample);
            }

            phase += ratio;
            int advance = static_cast<int>(std::floor(phase));
            phase -= advance;
            inIdx += advance;
        }

        if (inputFrames > 0) {
            std::copy(input + (inputFrames - 1) * channels, input + inputFrames * channels, lastFrame.begin());
        }
    }
};

static void ConvertToFloat(const BYTE* pData, float* outBuf, DWORD numFrames, WORD channels, WORD bitsPerSample, bool isFloat, DWORD flags) {
    size_t numSamples = numFrames * channels;
    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        std::memset(outBuf, 0, numSamples * sizeof(float));
        return;
    }

    if (isFloat && bitsPerSample == 32) {
        std::memcpy(outBuf, pData, numSamples * sizeof(float));
    } else if (bitsPerSample == 16) {
        const short* pShort = reinterpret_cast<const short*>(pData);
        for (size_t i = 0; i < numSamples; ++i) {
            outBuf[i] = pShort[i] / 32768.0f;
        }
    } else if (bitsPerSample == 32) {
        const int32_t* pInt = reinterpret_cast<const int32_t*>(pData);
        for (size_t i = 0; i < numSamples; ++i) {
            outBuf[i] = pInt[i] / 2147483648.0f;
        }
    } else if (bitsPerSample == 24) {
        for (size_t i = 0; i < numSamples; ++i) {
            int32_t val = (pData[3 * i] << 8) | (pData[3 * i + 1] << 16) | (pData[3 * i + 2] << 24);
            outBuf[i] = val / 2147483648.0f;
        }
    } else {
        std::memset(outBuf, 0, numSamples * sizeof(float));
    }
}

static void ConvertFromFloat(const float* inBuf, BYTE* pData, DWORD numFrames, WORD channels, WORD bitsPerSample, bool isFloat) {
    size_t numSamples = numFrames * channels;
    if (isFloat && bitsPerSample == 32) {
        std::memcpy(pData, inBuf, numSamples * sizeof(float));
    } else if (bitsPerSample == 16) {
        short* pShort = reinterpret_cast<short*>(pData);
        for (size_t i = 0; i < numSamples; ++i) {
            float val = inBuf[i] * 32767.0f;
            pShort[i] = static_cast<short>(std::clamp(val, -32768.0f, 32767.0f));
        }
    } else if (bitsPerSample == 32) {
        int32_t* pInt = reinterpret_cast<int32_t*>(pData);
        for (size_t i = 0; i < numSamples; ++i) {
            float val = inBuf[i] * 2147483647.0f;
            pInt[i] = static_cast<int32_t>(std::clamp(val, -2147483648.0f, 2147483647.0f));
        }
    } else if (bitsPerSample == 24) {
        for (size_t i = 0; i < numSamples; ++i) {
            float val = inBuf[i] * 8388607.0f;
            int32_t ival = static_cast<int32_t>(std::clamp(val, -8388608.0f, 8388607.0f));
            pData[3 * i] = ival & 0xFF;
            pData[3 * i + 1] = (ival >> 8) & 0xFF;
            pData[3 * i + 2] = (ival >> 16) & 0xFF;
        }
    }
}

int main(int argc, char* argv[]) {
    // Enable MMCSS for the audio loop thread and set to time critical priority
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    if (hTask == NULL) {
        std::cerr << "[-] Warning: Failed to enable MMCSS Pro Audio: " << GetLastError() << std::endl;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    } else {
        std::cout << "[+] MMCSS Pro Audio enabled successfully." << std::endl;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "        Sonara VAD User-Mode Audio Host           " << std::endl;
    std::cout << "==================================================" << std::endl;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "[-] COM initialization failed: hr = " << std::hex << hr << std::endl;
        return 1;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) {
        std::cerr << "[-] Failed to create MMDeviceEnumerator: hr = " << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    IMMDeviceCollection* pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        std::cerr << "[-] Failed to enumerate render endpoints: hr = " << std::hex << hr << std::endl;
        pEnumerator->Release();
        CoUninitialize();
        return 1;
    }

    UINT count = 0;
    pCollection->GetCount(&count);
    
    std::string defaultDeviceName = "";
    IMMDevice* pDefaultDevice = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefaultDevice);
    if (SUCCEEDED(hr)) {
        pDefaultDevice->GetId(&g_pwszOriginalDefaultId);
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

        // Write original device ID to file for Electron restore fallback
        wchar_t originalDeviceFilePath[MAX_PATH];
        if (wab::SharedStatus::resolvePath(originalDeviceFilePath, MAX_PATH)) {
            wchar_t* pBin = wcsstr(originalDeviceFilePath, L"status.bin");
            if (pBin) {
                wcscpy_s(pBin, 12, L"origdev.txt");
                FILE* f = nullptr;
                if (_wfopen_s(&f, originalDeviceFilePath, L"w, ccs=UTF-8") == 0 && f) {
                    fwprintf(f, L"%s", g_pwszOriginalDefaultId);
                    fclose(f);
                }
            }
        }

        IPropertyStore* pProps = nullptr;
        pDefaultDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (pProps) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            pProps->GetValue(PKEY_Device_FriendlyName, &varName);
            defaultDeviceName = WideToString(varName.pwszVal);
            std::cout << "[*] Default Windows Render Device: " << defaultDeviceName << std::endl;
            PropVariantClear(&varName);
            pProps->Release();
        }
        pDefaultDevice->Release();
    }

    std::cout << "[+] Found " << count << " active render devices:" << std::endl;

    std::vector<std::pair<std::string, IMMDevice*>> devices;
    int virtualCableIdx = -1;
    int physicalSpeakerIdx = -1;

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* pDev = nullptr;
        pCollection->Item(i, &pDev);

        IPropertyStore* pProps = nullptr;
        pDev->OpenPropertyStore(STGM_READ, &pProps);
        
        std::string friendlyName = "Unknown Device";
        if (pProps) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            pProps->GetValue(PKEY_Device_FriendlyName, &varName);
            friendlyName = WideToString(varName.pwszVal);
            PropVariantClear(&varName);
            pProps->Release();
        }

        devices.push_back({friendlyName, pDev});
        std::cout << "  [" << i << "] " << friendlyName << std::endl;

        // Auto-selection heuristic
        std::string lowerName = friendlyName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        bool isVirtual = (lowerName.find("cable") != std::string::npos || 
                          lowerName.find("virtual") != std::string::npos || 
                          lowerName.find("fxsound") != std::string::npos);

        if (virtualCableIdx == -1 && isVirtual) {
            virtualCableIdx = i;
        } else if (physicalSpeakerIdx == -1 && !isVirtual && 
                   (lowerName.find("speaker") != std::string::npos || 
                    lowerName.find("realtek") != std::string::npos || 
                    lowerName.find("headphones") != std::string::npos || 
                    lowerName.find("audio") != std::string::npos)) {
            physicalSpeakerIdx = i;
        }
    }

    int captureIdx = -1;
    int renderIdx = -1;

    if (argc >= 3) {
        captureIdx = std::stoi(argv[1]);
        renderIdx = std::stoi(argv[2]);
        std::cout << "[+] Command line override: Capture=" << captureIdx << ", Render=" << renderIdx << std::endl;
    } else {
        captureIdx = (virtualCableIdx != -1) ? virtualCableIdx : 0;
        // fallback render is the first physical, or default console render
        renderIdx = (physicalSpeakerIdx != -1) ? physicalSpeakerIdx : (captureIdx == 0 && count > 1 ? 1 : 0);
        std::cout << "[+] Auto-selection: Capture=" << captureIdx << " (" << devices[captureIdx].first << "), Render=" << renderIdx << " (" << devices[renderIdx].first << ")" << std::endl;
    }

    if (captureIdx >= (int)count || renderIdx >= (int)count) {
        std::cerr << "[-] Error: Specified device index out of range." << std::endl;
        pCollection->Release();
        pEnumerator->Release();
        CoUninitialize();
        return 1;
    }

    if (captureIdx == renderIdx) {
        std::cerr << "[-] Error: Capture device index cannot be the same as the Render device index (" << captureIdx << ")." << std::endl;
        std::cerr << "[-] This would create a feedback loop. Please use a Virtual Audio Device (e.g. VB-CABLE or FxSound Speakers) as the capture source, and your real physical speaker as the render destination." << std::endl;
        pCollection->Release();
        pEnumerator->Release();
        CoUninitialize();
        return 1;
    }

    // Check if default playback device is indeed the capture device
    std::string lowerDefault = defaultDeviceName;
    std::transform(lowerDefault.begin(), lowerDefault.end(), lowerDefault.begin(), ::tolower);
    std::string lowerCapture = devices[captureIdx].first;
    std::transform(lowerCapture.begin(), lowerCapture.end(), lowerCapture.begin(), ::tolower);

    if (lowerDefault != lowerCapture) {
        std::cout << "\n==================================================" << std::endl;
        std::cout << " [!] WARNING: Sonara is NOT processing system sound!" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << " Your Windows default playback device is set to:" << std::endl;
        std::cout << "   '" << defaultDeviceName << "'" << std::endl;
        std::cout << " But Sonara expects audio to be sent to:" << std::endl;
        std::cout << "   '" << devices[captureIdx].first << "'" << std::endl;
        std::cout << "\n >>> TO FIX THIS: <<<" << std::endl;
        std::cout << " 1. Click the speaker icon in your Windows Taskbar tray." << std::endl;
        std::cout << " 2. Select '" << devices[captureIdx].first << "' as your output device." << std::endl;
        std::cout << " Sonara will then boost it and play it out of '" << devices[renderIdx].first << "'." << std::endl;
        std::cout << "==================================================\n" << std::endl;
    }

    IMMDevice* pCaptureDevice = devices[captureIdx].second;
    IMMDevice* pRenderDevice = devices[renderIdx].second;

    pCaptureDevice->AddRef();
    pRenderDevice->AddRef();
    pCollection->Release();

    // Initialize Capture Client
    IAudioClient* pCaptureClient = nullptr;
    hr = pCaptureDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&pCaptureClient);
    if (FAILED(hr)) {
        std::cerr << "[-] Failed to activate capture client: hr = " << std::hex << hr << std::endl;
        return 1;
    }

    WAVEFORMATEX* pwfxCapture = nullptr;
    pCaptureClient->GetMixFormat(&pwfxCapture);
    std::cout << "[+] Capture Mix Format: " << pwfxCapture->nSamplesPerSec << " Hz, " << pwfxCapture->nChannels << " channels, " << pwfxCapture->wBitsPerSample << " bits" << std::endl;

    HANDLE hCaptureEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!hCaptureEvent) {
        std::cerr << "[-] Failed to create capture event." << std::endl;
        return 1;
    }

    hr = pCaptureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 300000, 0, pwfxCapture, nullptr);
    if (FAILED(hr)) {
        std::cerr << "[-] Failed to initialize capture client loopback: hr = " << std::hex << hr << std::endl;
        CloseHandle(hCaptureEvent);
        return 1;
    }

    hr = pCaptureClient->SetEventHandle(hCaptureEvent);
    if (FAILED(hr)) {
        std::cerr << "[-] Failed to set capture event handle: hr = " << std::hex << hr << std::endl;
        CloseHandle(hCaptureEvent);
        return 1;
    }

    IAudioCaptureClient* pCaptureClientService = nullptr;
    pCaptureClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClientService);

    // Initialize Render Client
    IAudioClient* pRenderClient = nullptr;
    hr = pRenderDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&pRenderClient);
    if (FAILED(hr)) {
        std::cerr << "[-] Failed to activate render client: hr = " << std::hex << hr << std::endl;
        return 1;
    }

    WAVEFORMATEX* pwfxRender = nullptr;
    pRenderClient->GetMixFormat(&pwfxRender);
    std::cout << "[+] Render Native Mix Format: " << pwfxRender->nSamplesPerSec << " Hz, " << pwfxRender->nChannels << " channels, " << pwfxRender->wBitsPerSample << " bits" << std::endl;

    WAVEFORMATEX* pwfxRenderToUse = pwfxCapture;
    std::cout << "[+] Attempting to initialize Render client with Capture format..." << std::endl;
    hr = pRenderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 400000, 0, pwfxRenderToUse, nullptr);
    if (FAILED(hr)) {
        std::cout << "[*] Failed to initialize with Capture format. Falling back to native Render format..." << std::endl;
        pwfxRenderToUse = pwfxRender;
        hr = pRenderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 400000, 0, pwfxRenderToUse, nullptr);
        if (FAILED(hr)) {
            std::cerr << "[-] Failed to initialize render client: hr = " << std::hex << hr << std::endl;
            return 1;
        }
    } else {
        std::cout << "[+] Render client successfully initialized with Capture format." << std::endl;
    }
    pwfxRender = pwfxRenderToUse;

    IAudioRenderClient* pRenderClientService = nullptr;
    pRenderClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClientService);

    // Switch default playback device to the capture device (virtual device)
    LPWSTR pwszCaptureId = nullptr;
    if (SUCCEEDED(pCaptureDevice->GetId(&pwszCaptureId)) && pwszCaptureId) {
        std::wcout << L"[+] Setting default Windows playback device to virtual device: " << pwszCaptureId << std::endl;
        SetDefaultAudioDevice(pwszCaptureId);
        CoTaskMemFree(pwszCaptureId);
    }

    // Prepare DSP Engine
    wab::dsp::BoostEngine dspEngine;
    dspEngine.prepare(pwfxCapture->nSamplesPerSec, pwfxCapture->nChannels);

    // Open Shared Parameters and Status
    wab::SharedParams paramsReader;
    if (!paramsReader.open()) {
        std::cout << "[*] Shared params file not found. Will retry when UI starts." << std::endl;
    }

    wab::SharedStatus statusWriter;
    if (!statusWriter.open()) {
        std::cerr << "[-] Failed to open status.bin file for writing heartbeat!" << std::endl;
    }

    // Pre-roll render client with silence to avoid initial buffer underrun
    UINT32 renderBufferSize = 0;
    pRenderClient->GetBufferSize(&renderBufferSize);
    UINT32 preRoll = 1024;
    BYTE* pPreRollData = nullptr;
    hr = pRenderClientService->GetBuffer(preRoll, &pPreRollData);
    if (SUCCEEDED(hr) && pPreRollData) {
        std::memset(pPreRollData, 0, preRoll * pwfxRender->nBlockAlign);
        pRenderClientService->ReleaseBuffer(preRoll, 0);
    }

    // Start Audio Streams
    hr = pCaptureClient->Start();
    hr = pRenderClient->Start();
    if (FAILED(hr)) {
        std::cerr << "[-] Failed to start audio client(s)" << std::endl;
        return 1;
    }

    std::cout << "[+] Audio processing loop started. Press Ctrl+C to stop." << std::endl;

    timeBeginPeriod(1);
    ULONGLONG lastStatusWriteTicks = GetTickCount64();
    double sumSqLAcc = 0.0;
    double sumSqRAcc = 0.0;
    float peakLAcc = 0.0f;
    float peakRAcc = 0.0f;
    uint32_t frameCountAcc = 0;
    float rawSamplesAcc[256] = {0};
    bool isFloatCapture = IsIeeeFloat(pwfxCapture);
    bool isFloatRender = IsIeeeFloat(pwfxRender);
    std::cout << "[+] Capture Float: " << (isFloatCapture ? "YES" : "NO") << std::endl;
    std::cout << "[+] Render Float: " << (isFloatRender ? "YES" : "NO") << std::endl;
    std::vector<float> fifoBuffer;

    Resampler resampler;
    resampler.prepare(pwfxCapture->nSamplesPerSec, pwfxRender->nSamplesPerSec, pwfxRender->nChannels);

    while (true) {
        // Read updated parameters from UI
        wab::dsp::Parameters params;
        if (paramsReader.read(params)) {
            dspEngine.updateParameters(params);
        } else {
            // Retry opening params mapping in case UI just started
            paramsReader.open();
        }

        UINT32 nextPacketSize = 0;
        hr = pCaptureClientService->GetNextPacketSize(&nextPacketSize);
        if (FAILED(hr)) break;

        while (nextPacketSize > 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesRead = 0;
            DWORD flags = 0;

            hr = pCaptureClientService->GetBuffer(&pData, &numFramesRead, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

            if (numFramesRead > 0) {
                int channels = pwfxCapture->nChannels;
                std::vector<float> processBuf(numFramesRead * channels, 0.0f);

                // 1. Convert captured data to float
                ConvertToFloat(pData, processBuf.data(), numFramesRead, channels, pwfxCapture->wBitsPerSample, isFloatCapture, flags);

                // 2. Process through DSP
                dspEngine.process(processBuf.data(), numFramesRead);

                // Save latest raw samples for FFT visualizer
                {
                    int copyCount = std::min(256, (int)numFramesRead);
                    for (int i = 0; i < 256; ++i) {
                        if (i < copyCount) {
                            int frameIdx = (int)numFramesRead - copyCount + i;
                            if (channels >= 2) {
                                rawSamplesAcc[i] = (processBuf[frameIdx * channels] + processBuf[frameIdx * channels + 1]) * 0.5f;
                            } else {
                                rawSamplesAcc[i] = processBuf[frameIdx * channels];
                            }
                        }
                    }
                }

                // 3. Accumulate live levels for status.bin
                for (UINT32 f = 0; f < numFramesRead; ++f) {
                    float l = processBuf[f * channels];
                    float r = (channels > 1) ? processBuf[f * channels + 1] : l;
                    sumSqLAcc += static_cast<double>(l) * l;
                    sumSqRAcc += static_cast<double>(r) * r;
                    peakLAcc = std::max(peakLAcc, std::fabs(l));
                    peakRAcc = std::max(peakRAcc, std::fabs(r));
                }
                frameCountAcc += numFramesRead;

                // Channel conversion from Capture Channels to Render Channels
                int renderChannels = pwfxRender->nChannels;
                std::vector<float> renderReadyBuf(numFramesRead * renderChannels, 0.0f);
                if (channels == renderChannels) {
                    renderReadyBuf = processBuf;
                } else if (channels == 2 && renderChannels == 1) {
                    for (UINT32 f = 0; f < numFramesRead; ++f) {
                        renderReadyBuf[f] = (processBuf[f * 2] + processBuf[f * 2 + 1]) * 0.5f;
                    }
                } else if (channels == 1 && renderChannels == 2) {
                    for (UINT32 f = 0; f < numFramesRead; ++f) {
                        float mono = processBuf[f];
                        renderReadyBuf[f * 2] = mono;
                        renderReadyBuf[f * 2 + 1] = mono;
                    }
                } else if (channels == 2 && renderChannels > 2) {
                    for (UINT32 f = 0; f < numFramesRead; ++f) {
                        float l = processBuf[f * 2];
                        float r = processBuf[f * 2 + 1];
                        renderReadyBuf[f * renderChannels + 0] = l; // FL
                        renderReadyBuf[f * renderChannels + 1] = r; // FR
                        if (renderChannels > 2) renderReadyBuf[f * renderChannels + 2] = (l + r) * 0.5f; // Center
                        if (renderChannels > 4) {
                            renderReadyBuf[f * renderChannels + 4] = l; // BL
                            renderReadyBuf[f * renderChannels + 5] = r; // BR
                        }
                        if (renderChannels > 7) {
                            renderReadyBuf[f * renderChannels + 6] = l; // SL
                            renderReadyBuf[f * renderChannels + 7] = r; // SR
                        }
                    }
                } else {
                    int minCh = std::min((int)channels, (int)renderChannels);
                    for (UINT32 f = 0; f < numFramesRead; ++f) {
                        for (int c = 0; c < minCh; ++c) {
                            renderReadyBuf[f * renderChannels + c] = processBuf[f * channels + c];
                        }
                    }
                }

                // Resample to render rate if necessary
                std::vector<float> resampledBuf;
                resampler.process(renderReadyBuf.data(), numFramesRead, resampledBuf);

                // Push processed & resampled samples to FIFO buffer
                fifoBuffer.insert(fifoBuffer.end(), resampledBuf.begin(), resampledBuf.end());
            }

            pCaptureClientService->ReleaseBuffer(numFramesRead);

            hr = pCaptureClientService->GetNextPacketSize(&nextPacketSize);
            if (FAILED(hr)) break;
        }

        // 4. Output as much as possible from FIFO to physical Speakers
        int renderChannels = pwfxRender->nChannels;
        UINT32 fifoFrames = (UINT32)(fifoBuffer.size() / renderChannels);

        // Drift protection using render rate
        UINT32 maxFifoFrames = (pwfxRender->nSamplesPerSec * 80) / 1000;
        if (fifoFrames > maxFifoFrames) {
            UINT32 targetFifoFrames = (pwfxRender->nSamplesPerSec * 20) / 1000;
            size_t dropSamples = (fifoFrames - targetFifoFrames) * renderChannels;
            if (fifoBuffer.size() > dropSamples) {
                fifoBuffer.erase(fifoBuffer.begin(), fifoBuffer.begin() + dropSamples);
            }
            fifoFrames = (UINT32)(fifoBuffer.size() / renderChannels);
        }

        UINT32 numPaddingFrames = 0;
        hr = pRenderClient->GetCurrentPadding(&numPaddingFrames);
        if (SUCCEEDED(hr)) {
            // Target render padding to prevent underrun (30ms)
            UINT32 targetPadding = (pwfxRender->nSamplesPerSec * 30) / 1000;

            // Only write cushion if we actually have audio in the FIFO to play
            if (fifoFrames > 0 && (numPaddingFrames + fifoFrames < targetPadding)) {
                UINT32 cushionFrames = targetPadding - (numPaddingFrames + fifoFrames);
                UINT32 numFreeFrames = renderBufferSize - numPaddingFrames;
                if (cushionFrames > numFreeFrames) {
                    cushionFrames = numFreeFrames;
                }
                if (cushionFrames > 0) {
                    BYTE* pRenderData = nullptr;
                    hr = pRenderClientService->GetBuffer(cushionFrames, &pRenderData);
                    if (SUCCEEDED(hr) && pRenderData) {
                        std::memset(pRenderData, 0, cushionFrames * pwfxRender->nBlockAlign);
                        pRenderClientService->ReleaseBuffer(cushionFrames, 0);
                        numPaddingFrames += cushionFrames;
                    }
                }
            }

            UINT32 numFreeFrames = renderBufferSize - numPaddingFrames;
            UINT32 framesToWrite = std::min(numFreeFrames, fifoFrames);
            
            if (framesToWrite > 0) {
                BYTE* pRenderData = nullptr;
                hr = pRenderClientService->GetBuffer(framesToWrite, &pRenderData);
                if (SUCCEEDED(hr) && pRenderData) {
                    ConvertFromFloat(fifoBuffer.data(), pRenderData, framesToWrite, renderChannels, pwfxRender->wBitsPerSample, isFloatRender);
                    pRenderClientService->ReleaseBuffer(framesToWrite, 0);
                    fifoBuffer.erase(fifoBuffer.begin(), fifoBuffer.begin() + (framesToWrite * renderChannels));
                }
            }
        }

        // 5. Periodic status write (every 100ms)
        ULONGLONG nowTicks = GetTickCount64();
        if (nowTicks - lastStatusWriteTicks >= 100) {
            float rmsL = 0.0f;
            float rmsR = 0.0f;
            float peakL = 0.0f;
            float peakR = 0.0f;
            if (frameCountAcc > 0) {
                rmsL = static_cast<float>(std::sqrt(sumSqLAcc / frameCountAcc));
                rmsR = static_cast<float>(std::sqrt(sumSqRAcc / frameCountAcc));
                peakL = peakLAcc;
                peakR = peakRAcc;
            }

            char activeDevice[128] = {0};
            IMMDevice* pCurrentDefault = nullptr;
            if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pCurrentDefault)) && pCurrentDefault) {
                IPropertyStore* pProps = nullptr;
                if (SUCCEEDED(pCurrentDefault->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
                        WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, activeDevice, sizeof(activeDevice), nullptr, nullptr);
                    }
                    PropVariantClear(&varName);
                    pProps->Release();
                }
                pCurrentDefault->Release();
            }

            statusWriter.write(rmsL, rmsR, peakL, peakR, pwfxCapture->nSamplesPerSec, pwfxCapture->nChannels, activeDevice, rawSamplesAcc);

            // Reset accumulators
            sumSqLAcc = 0.0;
            sumSqRAcc = 0.0;
            peakLAcc = 0.0f;
            peakRAcc = 0.0f;
            frameCountAcc = 0;
            lastStatusWriteTicks = nowTicks;
        }

        // Wait for next capture packet with 500ms timeout
        WaitForSingleObject(hCaptureEvent, 500);
    }

    timeEndPeriod(1);

    if (hTask) {
        AvRevertMmThreadCharacteristics(hTask);
    }

    // Stop streams
    pCaptureClient->Stop();
    pRenderClient->Stop();

    // Clean up
    pCaptureClientService->Release();
    pRenderClientService->Release();
    pCaptureClient->Release();
    pRenderClient->Release();
    pCaptureDevice->Release();
    pRenderDevice->Release();
    if (hCaptureEvent) {
        CloseHandle(hCaptureEvent);
    }
    // Restore original default playback device
    if (g_pwszOriginalDefaultId) {
        std::wcout << L"[*] Restoring original default Windows playback device..." << std::endl;
        SetDefaultAudioDevice(g_pwszOriginalDefaultId);
        CoTaskMemFree(g_pwszOriginalDefaultId);
        g_pwszOriginalDefaultId = nullptr;
    }

    pEnumerator->Release();
    CoUninitialize();

    return 0;
}
