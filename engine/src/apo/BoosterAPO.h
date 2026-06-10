// BoosterAPO.h - The System Effect APO (SFX/MFX) that Windows loads into the
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

// {A1B2C3D4-E5F6-47A8-9B0C-1D2E3F4A5B6C} - CLSID of the Booster APO.
DEFINE_GUID(CLSID_BoosterAPO,
    0xa1b2c3d4, 0xe5f6, 0x47a8, 0x9b, 0x0c, 0x1d, 0x2e, 0x3f, 0x4a, 0x5b, 0x6c);

// Long-running identifier used in the registry APO registration.
class __declspec(uuid("A1B2C3D4-E5F6-47A8-9B0C-1D2E3F4A5B6C")) CBoosterAPO;

constexpr LONG_PTR APOERR_DEFAULT = 0;

class CBoosterAPO :
    public CBaseAudioProcessingObject,
    public IAudioSystemEffects2
{
public:
    CBoosterAPO();
    virtual ~CBoosterAPO();

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
    UINT64        m_frameCounter = 0;  // throttle param polling
};
