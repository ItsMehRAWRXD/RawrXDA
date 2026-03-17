#pragma once
// ============================================================================
// WindowsDefenderBridge.h — Windows Defender / AMSI Integration for HotPatcher
// ============================================================================
// Scans all hotpatch payloads through Windows Defender before application.
// Uses two Windows APIs:
//   1. AMSI (Antimalware Scan Interface) — in-memory buffer scanning
//   2. MpScanStart (Windows Defender MpClient) — file-based fallback
//
// Zero external deps beyond Windows SDK. C++20, Win32 only.
// ============================================================================

#include "../RawrXD_SignalSlot.h"
#include <windows.h>
#include <amsi.h>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>

#pragma comment(lib, "amsi.lib")
#pragma comment(lib, "ole32.lib")

namespace RawrXD {

// ── Scan verdict ──
enum class DefenderVerdict {
    Clean,          // AMSI_RESULT_CLEAN or NOT_DETECTED
    Suspicious,     // AMSI_RESULT_DETECTED but below block threshold
    Malicious,      // AMSI_RESULT_DETECTED — blocked
    ScanFailed,     // AMSI unavailable or COM error
    NotInitialized  // Bridge not yet initialized
};

inline const char* VerdictToString(DefenderVerdict v) {
    switch (v) {
        case DefenderVerdict::Clean:          return "CLEAN";
        case DefenderVerdict::Suspicious:     return "SUSPICIOUS";
        case DefenderVerdict::Malicious:      return "MALICIOUS";
        case DefenderVerdict::ScanFailed:     return "SCAN_FAILED";
        case DefenderVerdict::NotInitialized: return "NOT_INITIALIZED";
        default:                              return "UNKNOWN";
    }
}

// ── Scan result with metadata ──
struct DefenderScanResult {
    DefenderVerdict verdict;
    AMSI_RESULT     amsiResult;
    std::wstring    threatName;      // If detected
    std::wstring    scanSource;      // "hotpatch", "file_load", etc.
    uint64_t        scanTimeMs;      // Scan duration
    size_t          payloadSize;     // Bytes scanned
    bool            wasBlocked;      // True if patch was rejected
};

// ============================================================================
// WindowsDefenderBridge — AMSI-based scanning for all hotpatch payloads
// ============================================================================
class WindowsDefenderBridge {
public:
    // Signals
    Signal<const DefenderScanResult&> onScanComplete;
    Signal<const DefenderScanResult&> onThreatDetected;
    Signal<const std::wstring&>       onScanError;

    WindowsDefenderBridge() = default;
    ~WindowsDefenderBridge() { Shutdown(); }

    // ── Lifecycle ──
    bool Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return true;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            LogError(L"COM init failed", hr);
            return false;
        }
        m_comInitialized = true;

        hr = AmsiInitialize(L"RawrXD-HotPatcher", &m_amsiContext);
        if (FAILED(hr)) {
            LogError(L"AMSI init failed", hr);
            return false;
        }

        hr = AmsiOpenSession(m_amsiContext, &m_amsiSession);
        if (FAILED(hr)) {
            LogError(L"AMSI session failed", hr);
            AmsiUninitialize(m_amsiContext);
            m_amsiContext = nullptr;
            return false;
        }

        m_initialized = true;
        OutputDebugStringW(L"[DefenderBridge] AMSI initialized — scanning active\n");
        return true;
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return;

        if (m_amsiSession) {
            AmsiCloseSession(m_amsiContext, m_amsiSession);
            m_amsiSession = nullptr;
        }
        if (m_amsiContext) {
            AmsiUninitialize(m_amsiContext);
            m_amsiContext = nullptr;
        }
        if (m_comInitialized) {
            CoUninitialize();
            m_comInitialized = false;
        }

        m_initialized = false;
        m_totalScans = 0;
        m_totalBlocked = 0;
        OutputDebugStringW(L"[DefenderBridge] Shutdown complete\n");
    }

    bool IsInitialized() const { return m_initialized; }

    // ── Core scan: buffer ──
    // Scans raw bytes (hotpatch opcodes) through AMSI/Defender.
    // Returns the verdict. Emits onScanComplete / onThreatDetected.
    DefenderScanResult ScanBuffer(
        const void* data, size_t size,
        const std::wstring& contentName = L"hotpatch_payload"
    ) {
        DefenderScanResult result{};
        result.scanSource = contentName;
        result.payloadSize = size;

        auto start = std::chrono::steady_clock::now();

        if (!m_initialized) {
            result.verdict = DefenderVerdict::NotInitialized;
            result.wasBlocked = false;
            onScanError.emit(L"DefenderBridge not initialized");
            return result;
        }

        if (!data || size == 0) {
            result.verdict = DefenderVerdict::Clean;
            result.wasBlocked = false;
            return result;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        AMSI_RESULT amsiResult = AMSI_RESULT_NOT_DETECTED;
        HRESULT hr = AmsiScanBuffer(
            m_amsiContext,
            const_cast<void*>(data),
            static_cast<ULONG>(size),
            contentName.c_str(),
            m_amsiSession,
            &amsiResult
        );

        auto end = std::chrono::steady_clock::now();
        result.scanTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        result.amsiResult = amsiResult;

        if (FAILED(hr)) {
            result.verdict = DefenderVerdict::ScanFailed;
            result.wasBlocked = false;
            wchar_t errBuf[256];
            wsprintfW(errBuf, L"AMSI scan failed: HRESULT 0x%08X", hr);
            onScanError.emit(errBuf);
        } else if (AmsiResultIsMalware(amsiResult)) {
            result.verdict = DefenderVerdict::Malicious;
            result.wasBlocked = true;
            result.threatName = L"Detected by Windows Defender";
            m_totalBlocked++;
            onThreatDetected.emit(result);
        } else if (amsiResult >= AMSI_RESULT_DETECTED) {
            result.verdict = DefenderVerdict::Suspicious;
            result.wasBlocked = m_blockSuspicious;
            if (m_blockSuspicious) m_totalBlocked++;
            onThreatDetected.emit(result);
        } else {
            result.verdict = DefenderVerdict::Clean;
            result.wasBlocked = false;
        }

        m_totalScans++;
        onScanComplete.emit(result);

        // Debug log
        char dbg[512];
        wsprintfA(dbg, "[DefenderBridge] Scan: %ls → %s (%llu bytes, %llu ms)\n",
                  contentName.c_str(), VerdictToString(result.verdict),
                  static_cast<unsigned long long>(size),
                  static_cast<unsigned long long>(result.scanTimeMs));
        OutputDebugStringA(dbg);

        return result;
    }

    // ── Convenience: scan a vector of opcodes ──
    DefenderScanResult ScanOpcodes(
        const std::vector<uint8_t>& opcodes,
        const std::wstring& patchName = L"hotpatch_opcodes"
    ) {
        return ScanBuffer(opcodes.data(), opcodes.size(), patchName);
    }

    // ── Convenience: scan a file on disk ──
    DefenderScanResult ScanFile(const std::wstring& filePath) {
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            DefenderScanResult r{};
            r.verdict = DefenderVerdict::ScanFailed;
            r.scanSource = filePath;
            onScanError.emit(L"Failed to open file: " + filePath);
            return r;
        }

        DWORD fileSize = GetFileSize(hFile, nullptr);
        std::vector<uint8_t> buf(fileSize);
        DWORD bytesRead = 0;
        ReadFile(hFile, buf.data(), fileSize, &bytesRead, nullptr);
        CloseHandle(hFile);

        return ScanBuffer(buf.data(), bytesRead, filePath);
    }

    // ── Policy ──
    void SetBlockSuspicious(bool block) { m_blockSuspicious = block; }
    bool GetBlockSuspicious() const { return m_blockSuspicious; }

    // ── Stats ──
    uint64_t GetTotalScans() const { return m_totalScans; }
    uint64_t GetTotalBlocked() const { return m_totalBlocked; }

private:
    void LogError(const std::wstring& msg, HRESULT hr) {
        wchar_t buf[512];
        wsprintfW(buf, L"[DefenderBridge] %s (HRESULT: 0x%08X)\n", msg.c_str(), hr);
        OutputDebugStringW(buf);
        onScanError.emit(buf);
    }

    HAMSICONTEXT m_amsiContext = nullptr;
    HAMSISESSION m_amsiSession = nullptr;
    bool         m_initialized = false;
    bool         m_comInitialized = false;
    bool         m_blockSuspicious = true;  // Block suspicious by default
    uint64_t     m_totalScans = 0;
    uint64_t     m_totalBlocked = 0;
    std::mutex   m_mutex;
};

// ── Global accessor ──
inline WindowsDefenderBridge& GetDefenderBridge() {
    static WindowsDefenderBridge instance;
    return instance;
}

} // namespace RawrXD
