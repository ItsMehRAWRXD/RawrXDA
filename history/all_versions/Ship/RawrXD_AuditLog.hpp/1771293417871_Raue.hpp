// RawrXD_AuditLog.hpp - Tamper-Evident Audit Logging System
// Pure C++20 - No Qt Dependencies
// Features: Structured event logging, HMAC chain integrity, file + memory ring buffer,
//           log rotation, export to JSON, real-time filtering, severity-based routing

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <fstream>
#include <sstream>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <iomanip>
#include <ctime>

#pragma comment(lib, "bcrypt.lib")

namespace RawrXD {
namespace Security {

// ============================================================================
// Audit Event Severity & Category
// ============================================================================
enum class AuditSeverity : uint8_t {
    Debug    = 0,
    Info     = 1,
    Warning  = 2,
    Error    = 3,
    Critical = 4,
    Alert    = 5   // Requires immediate human attention
};

enum class AuditCategory : uint16_t {
    Authentication   = 0x0001,
    Authorization    = 0x0002,
    Encryption       = 0x0004,
    KeyManagement    = 0x0008,
    CredentialAccess = 0x0010,
    NetworkSecurity  = 0x0020,
    CertPinning      = 0x0040,
    SandboxEvent     = 0x0080,
    PromptInjection  = 0x0100,
    PIIRedaction     = 0x0200,
    TamperDetection  = 0x0400,
    BootChain        = 0x0800,
    Configuration    = 0x1000,
    DataAccess       = 0x2000,
    SystemEvent      = 0x4000,
    All              = 0xFFFF
};

inline AuditCategory operator|(AuditCategory a, AuditCategory b) {
    return static_cast<AuditCategory>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline bool operator&(AuditCategory a, AuditCategory b) {
    return (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0;
}

// ============================================================================
// Audit Event Record
// ============================================================================
struct AuditEvent {
    uint64_t       sequenceId    = 0;        // Monotonic sequence number
    uint64_t       timestampMs   = 0;        // Unix epoch milliseconds
    AuditSeverity  severity      = AuditSeverity::Info;
    AuditCategory  category      = AuditCategory::SystemEvent;
    std::string    actor;                    // User/service/process performing action
    std::string    action;                   // Operation performed (verb)
    std::string    resource;                 // Target of the action
    bool           success       = true;
    std::string    detail;                   // Free-form detail/context
    std::string    sourceIP;                 // Network origin if applicable
    std::string    sessionId;                // Session correlation ID
    std::string    chainHash;                // HMAC of previous event + this event (tamper-evident)

    // ---- Serialization ----
    std::string ToJSON() const {
        std::ostringstream o;
        o << "{\"seq\":" << sequenceId
          << ",\"ts\":" << timestampMs
          << ",\"sev\":" << static_cast<int>(severity)
          << ",\"cat\":" << static_cast<int>(category)
          << ",\"actor\":\"" << EscapeJSON(actor) << "\""
          << ",\"action\":\"" << EscapeJSON(action) << "\""
          << ",\"resource\":\"" << EscapeJSON(resource) << "\""
          << ",\"ok\":" << (success ? "true" : "false")
          << ",\"detail\":\"" << EscapeJSON(detail) << "\""
          << ",\"srcIP\":\"" << EscapeJSON(sourceIP) << "\""
          << ",\"sid\":\"" << EscapeJSON(sessionId) << "\""
          << ",\"chain\":\"" << EscapeJSON(chainHash) << "\""
          << "}";
        return o.str();
    }

    static std::string SeverityStr(AuditSeverity s) {
        switch (s) {
            case AuditSeverity::Debug:    return "DEBUG";
            case AuditSeverity::Info:     return "INFO";
            case AuditSeverity::Warning:  return "WARN";
            case AuditSeverity::Error:    return "ERROR";
            case AuditSeverity::Critical: return "CRITICAL";
            case AuditSeverity::Alert:    return "ALERT";
            default: return "UNKNOWN";
        }
    }

private:
    static std::string EscapeJSON(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:   out += c; break;
            }
        }
        return out;
    }
};

// ============================================================================
// Audit Log Listener (observer callback)
// ============================================================================
using AuditListener = std::function<void(const AuditEvent&)>;

// ============================================================================
// AuditLogger - Thread-safe, tamper-evident audit logging engine
// ============================================================================
class AuditLogger {
public:
    struct Config {
        size_t      ringBufferSize      = 10000;      // Max events in memory ring
        size_t      maxFileSize          = 10 * 1024 * 1024;  // 10 MB per log file
        int         maxRotatedFiles      = 10;         // Keep N rotated logs
        std::string logDirectory;                      // Directory for log files (empty = memory only)
        std::string logFilePrefix        = "rawrxd_audit";
        AuditSeverity minFileSeverity    = AuditSeverity::Info;
        AuditSeverity minMemorySeverity  = AuditSeverity::Debug;
        bool        enableChainHMAC      = true;       // Tamper-evident HMAC chain
        bool        enableConsoleOutput  = false;
        bool        asyncFlush           = true;       // Background thread flushes to disk
    };

    AuditLogger() {
        m_running.store(false);
        m_sequenceCounter.store(0);
        m_currentFileSize.store(0);
        // Initialize HMAC chain seed with random bytes
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, nullptr, 0))) {
            m_hmacKey.resize(32);
            BCryptGenRandom(hAlg, m_hmacKey.data(), 32, 0);
            BCryptCloseAlgorithmProvider(hAlg, 0);
        }
        m_lastChainHash = "GENESIS";
    }

    ~AuditLogger() {
        Shutdown();
    }

    // ---- Lifecycle ----
    bool Initialize(const Config& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;

        // Create log directory if needed
        if (!m_config.logDirectory.empty()) {
            std::wstring wdir(m_config.logDirectory.begin(), m_config.logDirectory.end());
            CreateDirectoryW(wdir.c_str(), nullptr);

            // Open initial log file
            if (!OpenLogFile()) return false;
        }

        if (m_config.asyncFlush) {
            m_running.store(true);
            m_flushThread = std::thread(&AuditLogger::FlushWorker, this);
        }

        LogInternal(AuditSeverity::Info, AuditCategory::SystemEvent,
                    "AuditLogger", "initialize", "audit_system", true,
                    "Audit logging started. Chain HMAC: " + std::string(m_config.enableChainHMAC ? "enabled" : "disabled"));
        return true;
    }

    void Shutdown() {
        if (m_running.exchange(false)) {
            m_flushCV.notify_all();
            if (m_flushThread.joinable()) m_flushThread.join();
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile.is_open()) {
            m_logFile.flush();
            m_logFile.close();
        }
    }

    // ---- Primary Logging Interface ----
    uint64_t Log(AuditSeverity severity, AuditCategory category,
                 const std::string& actor, const std::string& action,
                 const std::string& resource, bool success,
                 const std::string& detail = "",
                 const std::string& sourceIP = "",
                 const std::string& sessionId = "") {
        return LogInternal(severity, category, actor, action, resource, success, detail, sourceIP, sessionId);
    }

    // ---- Convenience Helpers ----
    uint64_t LogAuthSuccess(const std::string& actor, const std::string& method, const std::string& sessionId = "") {
        return Log(AuditSeverity::Info, AuditCategory::Authentication,
                   actor, "authenticate", method, true, "Authentication succeeded", "", sessionId);
    }

    uint64_t LogAuthFailure(const std::string& actor, const std::string& method, const std::string& reason, const std::string& srcIP = "") {
        return Log(AuditSeverity::Warning, AuditCategory::Authentication,
                   actor, "authenticate", method, false, "Authentication failed: " + reason, srcIP);
    }

    uint64_t LogAccessDenied(const std::string& actor, const std::string& resource, const std::string& reason = "") {
        return Log(AuditSeverity::Warning, AuditCategory::Authorization,
                   actor, "access", resource, false, "Access denied" + (reason.empty() ? "" : ": " + reason));
    }

    uint64_t LogKeyOperation(const std::string& actor, const std::string& op, const std::string& keyId, bool success) {
        return Log(success ? AuditSeverity::Info : AuditSeverity::Error, AuditCategory::KeyManagement,
                   actor, op, keyId, success);
    }

    uint64_t LogTamperDetected(const std::string& component, const std::string& detail) {
        return Log(AuditSeverity::Alert, AuditCategory::TamperDetection,
                   "system", "tamper_detected", component, false, detail);
    }

    uint64_t LogPromptInjection(const std::string& actor, const std::string& pattern, const std::string& snippet) {
        return Log(AuditSeverity::Warning, AuditCategory::PromptInjection,
                   actor, "prompt_injection_blocked", pattern, false, "Blocked snippet: " + snippet);
    }

    uint64_t LogPIIRedaction(const std::string& actor, const std::string& fieldType, int count) {
        return Log(AuditSeverity::Info, AuditCategory::PIIRedaction,
                   actor, "pii_redacted", fieldType, true, "Redacted " + std::to_string(count) + " instances");
    }

    // ---- Query ----
    std::vector<AuditEvent> GetRecentEvents(size_t limit = 100, AuditCategory filter = AuditCategory::All) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<AuditEvent> result;
        result.reserve(std::min(limit, m_ringBuffer.size()));
        size_t count = 0;
        for (auto it = m_ringBuffer.rbegin(); it != m_ringBuffer.rend() && count < limit; ++it) {
            if (filter & it->category) {
                result.push_back(*it);
                ++count;
            }
        }
        return result;
    }

    size_t GetEventCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_ringBuffer.size();
    }

    uint64_t GetTotalEventsLogged() const {
        return m_sequenceCounter.load();
    }

    // ---- Export ----
    bool ExportToJSON(const std::string& filePath, size_t limit = 0) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream out(filePath, std::ios::trunc);
        if (!out.is_open()) return false;

        out << "[\n";
        size_t count = 0;
        for (auto it = m_ringBuffer.begin(); it != m_ringBuffer.end(); ++it) {
            if (limit > 0 && count >= limit) break;
            if (count > 0) out << ",\n";
            out << "  " << it->ToJSON();
            ++count;
        }
        out << "\n]\n";
        return true;
    }

    // ---- Verify Chain Integrity ----
    bool VerifyChainIntegrity() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_ringBuffer.empty()) return true;

        std::string prevHash = "GENESIS";
        for (const auto& evt : m_ringBuffer) {
            std::string expected = ComputeChainHash(prevHash, evt);
            if (evt.chainHash != expected) return false;
            prevHash = evt.chainHash;
        }
        return true;
    }

    // ---- Listeners ----
    void AddListener(AuditListener listener) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.push_back(std::move(listener));
    }

private:
    uint64_t LogInternal(AuditSeverity severity, AuditCategory category,
                         const std::string& actor, const std::string& action,
                         const std::string& resource, bool success,
                         const std::string& detail = "",
                         const std::string& sourceIP = "",
                         const std::string& sessionId = "") {
        AuditEvent evt;
        evt.sequenceId = m_sequenceCounter.fetch_add(1);
        evt.timestampMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        evt.severity  = severity;
        evt.category  = category;
        evt.actor     = actor;
        evt.action    = action;
        evt.resource  = resource;
        evt.success   = success;
        evt.detail    = detail;
        evt.sourceIP  = sourceIP;
        evt.sessionId = sessionId;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // HMAC chain
            if (m_config.enableChainHMAC) {
                evt.chainHash = ComputeChainHash(m_lastChainHash, evt);
                m_lastChainHash = evt.chainHash;
            }

            // Add to ring buffer
            m_ringBuffer.push_back(evt);
            if (m_ringBuffer.size() > m_config.ringBufferSize) {
                m_ringBuffer.pop_front();
            }

            // Synchronous file write if not async
            if (!m_config.asyncFlush && m_logFile.is_open() && severity >= m_config.minFileSeverity) {
                WriteEventToFile(evt);
            }

            // Pending for async flush
            if (m_config.asyncFlush && severity >= m_config.minFileSeverity) {
                m_pendingFlush.push_back(evt);
            }

            // Console output
            if (m_config.enableConsoleOutput) {
                OutputDebugStringA(("[AUDIT] " + AuditEvent::SeverityStr(severity) + " " + action + " by " + actor + "\n").c_str());
            }

            // Notify listeners
            for (auto& listener : m_listeners) {
                try { listener(evt); } catch (...) {}
            }
        }

        if (m_config.asyncFlush) {
            m_flushCV.notify_one();
        }

        return evt.sequenceId;
    }

    // ---- HMAC Chain ----
    std::string ComputeChainHash(const std::string& prevHash, const AuditEvent& evt) const {
        // Build payload: previous hash + event unique data
        std::string payload = prevHash + "|" + std::to_string(evt.sequenceId) + "|"
            + std::to_string(evt.timestampMs) + "|" + evt.actor + "|" + evt.action
            + "|" + evt.resource + "|" + (evt.success ? "1" : "0") + "|" + evt.detail;

        return HMAC_SHA256(payload);
    }

    std::string HMAC_SHA256(const std::string& data) const {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        std::string result;

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
            return "HMAC_FAIL";

        DWORD hashObjSize = 0, hashSize = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize, sizeof(DWORD), &cbResult, 0);
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashSize, sizeof(DWORD), &cbResult, 0);

        std::vector<UCHAR> hashObj(hashObjSize);
        std::vector<UCHAR> hashVal(hashSize);

        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjSize,
                (PUCHAR)m_hmacKey.data(), (ULONG)m_hmacKey.size(), 0))) {
            BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0);
            BCryptFinishHash(hHash, hashVal.data(), hashSize, 0);
            BCryptDestroyHash(hHash);

            // Hex encode
            static const char hex[] = "0123456789abcdef";
            result.reserve(hashSize * 2);
            for (DWORD i = 0; i < hashSize; ++i) {
                result += hex[hashVal[i] >> 4];
                result += hex[hashVal[i] & 0x0F];
            }
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return result.empty() ? "HMAC_FAIL" : result;
    }

    // ---- File I/O ----
    bool OpenLogFile() {
        std::string path = m_config.logDirectory + "\\" + m_config.logFilePrefix + ".jsonl";
        m_logFile.open(path, std::ios::app);
        m_currentFilePath = path;
        if (m_logFile.is_open()) {
            m_logFile.seekp(0, std::ios::end);
            m_currentFileSize.store(static_cast<size_t>(m_logFile.tellp()));
        }
        return m_logFile.is_open();
    }

    void WriteEventToFile(const AuditEvent& evt) {
        std::string line = evt.ToJSON() + "\n";
        m_logFile << line;
        m_logFile.flush();
        m_currentFileSize.fetch_add(line.size());

        if (m_currentFileSize.load() >= m_config.maxFileSize) {
            RotateLogFile();
        }
    }

    void RotateLogFile() {
        m_logFile.close();

        // Rename current -> .1, .1 -> .2, etc.
        for (int i = m_config.maxRotatedFiles - 1; i >= 1; --i) {
            std::string src = m_config.logDirectory + "\\" + m_config.logFilePrefix + "." + std::to_string(i) + ".jsonl";
            std::string dst = m_config.logDirectory + "\\" + m_config.logFilePrefix + "." + std::to_string(i + 1) + ".jsonl";
            std::wstring wsrc(src.begin(), src.end());
            std::wstring wdst(dst.begin(), dst.end());
            MoveFileExW(wsrc.c_str(), wdst.c_str(), MOVEFILE_REPLACE_EXISTING);
        }

        std::string rotated = m_config.logDirectory + "\\" + m_config.logFilePrefix + ".1.jsonl";
        std::wstring wcur(m_currentFilePath.begin(), m_currentFilePath.end());
        std::wstring wrot(rotated.begin(), rotated.end());
        MoveFileExW(wcur.c_str(), wrot.c_str(), MOVEFILE_REPLACE_EXISTING);

        m_currentFileSize.store(0);
        OpenLogFile();
    }

    void FlushWorker() {
        while (m_running.load()) {
            std::vector<AuditEvent> batch;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_flushCV.wait_for(lock, std::chrono::milliseconds(500),
                    [this] { return !m_pendingFlush.empty() || !m_running.load(); });
                batch.swap(m_pendingFlush);
            }

            if (!batch.empty() && m_logFile.is_open()) {
                for (const auto& evt : batch) {
                    WriteEventToFile(evt);
                }
            }
        }

        // Final flush
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile.is_open()) {
            for (const auto& evt : m_pendingFlush) {
                WriteEventToFile(evt);
            }
            m_pendingFlush.clear();
        }
    }

    // ---- State ----
    mutable std::mutex   m_mutex;
    Config               m_config;
    std::deque<AuditEvent> m_ringBuffer;
    std::vector<AuditEvent> m_pendingFlush;
    std::atomic<uint64_t> m_sequenceCounter;
    std::atomic<size_t>   m_currentFileSize;
    std::string          m_currentFilePath;
    std::ofstream        m_logFile;
    std::vector<UCHAR>   m_hmacKey;
    std::string          m_lastChainHash;
    std::vector<AuditListener> m_listeners;
    std::atomic<bool>    m_running;
    std::thread          m_flushThread;
    std::condition_variable m_flushCV;
};

} // namespace Security
} // namespace RawrXD
