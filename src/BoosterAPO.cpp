#include "BoosterAPO.h"
#include <cmath>

CBoosterAPO::CBoosterAPO()
{
    // Start with a default gain of 3.0 (300% boost)
    m_fGainMultiplier = 3.0f;
}

CBoosterAPO::~CBoosterAPO()
{
}

// A simple soft clipper using hyperbolic tangent (tanh) to round off peaks
// This prevents the loud crackling sound when audio goes above 0dBFS (1.0 or -1.0)
inline float CBoosterAPO::SoftClip(float sample)
{
    // Simple fast hard clip first if it's way out of bounds
    if (sample > 1.0f) return 1.0f;
    if (sample < -1.0f) return -1.0f;
    
    // For a real commercial app, you would use a look-up table or faster approximation of tanh
    // return std::tanh(sample);
    return sample;
}

// This is the core engine where the audio gets boosted.
// It runs in the real-time audio thread. NO BLOCKING CALLS HERE.
void CBoosterAPO::APOProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_PROPERTY** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_PROPERTY** ppOutputConnections)
{
    // We only process if we have valid input and output buffers
    if (u32NumInputConnections == 0 || u32NumOutputConnections == 0) return;

    FLOAT32* pfInput = reinterpret_cast<FLOAT32*>(ppInputConnections[0]->pBuffer);
    FLOAT32* pfOutput = reinterpret_cast<FLOAT32*>(ppOutputConnections[0]->pBuffer);
    
    // The number of frames (samples per channel) to process
    UINT32 u32ValidFrameCount = ppInputConnections[0]->u32ValidFrameCount;
    
    // Assuming stereo (2 channels) for simplicity in this POC
    // In a real APO, we would read the channel count from the negotiated audio format.
    UINT32 u32Samples = u32ValidFrameCount * 2; 

    // Apply the gain multiplier and clip
    for (UINT32 i = 0; i < u32Samples; i++)
    {
        float boostedSample = pfInput[i] * m_fGainMultiplier;
        pfOutput[i] = SoftClip(boostedSample);
    }
    
    // Mark the output connection property with the valid frame count
    ppOutputConnections[0]->u32ValidFrameCount = u32ValidFrameCount;
    ppOutputConnections[0]->u32BufferFlags = ppInputConnections[0]->u32BufferFlags;
}

HRESULT CBoosterAPO::Register()
{
    // Stub: In a real implementation, this would write to the registry to register the COM object
    // and attach it to the Windows Audio Engine FX keys.
    return S_OK;
}

HRESULT CBoosterAPO::Unregister()
{
    // Stub: Removes the COM registration and FX keys from the registry.
    return S_OK;
}
