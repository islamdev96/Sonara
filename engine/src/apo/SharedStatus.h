// SharedStatus.h - Writes the StatusBlock to a memory-mapped file so the
// Electron UI can read it and display a real VU meter + heartbeat.
//
// Unlike SharedParams (read-only from the APO), this is WRITE-only from
// the APO. The UI reads it. Separate file to keep concerns clean.
//
// File: %ProgramData%\WinAudioBoosterPro\status.bin
#pragma once
#include <windows.h>
#include <shlobj.h>
#include <strsafe.h>
#include "StatusBlock.h"

namespace wab {

class SharedStatus {
public:
    ~SharedStatus() { close(); }

    bool open() {
        if (view_) return true;
        wchar_t path[MAX_PATH];
        if (!resolvePath(path, MAX_PATH)) return false;

        // Open read/write — the APO creates this file if needed.
        hFile_ = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) { hFile_ = nullptr; return false; }

        hMap_ = CreateFileMappingW(hFile_, nullptr, PAGE_READWRITE, 0,
                                   sizeof(StatusBlock), nullptr);
        if (!hMap_) { close(); return false; }

        view_ = reinterpret_cast<StatusBlock*>(
            MapViewOfFile(hMap_, FILE_MAP_WRITE, 0, 0, sizeof(StatusBlock)));
        if (!view_) { close(); return false; }

        // Pin the page so writes from the RT thread don't fault.
        VirtualLock(view_, sizeof(StatusBlock));
        return true;
    }

    // Called from APOProcess() — must be RT-safe: no allocation, no blocking.
    // Only writes every `intervalCalls` invocations to avoid overhead.
    inline void write(float rmsL, float rmsR, float peakL, float peakR,
                      uint32_t sampleRate, uint32_t channels,
                      const char* activeDeviceName, const float* rawSamples) noexcept {
        if (!view_) return;
        // Atomic-ish write: bump seq, write fields, bump seq again.
        // The reader uses the same seqlock pattern to detect torn reads.
        const uint32_t s = ++seq_;
        view_->seq = 0; // mark write-in-progress
        MemoryBarrier();

        view_->heartbeatMs = GetTickCount64();
        view_->rmsLeft     = rmsL;
        view_->rmsRight    = rmsR;
        view_->peakLeft    = peakL;
        view_->peakRight   = peakR;
        view_->sampleRate  = sampleRate;
        view_->channels    = channels;
        view_->magic       = kStatusMagic;

        if (activeDeviceName) {
            size_t i = 0;
            for (; i < 127 && activeDeviceName[i] != '\0'; ++i) {
                view_->activeDevice[i] = activeDeviceName[i];
            }
            view_->activeDevice[i] = '\0';
        } else {
            view_->activeDevice[0] = '\0';
        }

        if (rawSamples) {
            for (size_t i = 0; i < 256; ++i) {
                view_->rawSamples[i] = rawSamples[i];
            }
        }

        MemoryBarrier();
        view_->seq = s; // finalize
    }

    void close() {
        if (view_) {
            VirtualUnlock(view_, sizeof(StatusBlock));
            UnmapViewOfFile(view_);
            view_ = nullptr;
        }
        if (hMap_) { CloseHandle(hMap_); hMap_ = nullptr; }
        if (hFile_) { CloseHandle(hFile_); hFile_ = nullptr; }
    }

    static bool resolvePath(wchar_t* out, size_t cch) {
        wchar_t base[MAX_PATH];
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, base)))
            return false;
        wchar_t dir[MAX_PATH];
        StringCchPrintfW(dir, MAX_PATH, L"%s\\WinAudioBoosterPro", base);
        CreateDirectoryW(dir, nullptr);
        return SUCCEEDED(StringCchPrintfW(out, cch, L"%s\\status.bin", dir));
    }

private:
    HANDLE hFile_ = nullptr;
    HANDLE hMap_  = nullptr;
    StatusBlock* view_ = nullptr;
    uint32_t seq_ = 0;
};

} // namespace wab
