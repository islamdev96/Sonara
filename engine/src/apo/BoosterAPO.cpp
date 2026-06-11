// BoosterAPO.cpp - implementation of the System Effect APO. Hosts the portable
// BoostEngine and pulls live parameters from shared memory. Real-time safe.
#include "BoosterAPO.h"
#include <audiomediatype.h>
#include <new>
#include <cstring>
#include <cmath>

extern LONG g_cDllRef;
// Registration properties advertised to the audio engine.
static const CRegAPOProperties<1> g_RegProps(
    CLSID_BoosterAPO,
    L"Sonara Engine",
    L"Copyright (c) Sonara",
    1, 1,
    __uuidof(IAudioProcessingObject),
    APO_FLAG_DEFAULT
);

CBoosterAPO::CBoosterAPO() : CBaseAudioProcessingObject(g_RegProps) {
    InterlockedIncrement(&g_cDllRef);
    // Open the shared parameter section. If it does not exist yet the engine
    // simply runs at unity until the UI publishes settings.
    m_params.open();
    m_status.open();
}

CBoosterAPO::~CBoosterAPO() {
    m_status.close();
    m_params.close();
    InterlockedDecrement(&g_cDllRef);
}

STDMETHODIMP CBoosterAPO::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown)) {
        *ppv = static_cast<IAudioProcessingObject*>(this);
    } else if (riid == __uuidof(IAudioProcessingObject)) {
        *ppv = static_cast<IAudioProcessingObject*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectConfiguration)) {
        *ppv = static_cast<IAudioProcessingObjectConfiguration*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectRT)) {
        *ppv = static_cast<IAudioProcessingObjectRT*>(this);
    } else if (riid == __uuidof(IAudioSystemEffects) ||
               riid == __uuidof(IAudioSystemEffects2)) {
        *ppv = static_cast<IAudioSystemEffects2*>(this);
    } else {
        *ppv = nullptr; return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP CBoosterAPO::GetRegistrationProperties(APO_REG_PROPERTIES** ppProps) {
    if (!ppProps) return E_POINTER;
    *ppProps = (APO_REG_PROPERTIES*)CoTaskMemAlloc(sizeof(APO_REG_PROPERTIES));
    if (!*ppProps) return E_OUTOFMEMORY;
    **ppProps = g_RegProps;
    return S_OK;
}

STDMETHODIMP CBoosterAPO::Initialize(UINT32 cbDataSize, BYTE* pbyData) {
    UNREFERENCED_PARAMETER(cbDataSize);
    UNREFERENCED_PARAMETER(pbyData);
    return S_OK;
}

STDMETHODIMP CBoosterAPO::GetLatency(HNSTIME* pTime) {
    if (!pTime) return E_POINTER;
    *pTime = 0; // limiter lookahead is internal and fixed; report 0 added blocks
    return S_OK;
}

STDMETHODIMP CBoosterAPO::IsInputFormatSupported(IAudioMediaType* pOpp, IAudioMediaType* pReq,
                                                 IAudioMediaType** ppSup) {
    if (!pReq) return E_POINTER;
    
    UNCOMPRESSEDAUDIOFORMAT fmt = {};
    HRESULT hr = pReq->GetUncompressedAudioFormat(&fmt);
    if (FAILED(hr)) return hr;
    
    // We only support IEEE Float. If not float, suggest the closest matching float format.
    if (fmt.guidFormatType != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
        if (ppSup == nullptr) {
            return APOERR_FORMAT_NOT_SUPPORTED;
        }

        WAVEFORMATEXTENSIBLE wfx = {};
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels = (WORD)fmt.dwSamplesPerFrame;
        wfx.Format.nSamplesPerSec = (DWORD)fmt.fFramesPerSecond;
        wfx.Format.wBitsPerSample = 32;
        wfx.Format.nBlockAlign = (WORD)(wfx.Format.nChannels * 4);
        wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.Samples.wValidBitsPerSample = 32;
        wfx.dwChannelMask = fmt.dwChannelMask;
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        IAudioMediaType* pProposed = nullptr;
        hr = CreateAudioMediaType((WAVEFORMATEX*)&wfx, sizeof(WAVEFORMATEXTENSIBLE), &pProposed);
        if (SUCCEEDED(hr)) {
            *ppSup = pProposed;
            return S_FALSE; // S_FALSE indicates a supported fallback format was suggested
        }
        return APOERR_FORMAT_NOT_SUPPORTED;
    }
    
    // If output format is specified, ensure it matches input parameters.
    if (pOpp) {
        UNCOMPRESSEDAUDIOFORMAT oppFmt = {};
        if (SUCCEEDED(pOpp->GetUncompressedAudioFormat(&oppFmt))) {
            if (oppFmt.dwSamplesPerFrame != fmt.dwSamplesPerFrame ||
                oppFmt.fFramesPerSecond != fmt.fFramesPerSecond) {
                return APOERR_FORMAT_NOT_SUPPORTED;
            }
        }
    }
    
    if (ppSup) {
        *ppSup = pReq;
        pReq->AddRef();
    }
    return S_OK;
}

STDMETHODIMP CBoosterAPO::LockForProcess(UINT32 cInput, APO_CONNECTION_DESCRIPTOR** ppInput,
                                         UINT32 cOutput, APO_CONNECTION_DESCRIPTOR** ppOutput) {
    HRESULT hr = CBaseAudioProcessingObject::LockForProcess(cInput, ppInput, cOutput, ppOutput);
    if (FAILED(hr)) return hr;

    // Pull the negotiated format from the input connections.
    UNCOMPRESSEDAUDIOFORMAT fmt = {};
    if (cInput > 0 && ppInput && ppInput[0] && ppInput[0]->pFormat) {
        ppInput[0]->pFormat->GetUncompressedAudioFormat(&fmt);
    }
    m_channels   = fmt.dwSamplesPerFrame ? fmt.dwSamplesPerFrame : 2;
    m_sampleRate = fmt.fFramesPerSecond ? fmt.fFramesPerSecond : 48000.0f;

    m_engine.prepare(m_sampleRate, (int)m_channels);
    m_params.open(); // Retry opening the file if it wasn't available at startup
    m_status.open(); // Also retry the status file
    wab::dsp::Parameters p;
    if (m_params.read(p)) m_engine.updateParameters(p);
    m_locked = true;
    return S_OK;
}

STDMETHODIMP CBoosterAPO::UnlockForProcess() {
    m_locked = false;
    return CBaseAudioProcessingObject::UnlockForProcess();
}

// ---- The real-time DSP callback. NO blocking, NO allocation here. ----
STDMETHODIMP_(void) CBoosterAPO::APOProcess(UINT32 cInput, APO_CONNECTION_PROPERTY** ppInput,
                                            UINT32 cOutput, APO_CONNECTION_PROPERTY** ppOutput) {
    if (cInput == 0 || cOutput == 0) return;
    APO_CONNECTION_PROPERTY* in = ppInput[0];
    APO_CONNECTION_PROPERTY* out = ppOutput[0];
    if (!in || !out) return;

    const UINT32 frames = in->u32ValidFrameCount;
    float* pIn  = reinterpret_cast<float*>(in->pBuffer);
    float* pOut = reinterpret_cast<float*>(out->pBuffer);

    switch (in->u32BufferFlags) {
    case BUFFER_INVALID:
        out->u32ValidFrameCount = 0; break;
    case BUFFER_SILENT:
        // Honor silence; copy through and let downstream see silence.
        std::memset(pOut, 0, sizeof(float) * frames * m_channels);
        out->u32ValidFrameCount = frames;
        out->u32BufferFlags = in->u32BufferFlags;
        break;
    case BUFFER_VALID:
    default:
        // Poll parameters every 4 calls (~40 ms) to stay responsive
        // to UI changes while keeping RT thread overhead minimal.
        if ((m_frameCounter++ & 0x3) == 0) {
            wab::dsp::Parameters p;
            if (m_params.read(p)) m_engine.updateParameters(p);
        }
        if (pIn != pOut) std::memcpy(pOut, pIn, sizeof(float) * frames * m_channels);
        m_engine.process(pOut, (int)frames);

        // Accumulate RMS/peak for the status writer.
        {
            float sumL = 0.0f, sumR = 0.0f, pkL = 0.0f, pkR = 0.0f;
            const UINT32 ch = m_channels;
            for (UINT32 n = 0; n < frames; ++n) {
                const float* f = pOut + static_cast<size_t>(n) * ch;
                const float absL = std::fabs(f[0]);
                sumL += f[0] * f[0];
                if (absL > pkL) pkL = absL;
                if (ch >= 2) {
                    const float absR = std::fabs(f[1]);
                    sumR += f[1] * f[1];
                    if (absR > pkR) pkR = absR;
                } else {
                    sumR = sumL; pkR = pkL;
                }
            }
            const float invN = (frames > 0) ? 1.0f / frames : 0.0f;
            m_rmsAccL += sumL * invN;
            m_rmsAccR += sumR * invN;
            if (pkL > m_peakL) m_peakL = pkL;
            if (pkR > m_peakR) m_peakR = pkR;
        }

        // Write status (heartbeat + levels) every 10 callbacks (~100 ms).
        if (++m_statusCounter >= 10) {
            const float rmsL = std::sqrt(m_rmsAccL / m_statusCounter);
            const float rmsR = std::sqrt(m_rmsAccR / m_statusCounter);
            m_status.write(rmsL, rmsR, m_peakL, m_peakR,
                           static_cast<uint32_t>(m_sampleRate), m_channels);
            m_rmsAccL = m_rmsAccR = 0.0f;
            m_peakL = m_peakR = 0.0f;
            m_statusCounter = 0;
        }

        out->u32ValidFrameCount = frames;
        out->u32BufferFlags = in->u32BufferFlags;
        break;
    }
}

STDMETHODIMP CBoosterAPO::GetEffectsList(LPGUID* ppEffectsIds, UINT* pcEffects, HANDLE) {
    if (!ppEffectsIds || !pcEffects) return E_POINTER;
    *ppEffectsIds = (LPGUID)CoTaskMemAlloc(sizeof(GUID));
    if (!*ppEffectsIds) return E_OUTOFMEMORY;
    **ppEffectsIds = CLSID_BoosterAPO;
    *pcEffects = 1;
    return S_OK;
}
