// SonaraAPO.h - The System Effect APO (SFX/MFX) that Windows loads into the
// audio engine for the chosen endpoint. It hosts our portable BoostEngine and
// reads live parameters from shared memory. This is the heart of the
// self-contained product: NO Equalizer APO, NO third-party driver.
//
// BUILD REQUIREMENT: needs the Windows Driver Kit (WDK) which provides
// <audioenginebaseapo.h> and <baseaudioprocessingobject.h> and the
// BaseAudioProcessingObject.lib import library. See BUILD.md.
#pragma once
#include <windows.h>
#include <unknwn.h>
#include <audioenginebaseapo.h>
#include <baseaudioprocessingobject.h>
#include <audioengineextensionapo.h>
#include "../dsp/BoostEngine.h"
#include "SharedParams.h"
#include "SharedStatus.h"
 
// {538B6BB6-27D6-4D50-A09D-6E1883A66888} - CLSID of the Sonara APO.
// Generated via [guid]::NewGuid() — do NOT reuse placeholder GUIDs in production.
DEFINE_GUID(CLSID_SonaraAPO,
    0x538b6bb6, 0x27d6, 0x4d50, 0xa0, 0x9d, 0x6e, 0x18, 0x83, 0xa6, 0x68, 0x88);
 
// Long-running identifier used in the registry APO registration.
class __declspec(uuid("538B6BB6-27D6-4D50-A09D-6E1883A66888")) CSonaraAPO;
 
constexpr LONG_PTR APOERR_DEFAULT = 0;
 
class CSonaraAPO :
    public CBaseAudioProcessingObject,
    public IAudioSystemEffects2
{
public:
    CSonaraAPO();
    virtual ~CSonaraAPO();
 
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override { return InterlockedIncrement(&m_cRef); }
    STDMETHOD_(ULONG, Release)() override {
        const ULONG n = InterlockedDecrement(&m_cRef);
        if (n == 0) delete this;
        return n;
    }
 
    // IAudioProcessingObject
    STDMETHOD(Initialize)(UINT32 cbDataSize, BYTE* pbyData) override;
    STDMETHOD(GetLatency)(HNSTIME* pTime) override;
    STDMETHOD(GetRegistrationProperties)(APO_REG_PROPERTIES** ppProps) override;
 
    // IAudioProcessingObjectConfiguration
    STDMETHOD(LockForProcess)(UINT32 cInput, APO_CONNECTION_DESCRIPTOR** ppInput,
                              UINT32 cOutput, APO_CONNECTION_DESCRIPTOR** ppOutput) override;
    STDMETHOD(UnlockForProcess)() override;
 
    // IAudioProcessingObjectRT  (called on the real-time audio thread!)
    STDMETHOD_(void, APOProcess)(UINT32 cInput, APO_CONNECTION_PROPERTY** ppInput,
                                 UINT32 cOutput, APO_CONNECTION_PROPERTY** ppOutput) override;
    STDMETHOD_(UINT32, CalcInputFrames)(UINT32 outputFrames) override { return outputFrames; }
    STDMETHOD_(UINT32, CalcOutputFrames)(UINT32 inputFrames) override { return inputFrames; }
    STDMETHOD(IsInputFormatSupported)(IAudioMediaType* pOpp, IAudioMediaType* pReq,
                                      IAudioMediaType** ppSup) override;
 
    // IAudioSystemEffects2 (lets the OS list our effect in the audio stack)
    STDMETHOD(GetEffectsList)(LPGUID* ppEffectsIds, UINT* pcEffects, HANDLE event) override;
 
private:
    LONG          m_cRef = 1;
    bool          m_locked = false;
    UINT32        m_channels = 2;
    float         m_sampleRate = 48000.0f;
 
    wab::dsp::BoostEngine m_engine;   // the self-contained DSP engine
    wab::SharedParams     m_params;   // live parameters from the UI
    wab::SharedStatus     m_status;   // heartbeat + RMS back to the UI
    UINT64        m_frameCounter = 0;  // throttle param polling
    // Accumulate RMS/peak over several callbacks, write status every ~10 calls.
    float         m_rmsAccL = 0.0f, m_rmsAccR = 0.0f;
    float         m_peakL = 0.0f, m_peakR = 0.0f;
    UINT32        m_statusCounter = 0;
};
