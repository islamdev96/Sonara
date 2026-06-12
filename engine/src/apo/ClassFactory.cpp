#include "ClassFactory.h"
#include "SonaraAPO.h"
#include <new>

STDMETHODIMP CSonaraClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP CSonaraClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    CSonaraAPO* apo = new (std::nothrow) CSonaraAPO();
    if (!apo) return E_OUTOFMEMORY;
    const HRESULT hr = apo->QueryInterface(riid, ppv);
    apo->Release();
    return hr;
}

STDMETHODIMP CSonaraClassFactory::LockServer(BOOL fLock) {
    if (fLock) InterlockedIncrement(&g_cDllRef);
    else       InterlockedDecrement(&g_cDllRef);
    return S_OK;
}
