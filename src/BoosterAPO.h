#pragma once
#include <windows.h>
// These headers require the Windows Driver Kit (WDK) to be installed
// #include <audioenginebaseapo.h>
// #include <baseaudioprocessingobject.h>

// Unique GUID for our Booster APO (will be registered in Windows Registry)
// {A1B2C3D4-E5F6-47A8-9B0C-1D2E3F4A5B6C}
DEFINE_GUID(CLSID_BoosterAPO, 0xa1b2c3d4, 0xe5f6, 0x47a8, 0x9b, 0x0c, 0x1d, 0x2e, 0x3f, 0x4a, 0x5b, 0x6c);

// Forward declaration of CBaseAudioProcessingObject to allow compiling the skeleton without WDK
class CBaseAudioProcessingObject {
public:
    virtual ~CBaseAudioProcessingObject() {}
};

struct APO_CONNECTION_PROPERTY {
    void* pBuffer;
    UINT32 u32ValidFrameCount;
    UINT32 u32BufferFlags;
};

class CBoosterAPO : public CBaseAudioProcessingObject
{
public:
    CBoosterAPO();
    virtual ~CBoosterAPO();

    // IAudioProcessingObjectRT methods (Real-Time DSP)
    // STDMETHOD_ is used for COM interfaces. We use a standard void for the skeleton.
    virtual void APOProcess(
        UINT32 u32NumInputConnections,
        APO_CONNECTION_PROPERTY** ppInputConnections,
        UINT32 u32NumOutputConnections,
        APO_CONNECTION_PROPERTY** ppOutputConnections
    );

    // COM Registration (stub)
    static HRESULT Register();
    static HRESULT Unregister();

private:
    float m_fGainMultiplier; // The volume boost factor (e.g., 3.0 for 300%)
    
    // Soft clipper to prevent harsh distortion
    inline float SoftClip(float sample);
};
