// BoosterAPO.cpp - implementation of the System Effect APO. Hosts the portable
// BoostEngine and pulls live parameters from shared memory. Real-time safe.
#include "BoosterAPO.h"
#include <new>
#include <cstring>

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
}

CBoosterAPO::~CBoosterAPO() {
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
    
    // We only support IEEE Float.
    if (fmt.guidFormatType != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
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
