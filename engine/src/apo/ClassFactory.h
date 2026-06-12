// ClassFactory.h - minimal IClassFactory that creates CSonaraAPO instances.
#pragma once
#include <windows.h>
#include <unknwn.h>

class CSonaraClassFactory : public IClassFactory {
public:
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override { return InterlockedIncrement(&m_cRef); }
    STDMETHOD_(ULONG, Release)() override {
        const ULONG n = InterlockedDecrement(&m_cRef);
        if (n == 0) delete this;
        return n;
    }
    STDMETHOD(CreateInstance)(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    STDMETHOD(LockServer)(BOOL fLock) override;
private:
    LONG m_cRef = 1;
};

// DLL-global object/lock counters (defined in dllmain.cpp).
extern LONG g_cDllRef;
