// SharedParams.h - Cross-process transport for the Parameters block.
//
// Design goal: the Electron UI must be able to drive the engine in real time
// WITHOUT a native Node addon. We therefore back the shared region with a small
// file at %ProgramData%\WinAudioBoosterPro\params.bin:
//   * The UI writes the binary Parameters struct to that file (plain fs write).
//   * Each APO instance memory-maps the same file once (in LockForProcess) and
//     reads it lock-free on the RT thread via a seqlock double-read.
// Windows keeps file writes and mapped views of the same local file coherent,
// so UI edits are visible to the engine within one poll interval.
#pragma once
#include <windows.h>
#include <shlobj.h>
#include <strsafe.h>
#include "../dsp/Parameters.h"

namespace wab {

class SharedParams {
public:
    ~SharedParams() { close(); }

    bool open() {
        if (view_) return true; // self-healing: exit if already open
        wchar_t path[MAX_PATH];
        if (!resolvePath(path, MAX_PATH)) return false;

        // Open read-only to support sandboxed audiodg.exe execution
        hFile_ = CreateFileW(path, GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) { hFile_ = nullptr; return false; }

        hMap_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, 0,
                                   sizeof(dsp::Parameters), nullptr);
        if (!hMap_) { close(); return false; }
        view_ = reinterpret_cast<dsp::Parameters*>(
            MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, sizeof(dsp::Parameters)));
        if (!view_) { close(); return false; }
        // Pin the page in physical memory so the RT thread never takes a
        // soft page fault on first read (which would cause a glitch).
        VirtualLock(view_, sizeof(dsp::Parameters));
        return true;
    }

    // RT-thread reader: seqlock double-read to avoid torn snapshots.
    // IMPORTANT: No Sleep/blocking calls allowed here — this runs on the
    // real-time audio thread inside audiodg.exe.
    bool read(dsp::Parameters& out) const {
        if (!view_) return false;
        for (int i = 0; i < 4; ++i) {
            const uint32_t s1 = view_->seq; MemoryBarrier();
            if (s1 == 0) {
                // Write in progress by the UI. Spin briefly and retry.
                YieldProcessor();
                continue;
            }
            out = *view_;                   MemoryBarrier();
            const uint32_t s2 = view_->seq;
            if (s1 == s2 && out.magic == dsp::kParamMagic) return true;
        }
        return out.magic == dsp::kParamMagic;
    }

    void write(dsp::Parameters) {
        // Disabled in read-only engine mode
    }

    void close() {
        if (view_) { UnmapViewOfFile(view_); view_ = nullptr; }
        if (hMap_) { CloseHandle(hMap_); hMap_ = nullptr; }
        if (hFile_) { CloseHandle(hFile_); hFile_ = nullptr; }
    }

    static bool resolvePath(wchar_t* out, size_t cch) {
        wchar_t base[MAX_PATH];
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, base)))
            return false;
        wchar_t dir[MAX_PATH];
        StringCchPrintfW(dir, MAX_PATH, L"%s\\Sonara", base);
        CreateDirectoryW(dir, nullptr);
        return SUCCEEDED(StringCchPrintfW(out, cch, L"%s\\params.bin", dir));
    }

private:
    HANDLE hFile_ = nullptr;
    HANDLE hMap_ = nullptr;
    dsp::Parameters* view_ = nullptr;
    uint32_t writeSeq_ = 0;
};

} // namespace wab
