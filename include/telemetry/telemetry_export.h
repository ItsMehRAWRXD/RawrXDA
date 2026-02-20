// ============================================================================
// telemetry_export.h — Telemetry Export System
// ============================================================================
// Multi-format telemetry export: OTLP, CSV, JSONL, Prometheus push,
// and pluggable export backends via C-ABI DLL interface.
//
// Extends UnifiedTelemetryCore with:
//   - Scheduled auto-export to configured backends
//   - OTLP/gRPC-compatible trace/metric serialization
//   - CSV bulk export for data analysis
//   - Pluggable exporter interface (DLL-based)
//   - Audit log export with chain-of-custody hashing
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "UnifiedTelemetryCore.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Telemetry {

// ============================================================================
// Export format types
// ============================================================================
enum class ExportFormat {
    JSON,               // Full JSON with metadata
    JSONL,              // Newline-delimited JSON (streaming)
    CSV,                // Tabular CSV
    Prometheus,         // Prometheus text exposition format
    OTLP_JSON,          // OpenTelemetry Protocol JSON
    OTLP_PROTOBUF,      // OpenTelemetry Protocol Protobuf (binary)
    Custom              // Via pluggable exporter DLL
};

// ============================================================================
// Export destination configuration
// ============================================================================
struct ExportDestination {
    std::string     name;           // Human-readable name
    ExportFormat    format;         // Export format
    std::string     endpoint;       // URL or file path
    std::string     apiKey;         // Optional auth key
    int             batchSize;      // Events per batch (0 = all)
    int             intervalMs;     // Auto-export interval (0 = manual only)
    bool            enabled;        // Is this exporter active?
    
    // OTLP-specific
    std::string     serviceName;    // OTLP service.name
    std::string     serviceVersion; // OTLP service.version
    std::map<std::string, std::string> resourceAttributes; // OTLP resource attrs
};

// ============================================================================
// Export result
// ============================================================================
struct ExportResult {
    bool        success;
    int         eventsExported;
    int64_t     bytesWritten;
    std::string error;
    std::string detail;
    
    static ExportResult ok(int events, int64_t bytes) {
        return {true, events, bytes, "", ""};
    }
    static ExportResult fail(const std::string& err) {
        return {false, 0, 0, err, ""};
    }
};

// ============================================================================
// Audit log entry for chain-of-custody export
// ============================================================================
struct AuditExportEntry {
    uint64_t    sequenceId;
    uint64_t    timestampMs;
    std::string actor;          // Who triggered the action
    std::string action;         // What was done
    std::string target;         // What was affected
    std::string detail;         // Additional context
    uint64_t    chainHash;      // SHA256 chain link (prev hash XOR data hash)
    
    // Tamper-evident: each entry's hash depends on the previous
    bool verifyChain(uint64_t previousHash) const;
};

// ============================================================================
// Pluggable Exporter C-ABI Interface (for DLL export plugins)
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

// C-ABI event struct for plugin boundary (no STL)
typedef struct TelemetryExportEvent {
    uint64_t    id;
    uint64_t    timestampMs;
    int         level;          // TelemetryLevel as int
    int         source;         // TelemetrySource as int
    const char* category;
    const char* message;
    double      valueNumeric;
    const char* unit;
    int64_t     durationMs;
    // Tags serialized as "key1=val1;key2=val2"
    const char* tagsSerialized;
} TelemetryExportEvent;

// Plugin exports these functions:
typedef struct ExporterPluginInfo {
    char name[64];
    char version[32];
    char description[256];
    char supportedFormats[128];  // Comma-separated format names
} ExporterPluginInfo;

typedef ExporterPluginInfo* (*ExporterPlugin_GetInfo_fn)(void);
typedef int  (*ExporterPlugin_Init_fn)(const char* configJson);
typedef int  (*ExporterPlugin_Export_fn)(const TelemetryExportEvent* events, int count);
typedef void (*ExporterPlugin_Shutdown_fn)(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// TelemetryExporter — Multi-format export engine
// ============================================================================
class TelemetryExporter {
public:
    static TelemetryExporter& Instance();

    // ---- Lifecycle ----
    void Initialize(UnifiedTelemetryCore* core);
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }

    // ---- Destination Management ----
    int  AddDestination(const ExportDestination& dest);
    bool RemoveDestination(int id);
    bool EnableDestination(int id, bool enabled);
    std::vector<ExportDestination> GetDestinations() const;

    // ---- Manual Export ----
    ExportResult ExportNow(int destinationId = -1);     // -1 = all destinations
    ExportResult ExportToFile(const std::string& path, ExportFormat format,
                              size_t maxEvents = 0);
    ExportResult ExportToEndpoint(const std::string& url, ExportFormat format,
                                  const std::string& apiKey = "");

    // ---- Format-Specific Export ----
    std::string FormatAsJSON(const std::vector<TelemetryEvent>& events) const;
    std::string FormatAsJSONL(const std::vector<TelemetryEvent>& events) const;
    std::string FormatAsCSV(const std::vector<TelemetryEvent>& events) const;
    std::string FormatAsOTLP(const std::vector<TelemetryEvent>& events,
                             const ExportDestination& dest) const;
    std::string FormatAsPrometheus() const;

    // ---- Audit Export ----
    ExportResult ExportAuditLog(const std::string& path,
                                uint64_t fromSequence = 0,
                                uint64_t toSequence = UINT64_MAX);
    void RecordAuditEntry(const std::string& actor, const std::string& action,
                          const std::string& target, const std::string& detail = "");
    std::vector<AuditExportEntry> GetAuditEntries(size_t maxCount = 100) const;
    bool VerifyAuditChain() const;

    // ---- Plugin Exporter ----
    bool LoadExporterPlugin(const std::string& dllPath);
    void UnloadExporterPlugins();

    // ---- Auto-Export Timer ----
    void StartAutoExport();
    void StopAutoExport();
    bool IsAutoExportRunning() const { return m_autoExportRunning.load(); }

    // ---- Statistics ----
    struct ExportStats {
        uint64_t totalExports;
        uint64_t totalEventsExported;
        uint64_t totalBytesWritten;
        uint64_t failedExports;
        uint64_t lastExportTimestampMs;
    };
    ExportStats GetStats() const;

private:
    TelemetryExporter() = default;
    ~TelemetryExporter();

    // Internal export helpers
    ExportResult doExport(const ExportDestination& dest,
                          const std::vector<TelemetryEvent>& events);
    ExportResult doFileExport(const std::string& path, const std::string& content);
    ExportResult doHttpExport(const std::string& url, const std::string& content,
                              const std::string& apiKey, const std::string& contentType);
    
    // OTLP trace span builder
    std::string buildOTLPSpan(const TelemetryEvent& event,
                              const ExportDestination& dest) const;
    
    // Tag serialization for C-ABI
    static std::string serializeTags(const std::map<std::string, std::string>& tags);
    static std::map<std::string, std::string> deserializeTags(const std::string& s);

    // Audit chain hash
    uint64_t computeChainHash(const AuditExportEntry& entry, uint64_t prevHash) const;

    UnifiedTelemetryCore*           m_core = nullptr;
    std::atomic<bool>               m_initialized{false};
    std::atomic<bool>               m_autoExportRunning{false};
    
    mutable std::mutex              m_destMutex;
    std::map<int, ExportDestination> m_destinations;
    int                             m_nextDestId = 1;

    mutable std::mutex              m_auditMutex;
    std::vector<AuditExportEntry>   m_auditLog;
    uint64_t                        m_auditSequence = 0;
    uint64_t                        m_lastAuditHash = 0;

    mutable std::mutex              m_statsMutex;
    ExportStats                     m_stats{};

    // Plugin exporters
    struct LoadedExporter {
        std::string                 path;
        ExporterPlugin_GetInfo_fn   fnGetInfo = nullptr;
        ExporterPlugin_Init_fn      fnInit = nullptr;
        ExporterPlugin_Export_fn    fnExport = nullptr;
        ExporterPlugin_Shutdown_fn  fnShutdown = nullptr;
#ifdef _WIN32
        HMODULE                     hModule = nullptr;
#else
        void*                       hModule = nullptr;
#endif
    };
    std::vector<LoadedExporter>     m_pluginExporters;

    // Auto-export thread
#ifdef _WIN32
    HANDLE                          m_hAutoExportThread = nullptr;
    static DWORD WINAPI AutoExportThreadProc(LPVOID param);
#endif
};

} // namespace Telemetry
} // namespace RawrXD
