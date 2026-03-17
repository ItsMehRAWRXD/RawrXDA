// ============================================================================
// telemetry_export.cpp — Multi-Format Telemetry Export Implementation
// ============================================================================
// Implements OTLP, CSV, JSONL, Prometheus push, audit export with
// chain-of-custody hashing, and pluggable DLL exporter backends.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "telemetry/telemetry_export.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Telemetry {

// ============================================================================
// Singleton
// ============================================================================
TelemetryExporter& TelemetryExporter::Instance() {
    static TelemetryExporter instance;
    return instance;
}

TelemetryExporter::~TelemetryExporter() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
void TelemetryExporter::Initialize(UnifiedTelemetryCore* core) {
    if (m_initialized.load()) return;
    m_core = core;
    m_stats = {};
    m_initialized.store(true);
}

void TelemetryExporter::Shutdown() {
    if (!m_initialized.load()) return;
    StopAutoExport();
    UnloadExporterPlugins();
    m_initialized.store(false);
}

// ============================================================================
// Destination Management
// ============================================================================
int TelemetryExporter::AddDestination(const ExportDestination& dest) {
    std::lock_guard<std::mutex> lock(m_destMutex);
    int id = m_nextDestId++;
    m_destinations[id] = dest;
    return id;
}

bool TelemetryExporter::RemoveDestination(int id) {
    std::lock_guard<std::mutex> lock(m_destMutex);
    return m_destinations.erase(id) > 0;
}

bool TelemetryExporter::EnableDestination(int id, bool enabled) {
    std::lock_guard<std::mutex> lock(m_destMutex);
    auto it = m_destinations.find(id);
    if (it == m_destinations.end()) return false;
    it->second.enabled = enabled;
    return true;
}

std::vector<ExportDestination> TelemetryExporter::GetDestinations() const {
    std::lock_guard<std::mutex> lock(m_destMutex);
    std::vector<ExportDestination> out;
    for (const auto& [id, dest] : m_destinations) {
        out.push_back(dest);
    }
    return out;
}

// ============================================================================
// Manual Export
// ============================================================================
ExportResult TelemetryExporter::ExportNow(int destinationId) {
    if (!m_core) return ExportResult::fail("Core not initialized");

    auto events = m_core->GetRecentEvents(TELEMETRY_RING_SIZE);
    if (events.empty()) return ExportResult::ok(0, 0);

    int totalEvents = 0;
    int64_t totalBytes = 0;

    std::lock_guard<std::mutex> lock(m_destMutex);
    for (auto& [id, dest] : m_destinations) {
        if (!dest.enabled) continue;
        if (destinationId >= 0 && id != destinationId) continue;

        auto result = doExport(dest, events);
        if (result.success) {
            totalEvents += result.eventsExported;
            totalBytes += result.bytesWritten;
        }
    }

    // Update stats
    {
        std::lock_guard<std::mutex> slock(m_statsMutex);
        m_stats.totalExports++;
        m_stats.totalEventsExported += totalEvents;
        m_stats.totalBytesWritten += totalBytes;
        m_stats.lastExportTimestampMs = UnifiedTelemetryCore::NowMs();
    }

    return ExportResult::ok(totalEvents, totalBytes);
}

ExportResult TelemetryExporter::ExportToFile(const std::string& path,
                                              ExportFormat format,
                                              size_t maxEvents) {
    if (!m_core) return ExportResult::fail("Core not initialized");

    auto events = m_core->GetRecentEvents(maxEvents > 0 ? maxEvents : TELEMETRY_RING_SIZE);
    std::string content;

    switch (format) {
        case ExportFormat::JSON:       content = FormatAsJSON(events); break;
        case ExportFormat::JSONL:      content = FormatAsJSONL(events); break;
        case ExportFormat::CSV:        content = FormatAsCSV(events); break;
        case ExportFormat::Prometheus: content = FormatAsPrometheus(); break;
        case ExportFormat::OTLP_JSON: {
            ExportDestination dummy;
            dummy.serviceName = "rawrxd-ide";
            dummy.serviceVersion = "1.0.0";
            content = FormatAsOTLP(events, dummy);
            break;
        }
        default: return ExportResult::fail("Unsupported format for file export");
    }

    return doFileExport(path, content);
}

ExportResult TelemetryExporter::ExportToEndpoint(const std::string& url,
                                                   ExportFormat format,
                                                   const std::string& apiKey) {
    if (!m_core) return ExportResult::fail("Core not initialized");

    auto events = m_core->GetRecentEvents(TELEMETRY_RING_SIZE);
    std::string content;
    std::string contentType = "application/json";

    switch (format) {
        case ExportFormat::JSON:
        case ExportFormat::JSONL:
            content = FormatAsJSON(events);
            break;
        case ExportFormat::OTLP_JSON: {
            ExportDestination dummy;
            dummy.serviceName = "rawrxd-ide";
            dummy.serviceVersion = "1.0.0";
            content = FormatAsOTLP(events, dummy);
            break;
        }
        case ExportFormat::Prometheus:
            content = FormatAsPrometheus();
            contentType = "text/plain; version=0.0.4";
            break;
        default:
            return ExportResult::fail("Unsupported format for endpoint export");
    }

    return doHttpExport(url, content, apiKey, contentType);
}

// ============================================================================
// Format: JSON
// ============================================================================
std::string TelemetryExporter::FormatAsJSON(const std::vector<TelemetryEvent>& events) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"exporter\": \"RawrXD-TelemetryExporter\",\n";
    ss << "  \"version\": \"1.0.0\",\n";
    ss << "  \"exportTimestamp\": " << UnifiedTelemetryCore::NowMs() << ",\n";
    ss << "  \"eventCount\": " << events.size() << ",\n";
    ss << "  \"events\": [\n";

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        ss << "    {";
        ss << "\"id\":" << e.id;
        ss << ",\"timestamp\":" << e.timestampMs;
        ss << ",\"level\":\"" << e.levelString() << "\"";
        ss << ",\"source\":\"" << e.sourceString() << "\"";
        ss << ",\"category\":\"" << e.category << "\"";
        ss << ",\"message\":\"" << e.message << "\"";
        if (e.valueNumeric != 0.0) {
            ss << ",\"value\":" << std::fixed << std::setprecision(4) << e.valueNumeric;
        }
        if (!e.unit.empty()) ss << ",\"unit\":\"" << e.unit << "\"";
        if (e.durationMs >= 0) ss << ",\"durationMs\":" << e.durationMs;
        if (!e.tags.empty()) {
            ss << ",\"tags\":{";
            bool first = true;
            for (const auto& [k, v] : e.tags) {
                if (!first) ss << ",";
                ss << "\"" << k << "\":\"" << v << "\"";
                first = false;
            }
            ss << "}";
        }
        ss << "}";
        if (i + 1 < events.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n}\n";
    return ss.str();
}

// ============================================================================
// Format: JSONL (newline-delimited JSON)
// ============================================================================
std::string TelemetryExporter::FormatAsJSONL(const std::vector<TelemetryEvent>& events) const {
    std::ostringstream ss;
    for (const auto& e : events) {
        ss << "{\"id\":" << e.id
           << ",\"ts\":" << e.timestampMs
           << ",\"level\":\"" << e.levelString() << "\""
           << ",\"source\":\"" << e.sourceString() << "\""
           << ",\"category\":\"" << e.category << "\""
           << ",\"message\":\"" << e.message << "\"";
        if (e.valueNumeric != 0.0)
            ss << ",\"value\":" << std::fixed << std::setprecision(4) << e.valueNumeric;
        if (!e.unit.empty()) ss << ",\"unit\":\"" << e.unit << "\"";
        if (e.durationMs >= 0) ss << ",\"duration_ms\":" << e.durationMs;
        if (!e.tags.empty()) {
            ss << ",\"tags\":{";
            bool first = true;
            for (const auto& [k, v] : e.tags) {
                if (!first) ss << ",";
                ss << "\"" << k << "\":\"" << v << "\"";
                first = false;
            }
            ss << "}";
        }
        ss << "}\n";
    }
    return ss.str();
}

// ============================================================================
// Format: CSV
// ============================================================================
std::string TelemetryExporter::FormatAsCSV(const std::vector<TelemetryEvent>& events) const {
    std::ostringstream ss;
    // Header
    ss << "id,timestamp_ms,level,source,category,message,value,unit,duration_ms,tags\n";
    
    for (const auto& e : events) {
        ss << e.id << ","
           << e.timestampMs << ","
           << e.levelString() << ","
           << e.sourceString() << ","
           << "\"" << e.category << "\","
           << "\"" << e.message << "\","
           << std::fixed << std::setprecision(4) << e.valueNumeric << ","
           << "\"" << e.unit << "\","
           << e.durationMs << ","
           << "\"" << serializeTags(e.tags) << "\"\n";
    }
    return ss.str();
}

// ============================================================================
// Format: OTLP JSON (OpenTelemetry Protocol)
// ============================================================================
std::string TelemetryExporter::FormatAsOTLP(const std::vector<TelemetryEvent>& events,
                                             const ExportDestination& dest) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"resourceSpans\": [{\n";
    ss << "    \"resource\": {\n";
    ss << "      \"attributes\": [\n";
    ss << "        {\"key\": \"service.name\", \"value\": {\"stringValue\": \""
       << (dest.serviceName.empty() ? "rawrxd-ide" : dest.serviceName) << "\"}},\n";
    ss << "        {\"key\": \"service.version\", \"value\": {\"stringValue\": \""
       << (dest.serviceVersion.empty() ? "1.0.0" : dest.serviceVersion) << "\"}},\n";
    ss << "        {\"key\": \"telemetry.sdk.name\", \"value\": {\"stringValue\": \"rawrxd-telemetry\"}},\n";
    ss << "        {\"key\": \"telemetry.sdk.language\", \"value\": {\"stringValue\": \"cpp\"}}";
    
    for (const auto& [k, v] : dest.resourceAttributes) {
        ss << ",\n        {\"key\": \"" << k << "\", \"value\": {\"stringValue\": \"" << v << "\"}}";
    }
    ss << "\n      ]\n";
    ss << "    },\n";
    ss << "    \"scopeSpans\": [{\n";
    ss << "      \"scope\": {\"name\": \"rawrxd.telemetry\", \"version\": \"1.0.0\"},\n";
    ss << "      \"spans\": [\n";

    for (size_t i = 0; i < events.size(); ++i) {
        ss << "        " << buildOTLPSpan(events[i], dest);
        if (i + 1 < events.size()) ss << ",";
        ss << "\n";
    }

    ss << "      ]\n";
    ss << "    }]\n";
    ss << "  }]\n";
    ss << "}\n";
    return ss.str();
}

std::string TelemetryExporter::buildOTLPSpan(const TelemetryEvent& event,
                                              const ExportDestination& dest) const {
    std::ostringstream ss;
    // Generate deterministic trace/span IDs from event ID
    char traceId[33], spanId[17];
    snprintf(traceId, sizeof(traceId), "%016llx%016llx",
             (unsigned long long)(event.id ^ 0xDEADBEEF),
             (unsigned long long)(event.timestampMs));
    snprintf(spanId, sizeof(spanId), "%016llx", (unsigned long long)event.id);

    ss << "{";
    ss << "\"traceId\":\"" << traceId << "\"";
    ss << ",\"spanId\":\"" << spanId << "\"";
    ss << ",\"name\":\"" << event.category << "\"";
    ss << ",\"kind\":1"; // SPAN_KIND_INTERNAL
    
    // Convert ms timestamps to nanoseconds for OTLP
    uint64_t startNs = event.timestampMs * 1000000ULL;
    uint64_t endNs = (event.durationMs >= 0)
        ? startNs + (event.durationMs * 1000000ULL)
        : startNs;
    
    ss << ",\"startTimeUnixNano\":\"" << startNs << "\"";
    ss << ",\"endTimeUnixNano\":\"" << endNs << "\"";
    
    // Attributes
    ss << ",\"attributes\":[";
    ss << "{\"key\":\"level\",\"value\":{\"stringValue\":\"" << event.levelString() << "\"}}";
    ss << ",{\"key\":\"source\",\"value\":{\"stringValue\":\"" << event.sourceString() << "\"}}";
    ss << ",{\"key\":\"message\",\"value\":{\"stringValue\":\"" << event.message << "\"}}";
    if (event.valueNumeric != 0.0) {
        ss << ",{\"key\":\"value\",\"value\":{\"doubleValue\":" 
           << std::fixed << std::setprecision(4) << event.valueNumeric << "}}";
    }
    for (const auto& [k, v] : event.tags) {
        ss << ",{\"key\":\"" << k << "\",\"value\":{\"stringValue\":\"" << v << "\"}}";
    }
    ss << "]";
    
    // Status
    int statusCode = (event.level >= TelemetryLevel::Error) ? 2 : 1; // ERROR or OK
    ss << ",\"status\":{\"code\":" << statusCode << "}";
    ss << "}";
    
    return ss.str();
}

std::string TelemetryExporter::FormatAsPrometheus() const {
    if (m_core) return m_core->ExportPrometheus();
    return "";
}

// ============================================================================
// Audit Log Export
// ============================================================================
void TelemetryExporter::RecordAuditEntry(const std::string& actor,
                                          const std::string& action,
                                          const std::string& target,
                                          const std::string& detail) {
    std::lock_guard<std::mutex> lock(m_auditMutex);

    AuditExportEntry entry;
    entry.sequenceId = m_auditSequence++;
    entry.timestampMs = UnifiedTelemetryCore::NowMs();
    entry.actor = actor;
    entry.action = action;
    entry.target = target;
    entry.detail = detail;
    entry.chainHash = computeChainHash(entry, m_lastAuditHash);
    m_lastAuditHash = entry.chainHash;

    m_auditLog.push_back(std::move(entry));
}

std::vector<AuditExportEntry> TelemetryExporter::GetAuditEntries(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_auditMutex);
    size_t start = (m_auditLog.size() > maxCount) ? m_auditLog.size() - maxCount : 0;
    return std::vector<AuditExportEntry>(m_auditLog.begin() + start, m_auditLog.end());
}

bool TelemetryExporter::VerifyAuditChain() const {
    std::lock_guard<std::mutex> lock(m_auditMutex);
    if (m_auditLog.empty()) return true;

    uint64_t prevHash = 0;
    for (const auto& entry : m_auditLog) {
        uint64_t expected = computeChainHash(entry, prevHash);
        if (entry.chainHash != expected) return false;
        prevHash = entry.chainHash;
    }
    return true;
}

ExportResult TelemetryExporter::ExportAuditLog(const std::string& path,
                                                uint64_t fromSeq, uint64_t toSeq) {
    std::lock_guard<std::mutex> lock(m_auditMutex);

    std::ofstream file(path);
    if (!file.is_open()) return ExportResult::fail("Cannot open audit export file");

    file << "{\n  \"auditLog\": [\n";
    int count = 0;
    for (const auto& entry : m_auditLog) {
        if (entry.sequenceId < fromSeq || entry.sequenceId > toSeq) continue;
        if (count > 0) file << ",\n";
        file << "    {\"seq\":" << entry.sequenceId
             << ",\"ts\":" << entry.timestampMs
             << ",\"actor\":\"" << entry.actor << "\""
             << ",\"action\":\"" << entry.action << "\""
             << ",\"target\":\"" << entry.target << "\""
             << ",\"detail\":\"" << entry.detail << "\""
             << ",\"chainHash\":\"" << std::hex << entry.chainHash << std::dec << "\""
             << "}";
        count++;
    }
    file << "\n  ],\n";
    file << "  \"chainVerified\": " << (VerifyAuditChain() ? "true" : "false") << ",\n";
    file << "  \"totalEntries\": " << count << "\n";
    file << "}\n";

    int64_t bytes = static_cast<int64_t>(file.tellp());
    file.close();
    return ExportResult::ok(count, bytes);
}

uint64_t TelemetryExporter::computeChainHash(const AuditExportEntry& entry,
                                              uint64_t prevHash) const {
    // FNV-1a 64-bit hash of entry fields chained with previous hash
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    auto feed = [&](const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i) {
            hash ^= p[i];
            hash *= 1099511628211ULL; // FNV prime
        }
    };

    feed(&prevHash, sizeof(prevHash));
    feed(&entry.sequenceId, sizeof(entry.sequenceId));
    feed(&entry.timestampMs, sizeof(entry.timestampMs));
    feed(entry.actor.data(), entry.actor.size());
    feed(entry.action.data(), entry.action.size());
    feed(entry.target.data(), entry.target.size());
    feed(entry.detail.data(), entry.detail.size());

    return hash;
}

bool AuditExportEntry::verifyChain(uint64_t previousHash) const {
    // Recompute — delegate to a standalone function
    uint64_t hash = 14695981039346656037ULL;
    auto feed = [&](const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i) {
            hash ^= p[i];
            hash *= 1099511628211ULL;
        }
    };

    feed(&previousHash, sizeof(previousHash));
    feed(&sequenceId, sizeof(sequenceId));
    feed(&timestampMs, sizeof(timestampMs));
    feed(actor.data(), actor.size());
    feed(action.data(), action.size());
    feed(target.data(), target.size());
    feed(detail.data(), detail.size());

    return hash == chainHash;
}

// ============================================================================
// Plugin Exporter
// ============================================================================
bool TelemetryExporter::LoadExporterPlugin(const std::string& dllPath) {
#ifdef _WIN32
    HMODULE hMod = LoadLibraryA(dllPath.c_str());
    if (!hMod) return false;

    LoadedExporter exp;
    exp.path = dllPath;
    exp.hModule = hMod;
    exp.fnGetInfo = reinterpret_cast<ExporterPlugin_GetInfo_fn>(
        GetProcAddress(hMod, "ExporterPlugin_GetInfo"));
    exp.fnInit = reinterpret_cast<ExporterPlugin_Init_fn>(
        GetProcAddress(hMod, "ExporterPlugin_Init"));
    exp.fnExport = reinterpret_cast<ExporterPlugin_Export_fn>(
        GetProcAddress(hMod, "ExporterPlugin_Export"));
    exp.fnShutdown = reinterpret_cast<ExporterPlugin_Shutdown_fn>(
        GetProcAddress(hMod, "ExporterPlugin_Shutdown"));

    if (!exp.fnGetInfo || !exp.fnExport) {
        FreeLibrary(hMod);
        return false;
    }

    // Initialize the plugin
    if (exp.fnInit) {
        int result = exp.fnInit("{}");
        if (result != 0) {
            FreeLibrary(hMod);
            return false;
        }
    }

    m_pluginExporters.push_back(std::move(exp));
    return true;
#else
    (void)dllPath;
    return false;
#endif
}

void TelemetryExporter::UnloadExporterPlugins() {
    for (auto& exp : m_pluginExporters) {
        if (exp.fnShutdown) exp.fnShutdown();
#ifdef _WIN32
        if (exp.hModule) FreeLibrary(exp.hModule);
#endif
    }
    m_pluginExporters.clear();
}

// ============================================================================
// Internal Export
// ============================================================================
ExportResult TelemetryExporter::doExport(const ExportDestination& dest,
                                          const std::vector<TelemetryEvent>& events) {
    std::string content;
    std::string contentType = "application/json";

    switch (dest.format) {
        case ExportFormat::JSON:       content = FormatAsJSON(events); break;
        case ExportFormat::JSONL:      content = FormatAsJSONL(events); break;
        case ExportFormat::CSV:        content = FormatAsCSV(events); contentType = "text/csv"; break;
        case ExportFormat::Prometheus: content = FormatAsPrometheus(); contentType = "text/plain"; break;
        case ExportFormat::OTLP_JSON:  content = FormatAsOTLP(events, dest); break;
        case ExportFormat::Custom: {
            // Route to plugin exporters
            for (auto& exp : m_pluginExporters) {
                if (!exp.fnExport) continue;
                std::vector<TelemetryExportEvent> cEvents;
                cEvents.reserve(events.size());
                // Convert to C-ABI structs
                std::vector<std::string> tagStrings; // keep alive
                for (const auto& e : events) {
                    TelemetryExportEvent ce;
                    ce.id = e.id;
                    ce.timestampMs = e.timestampMs;
                    ce.level = static_cast<int>(e.level);
                    ce.source = static_cast<int>(e.source);
                    ce.category = e.category.c_str();
                    ce.message = e.message.c_str();
                    ce.valueNumeric = e.valueNumeric;
                    ce.unit = e.unit.c_str();
                    ce.durationMs = e.durationMs;
                    tagStrings.push_back(serializeTags(e.tags));
                    ce.tagsSerialized = tagStrings.back().c_str();
                    cEvents.push_back(ce);
                }
                exp.fnExport(cEvents.data(), static_cast<int>(cEvents.size()));
            }
            return ExportResult::ok(static_cast<int>(events.size()), 0);
        }
        default:
            return ExportResult::fail("Unknown export format");
    }

    // Determine if file or HTTP
    if (dest.endpoint.find("http://") == 0 || dest.endpoint.find("https://") == 0) {
        return doHttpExport(dest.endpoint, content, dest.apiKey, contentType);
    } else {
        return doFileExport(dest.endpoint, content);
    }
}

ExportResult TelemetryExporter::doFileExport(const std::string& path,
                                              const std::string& content) {
    // Ensure directory exists
    auto dir = fs::path(path).parent_path();
    if (!dir.empty()) {
        fs::create_directories(dir);
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) return ExportResult::fail("Cannot open file: " + path);

    file << content;
    int64_t bytes = static_cast<int64_t>(content.size());
    file.close();

    RecordAuditEntry("TelemetryExporter", "file_export", path,
                     "Exported " + std::to_string(bytes) + " bytes");

    return ExportResult::ok(1, bytes);
}

ExportResult TelemetryExporter::doHttpExport(const std::string& url,
                                              const std::string& content,
                                              const std::string& apiKey,
                                              const std::string& contentType) {
#ifdef _WIN32
    // Parse URL
    URL_COMPONENTSW urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;

    std::wstring wUrl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        return ExportResult::fail("Invalid URL: " + url);
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Telemetry/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return ExportResult::fail("WinHttpOpen failed");

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    HINTERNET hConnect = WinHttpConnect(hSession, hostName,
                                         urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return ExportResult::fail("WinHttpConnect failed");
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath,
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ExportResult::fail("WinHttpOpenRequest failed");
    }

    // Add headers
    std::wstring ctHeader = L"Content-Type: " + std::wstring(contentType.begin(), contentType.end());
    WinHttpAddRequestHeaders(hRequest, ctHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (!apiKey.empty()) {
        std::wstring authHeader = L"Authorization: Bearer " + std::wstring(apiKey.begin(), apiKey.end());
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    (LPVOID)content.data(), (DWORD)content.size(),
                                    (DWORD)content.size(), 0);

    ExportResult result = ExportResult::fail("Send failed");
    if (sent && WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           nullptr, &statusCode, &statusSize, nullptr);

        if (statusCode >= 200 && statusCode < 300) {
            result = ExportResult::ok(1, static_cast<int64_t>(content.size()));
        } else {
            result = ExportResult::fail("HTTP " + std::to_string(statusCode));
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    RecordAuditEntry("TelemetryExporter", "http_export", url,
                     result.success ? "OK" : result.error);

    return result;
#else
    (void)url; (void)content; (void)apiKey; (void)contentType;
    return ExportResult::fail("HTTP export not available on this platform");
#endif
}

// ============================================================================
// Tag Serialization
// ============================================================================
std::string TelemetryExporter::serializeTags(const std::map<std::string, std::string>& tags) {
    std::ostringstream ss;
    bool first = true;
    for (const auto& [k, v] : tags) {
        if (!first) ss << ";";
        ss << k << "=" << v;
        first = false;
    }
    return ss.str();
}

std::map<std::string, std::string> TelemetryExporter::deserializeTags(const std::string& s) {
    std::map<std::string, std::string> tags;
    std::istringstream iss(s);
    std::string pair;
    while (std::getline(iss, pair, ';')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            tags[pair.substr(0, eq)] = pair.substr(eq + 1);
        }
    }
    return tags;
}

// ============================================================================
// Auto-Export Timer
// ============================================================================
#ifdef _WIN32
DWORD WINAPI TelemetryExporter::AutoExportThreadProc(LPVOID param) {
    auto* self = static_cast<TelemetryExporter*>(param);
    
    while (self->m_autoExportRunning.load()) {
        // Find minimum interval across all destinations
        int minInterval = 60000; // Default 60s
        {
            std::lock_guard<std::mutex> lock(self->m_destMutex);
            for (const auto& [id, dest] : self->m_destinations) {
                if (dest.enabled && dest.intervalMs > 0 && dest.intervalMs < minInterval) {
                    minInterval = dest.intervalMs;
                }
            }
        }
        
        Sleep(static_cast<DWORD>(minInterval));
        
        if (self->m_autoExportRunning.load()) {
            self->ExportNow();
        }
    }
    return 0;
}
#endif

void TelemetryExporter::StartAutoExport() {
    if (m_autoExportRunning.load()) return;
    m_autoExportRunning.store(true);
#ifdef _WIN32
    m_hAutoExportThread = CreateThread(nullptr, 0, AutoExportThreadProc, this, 0, nullptr);
#endif
}

void TelemetryExporter::StopAutoExport() {
    m_autoExportRunning.store(false);
#ifdef _WIN32
    if (m_hAutoExportThread) {
        WaitForSingleObject(m_hAutoExportThread, 5000);
        CloseHandle(m_hAutoExportThread);
        m_hAutoExportThread = nullptr;
    }
#endif
}

TelemetryExporter::ExportStats TelemetryExporter::GetStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

} // namespace Telemetry
} // namespace RawrXD
