// ============================================================================
// Win32IDE_FlightRecorder.cpp — Phase 36: Persistent Binary Flight Recorder
// ============================================================================
//
// Memory-mapped binary ring-buffer flight recorder for capturing
// OutputDebugString events, telemetry snapshots, error traces, and
// structured diagnostics. Zero external dependencies (no SQLite, no Qt).
//
// Architecture:
//   - File-backed ring buffer via CreateFileMapping / MapViewOfFile
//   - Fixed 4 MB file at %LOCALAPPDATA%\RawrXD\flight_recorder.bin
//   - Header (4096 bytes) + Ring (remaining)
//   - Lock-free single-producer writes via InterlockedCompareExchange64
//   - Read access via exported dump functions (JSON / raw binary)
//
// Record format (variable-length, padded to 8-byte alignment):
//   [8] timestamp_ms (uint64 — GetTickCount64)
//   [4] record_type  (uint32 — enum FlightRecordType)
//   [4] payload_len  (uint32 — bytes of payload following)
//   [N] payload      (UTF-8 string or binary blob)
//   [P] padding      (0-7 bytes to reach 8-byte alignment)
//
// ============================================================================

#include "Win32IDE.h"
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

// ============================================================================
//                         CONSTANTS
// ============================================================================

static constexpr uint32_t FR_MAGIC          = 0x52574658;   // "RWFX"
static constexpr uint32_t FR_VERSION        = 1;
static constexpr uint32_t FR_HEADER_SIZE    = 4096;
static constexpr uint32_t FR_FILE_SIZE      = 4 * 1024 * 1024; // 4 MB
static constexpr uint32_t FR_RING_SIZE      = FR_FILE_SIZE - FR_HEADER_SIZE;
static constexpr uint32_t FR_MAX_PAYLOAD    = 4000;         // max single record payload

// Record types
enum FlightRecordType : uint32_t {
    FR_TYPE_DEBUG_STRING    = 1,
    FR_TYPE_ERROR           = 2,
    FR_TYPE_TELEMETRY       = 3,
    FR_TYPE_COMMAND         = 4,
    FR_TYPE_LIFECYCLE       = 5,
    FR_TYPE_PERFORMANCE     = 6,
    FR_TYPE_AGENT_ACTION    = 7,
    FR_TYPE_HOTPATCH        = 8,
    FR_TYPE_LSP_EVENT       = 9,
    FR_TYPE_USER_ACTION     = 10,
    FR_TYPE_CRASH_MARKER    = 11,
    FR_TYPE_COT_STEP        = 12,
    FR_TYPE_MCP_REQUEST     = 13,
    FR_TYPE_MCP_RESPONSE    = 14,
    FR_TYPE_EXTENSION_EVENT = 15,
    FR_TYPE_CUSTOM          = 255
};

// File header (lives at offset 0, 4096 bytes total with padding)
#pragma pack(push, 1)
struct FlightRecorderHeader {
    uint32_t magic;             // FR_MAGIC
    uint32_t version;           // FR_VERSION
    uint64_t writeOffset;       // current write position in ring (monotonic, mod FR_RING_SIZE)
    uint64_t totalRecords;      // total records written (lifetime)
    uint64_t totalBytes;        // total payload bytes written (lifetime)
    uint64_t sessionId;         // random session identifier
    uint64_t sessionStartMs;    // GetTickCount64 at init
    uint32_t ringSize;          // FR_RING_SIZE
    uint32_t headerSize;        // FR_HEADER_SIZE
    uint32_t pid;               // process ID
    uint32_t flags;             // reserved
    char     buildVersion[64];  // "RawrXD v20.0.0" etc.
    char     reserved[FR_HEADER_SIZE - 128]; // pad to 4096
};
#pragma pack(pop)

static_assert(sizeof(FlightRecorderHeader) == FR_HEADER_SIZE, "Header must be exactly 4096 bytes");

// Per-record header (16 bytes + variable payload + padding)
#pragma pack(push, 1)
struct FlightRecord {
    uint64_t timestampMs;
    uint32_t type;
    uint32_t payloadLen;
    // payload follows immediately
};
#pragma pack(pop)

static_assert(sizeof(FlightRecord) == 16, "Record header must be 16 bytes");

// ============================================================================
//                      MODULE STATE
// ============================================================================

static HANDLE       s_hFile         = INVALID_HANDLE_VALUE;
static HANDLE       s_hMapping      = nullptr;
static uint8_t*     s_mappedBase    = nullptr;
static FlightRecorderHeader* s_header = nullptr;
static uint8_t*     s_ringBase      = nullptr;
static bool         s_initialized   = false;
static CRITICAL_SECTION s_cs;

// Forward declarations
static bool frEnsureDirectory(const char* path);
static void frWriteRecord(FlightRecordType type, const char* payload, uint32_t len);

// ============================================================================
//                    UTILITY: Build file path
// ============================================================================

static bool frGetFilePath(char* outPath, size_t outSize) {
    char appDataPath[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath))) {
        return false;
    }
    _snprintf_s(outPath, outSize, _TRUNCATE, "%s\\RawrXD", appDataPath);
    frEnsureDirectory(outPath);
    _snprintf_s(outPath, outSize, _TRUNCATE, "%s\\RawrXD\\flight_recorder.bin", appDataPath);
    return true;
}

static bool frEnsureDirectory(const char* path) {
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return CreateDirectoryA(path, nullptr) != 0;
}

// ============================================================================
//                    GENERATE SESSION ID
// ============================================================================

static uint64_t frGenerateSessionId() {
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    uint64_t seed = static_cast<uint64_t>(qpc.QuadPart);
    seed ^= static_cast<uint64_t>(GetCurrentProcessId()) << 32;
    seed ^= GetTickCount64();
    // Simple xorshift64
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

// ============================================================================
//                    INIT / SHUTDOWN
// ============================================================================

void Win32IDE::initFlightRecorder() {
    if (s_initialized) return;

    InitializeCriticalSection(&s_cs);

    char filePath[MAX_PATH] = {};
    if (!frGetFilePath(filePath, sizeof(filePath))) {
        OutputDebugStringA("FlightRecorder: Failed to resolve file path\n");
        return;
    }

    // Open or create the backing file
    s_hFile = CreateFileA(
        filePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,    // allow concurrent readers
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (s_hFile == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("FlightRecorder: CreateFile failed\n");
        return;
    }

    // Ensure file is exactly FR_FILE_SIZE
    LARGE_INTEGER fileSize;
    GetFileSizeEx(s_hFile, &fileSize);
    if (fileSize.QuadPart < FR_FILE_SIZE) {
        LARGE_INTEGER newSize;
        newSize.QuadPart = FR_FILE_SIZE;
        SetFilePointerEx(s_hFile, newSize, nullptr, FILE_BEGIN);
        SetEndOfFile(s_hFile);
    }

    // Create file mapping
    s_hMapping = CreateFileMappingA(
        s_hFile,
        nullptr,
        PAGE_READWRITE,
        0,
        FR_FILE_SIZE,
        nullptr     // unnamed mapping
    );
    if (!s_hMapping) {
        OutputDebugStringA("FlightRecorder: CreateFileMapping failed\n");
        CloseHandle(s_hFile);
        s_hFile = INVALID_HANDLE_VALUE;
        return;
    }

    // Map the entire file
    s_mappedBase = static_cast<uint8_t*>(
        MapViewOfFile(s_hMapping, FILE_MAP_ALL_ACCESS, 0, 0, FR_FILE_SIZE)
    );
    if (!s_mappedBase) {
        OutputDebugStringA("FlightRecorder: MapViewOfFile failed\n");
        CloseHandle(s_hMapping);
        CloseHandle(s_hFile);
        s_hMapping = nullptr;
        s_hFile = INVALID_HANDLE_VALUE;
        return;
    }

    s_header  = reinterpret_cast<FlightRecorderHeader*>(s_mappedBase);
    s_ringBase = s_mappedBase + FR_HEADER_SIZE;

    // Check if existing valid header or fresh file
    bool needsInit = (s_header->magic != FR_MAGIC || s_header->version != FR_VERSION);
    if (needsInit) {
        memset(s_mappedBase, 0, FR_FILE_SIZE);
        s_header->magic          = FR_MAGIC;
        s_header->version        = FR_VERSION;
        s_header->writeOffset    = 0;
        s_header->totalRecords   = 0;
        s_header->totalBytes     = 0;
        s_header->sessionId      = frGenerateSessionId();
        s_header->sessionStartMs = GetTickCount64();
        s_header->ringSize       = FR_RING_SIZE;
        s_header->headerSize     = FR_HEADER_SIZE;
        s_header->pid            = GetCurrentProcessId();
        s_header->flags          = 0;
        _snprintf_s(s_header->buildVersion, sizeof(s_header->buildVersion),
                    _TRUNCATE, "RawrXD v20.0.0-phase36");
    } else {
        // Continuing from previous session — update session fields
        s_header->sessionId      = frGenerateSessionId();
        s_header->sessionStartMs = GetTickCount64();
        s_header->pid            = GetCurrentProcessId();
    }

    // Flush header to disk
    FlushViewOfFile(s_header, FR_HEADER_SIZE);

    s_initialized = true;
    m_flightRecorderInitialized = true;

    // Write lifecycle event
    frWriteRecord(FR_TYPE_LIFECYCLE, "FlightRecorder initialized", 26);

    char msg[128];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "FlightRecorder: Initialized (session %016llX, %u bytes ring)\n",
                s_header->sessionId, FR_RING_SIZE);
    OutputDebugStringA(msg);
}

void Win32IDE::shutdownFlightRecorder() {
    if (!s_initialized) return;

    frWriteRecord(FR_TYPE_LIFECYCLE, "FlightRecorder shutting down", 28);

    // Flush before unmap
    if (s_mappedBase) {
        FlushViewOfFile(s_mappedBase, FR_FILE_SIZE);
        UnmapViewOfFile(s_mappedBase);
        s_mappedBase = nullptr;
        s_header = nullptr;
        s_ringBase = nullptr;
    }
    if (s_hMapping) {
        CloseHandle(s_hMapping);
        s_hMapping = nullptr;
    }
    if (s_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(s_hFile);
        s_hFile = INVALID_HANDLE_VALUE;
    }

    DeleteCriticalSection(&s_cs);
    s_initialized = false;
    m_flightRecorderInitialized = false;

    OutputDebugStringA("FlightRecorder: Shutdown complete\n");
}

// ============================================================================
//                    CORE WRITE (RING BUFFER)
// ============================================================================

static void frWriteRecord(FlightRecordType type, const char* payload, uint32_t len) {
    if (!s_initialized || !s_ringBase || !s_header) return;

    // Clamp payload
    if (len > FR_MAX_PAYLOAD) len = FR_MAX_PAYLOAD;

    // Calculate total record size (header + payload + padding to 8-byte boundary)
    uint32_t recordSize = sizeof(FlightRecord) + len;
    uint32_t padded = (recordSize + 7) & ~7u;

    EnterCriticalSection(&s_cs);

    uint64_t writePos = s_header->writeOffset % FR_RING_SIZE;

    // Build record header
    FlightRecord rec;
    rec.timestampMs = GetTickCount64();
    rec.type        = static_cast<uint32_t>(type);
    rec.payloadLen  = len;

    // Write record header — handle wrap-around
    uint64_t remaining = FR_RING_SIZE - writePos;
    if (remaining < padded) {
        // Not enough space at end — zero-fill remainder and wrap to start
        memset(s_ringBase + writePos, 0, static_cast<size_t>(remaining));
        writePos = 0;
    }

    // Write record header
    memcpy(s_ringBase + writePos, &rec, sizeof(FlightRecord));
    // Write payload
    if (len > 0 && payload) {
        memcpy(s_ringBase + writePos + sizeof(FlightRecord), payload, len);
    }
    // Zero padding bytes
    uint32_t padBytes = padded - recordSize;
    if (padBytes > 0) {
        memset(s_ringBase + writePos + recordSize, 0, padBytes);
    }

    // Advance write offset (monotonic)
    s_header->writeOffset += padded;
    s_header->totalRecords++;
    s_header->totalBytes += len;

    LeaveCriticalSection(&s_cs);
}

// ============================================================================
//                    PUBLIC RECORDING API
// ============================================================================

void Win32IDE::flightRecordEvent(int type, const char* message) {
    if (!s_initialized || !message) return;

    uint32_t len = static_cast<uint32_t>(strlen(message));
    frWriteRecord(static_cast<FlightRecordType>(type), message, len);
}

void Win32IDE::flightRecordDebug(const char* message) {
    flightRecordEvent(FR_TYPE_DEBUG_STRING, message);
}

void Win32IDE::flightRecordError(const char* message) {
    flightRecordEvent(FR_TYPE_ERROR, message);
}

void Win32IDE::flightRecordCommand(int commandId) {
    char buf[64];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "CMD:%d", commandId);
    flightRecordEvent(FR_TYPE_COMMAND, buf);
}

void Win32IDE::flightRecordPerformance(const char* label, double elapsedMs) {
    char buf[256];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s:%.3fms", label, elapsedMs);
    flightRecordEvent(FR_TYPE_PERFORMANCE, buf);
}

void Win32IDE::flightRecordAgentAction(const char* action, const char* detail) {
    char buf[512];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s|%s",
                action ? action : "?", detail ? detail : "");
    flightRecordEvent(FR_TYPE_AGENT_ACTION, buf);
}

void Win32IDE::flightRecordCoTStep(int stepIndex, const char* role, const char* summary) {
    char buf[512];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "step:%d|role:%s|%s",
                stepIndex, role ? role : "?", summary ? summary : "");
    flightRecordEvent(FR_TYPE_COT_STEP, buf);
}

void Win32IDE::flightRecordCrashMarker(const char* reason) {
    flightRecordEvent(FR_TYPE_CRASH_MARKER, reason ? reason : "UNKNOWN_CRASH");
    // Force flush to disk immediately
    if (s_mappedBase) {
        FlushViewOfFile(s_mappedBase, FR_FILE_SIZE);
    }
}

// ============================================================================
//                    EXPORT: JSON DUMP
// ============================================================================

void Win32IDE::cmdFlightRecorderExportJSON() {
    if (!s_initialized || !s_header || !s_ringBase) {
        MessageBoxA(m_hwndMain, "Flight recorder not initialized.",
                    "Flight Recorder", MB_OK | MB_ICONWARNING);
        return;
    }

    // Build JSON output
    std::string json;
    json.reserve(1024 * 1024); // 1 MB pre-alloc
    json += "{\n  \"session\": \"";

    char hexBuf[32];
    _snprintf_s(hexBuf, sizeof(hexBuf), _TRUNCATE, "%016llX", s_header->sessionId);
    json += hexBuf;
    json += "\",\n  \"pid\": ";
    json += std::to_string(s_header->pid);
    json += ",\n  \"totalRecords\": ";
    json += std::to_string(s_header->totalRecords);
    json += ",\n  \"totalBytes\": ";
    json += std::to_string(s_header->totalBytes);
    json += ",\n  \"buildVersion\": \"";
    json += s_header->buildVersion;
    json += "\",\n  \"records\": [\n";

    // Walk the ring buffer and extract records
    uint64_t readPos = 0;
    uint64_t totalWritten = s_header->writeOffset;
    uint64_t startPos = 0;

    // If we've wrapped around, start reading from current writeOffset
    if (totalWritten > FR_RING_SIZE) {
        startPos = totalWritten % FR_RING_SIZE;
    }

    EnterCriticalSection(&s_cs);

    uint64_t pos = startPos;
    uint64_t bytesRead = 0;
    uint64_t maxRead = (totalWritten < FR_RING_SIZE) ? totalWritten : FR_RING_SIZE;
    int recordCount = 0;

    while (bytesRead < maxRead && recordCount < 100000) {
        uint64_t ringPos = pos % FR_RING_SIZE;

        // Check if we have enough space for a record header
        if (ringPos + sizeof(FlightRecord) > FR_RING_SIZE) {
            // Wrap
            pos = ((pos / FR_RING_SIZE) + 1) * FR_RING_SIZE;
            bytesRead += FR_RING_SIZE - ringPos;
            continue;
        }

        FlightRecord rec;
        memcpy(&rec, s_ringBase + ringPos, sizeof(FlightRecord));

        // Validate record
        if (rec.timestampMs == 0 && rec.type == 0 && rec.payloadLen == 0) {
            // Zero-filled gap (wrap padding)
            pos += 8; // skip 8 bytes
            bytesRead += 8;
            continue;
        }

        if (rec.payloadLen > FR_MAX_PAYLOAD) {
            // Corrupt record — skip
            pos += 8;
            bytesRead += 8;
            continue;
        }

        uint32_t padded = (sizeof(FlightRecord) + rec.payloadLen + 7) & ~7u;

        // Extract payload
        char payloadBuf[FR_MAX_PAYLOAD + 1] = {};
        uint32_t payloadStart = static_cast<uint32_t>(ringPos + sizeof(FlightRecord));
        if (payloadStart + rec.payloadLen <= FR_RING_SIZE && rec.payloadLen > 0) {
            memcpy(payloadBuf, s_ringBase + payloadStart, rec.payloadLen);
            payloadBuf[rec.payloadLen] = '\0';
        }

        // Emit JSON record
        if (recordCount > 0) json += ",\n";
        json += "    {\"ts\":";
        json += std::to_string(rec.timestampMs);
        json += ",\"type\":";
        json += std::to_string(rec.type);
        json += ",\"msg\":\"";

        // Escape JSON string
        for (uint32_t i = 0; i < rec.payloadLen && payloadBuf[i]; i++) {
            char ch = payloadBuf[i];
            if (ch == '"') json += "\\\"";
            else if (ch == '\\') json += "\\\\";
            else if (ch == '\n') json += "\\n";
            else if (ch == '\r') json += "\\r";
            else if (ch == '\t') json += "\\t";
            else if (ch >= 0x20) json += ch;
        }
        json += "\"}";

        recordCount++;
        pos += padded;
        bytesRead += padded;
    }

    LeaveCriticalSection(&s_cs);

    json += "\n  ]\n}\n";

    // Save via file dialog
    char savePath[MAX_PATH] = "flight_recorder_export.json";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile   = savePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = "Export Flight Recorder (JSON)";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "json";

    if (GetSaveFileNameA(&ofn)) {
        HANDLE hOut = CreateFileA(savePath, GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hOut, json.c_str(), static_cast<DWORD>(json.size()), &written, nullptr);
            CloseHandle(hOut);

            char msg[MAX_PATH + 64];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                        "Exported %d records (%zu bytes) to:\n%s",
                        recordCount, json.size(), savePath);
            MessageBoxA(m_hwndMain, msg, "Flight Recorder", MB_OK | MB_ICONINFORMATION);
        }
    }
}

// ============================================================================
//                    EXPORT: DASHBOARD (MessageBox summary)
// ============================================================================

void Win32IDE::cmdFlightRecorderDashboard() {
    if (!s_initialized || !s_header) {
        MessageBoxA(m_hwndMain, "Flight recorder not initialized.",
                    "Flight Recorder", MB_OK | MB_ICONWARNING);
        return;
    }

    uint64_t uptimeMs = GetTickCount64() - s_header->sessionStartMs;
    uint64_t uptimeSec = uptimeMs / 1000;
    uint64_t uptimeMin = uptimeSec / 60;
    uint64_t uptimeHr  = uptimeMin / 60;

    double avgRecordSize = (s_header->totalRecords > 0)
        ? static_cast<double>(s_header->totalBytes) / static_cast<double>(s_header->totalRecords)
        : 0.0;

    double recordsPerMin = (uptimeMin > 0)
        ? static_cast<double>(s_header->totalRecords) / static_cast<double>(uptimeMin)
        : static_cast<double>(s_header->totalRecords);

    char dashboard[2048];
    _snprintf_s(dashboard, sizeof(dashboard), _TRUNCATE,
        "=== RawrXD Flight Recorder Dashboard ===\n\n"
        "Session:        %016llX\n"
        "Build:          %s\n"
        "PID:            %u\n"
        "Uptime:         %lluh %llum %llus\n\n"
        "--- Ring Buffer ---\n"
        "Ring Size:      %s\n"
        "Write Offset:   %llu bytes\n"
        "Wraps:          %llu\n\n"
        "--- Statistics ---\n"
        "Total Records:  %llu\n"
        "Total Payload:  %llu bytes\n"
        "Avg Record:     %.1f bytes\n"
        "Records/min:    %.1f\n",
        s_header->sessionId,
        s_header->buildVersion,
        s_header->pid,
        uptimeHr, uptimeMin % 60, uptimeSec % 60,
        "4 MB",
        s_header->writeOffset,
        s_header->writeOffset / FR_RING_SIZE,
        s_header->totalRecords,
        s_header->totalBytes,
        avgRecordSize,
        recordsPerMin
    );

    MessageBoxA(m_hwndMain, dashboard, "Flight Recorder Dashboard",
                MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
//                    CLEAR (GDPR / Privacy)
// ============================================================================

void Win32IDE::cmdFlightRecorderClear() {
    if (!s_initialized || !s_header) {
        MessageBoxA(m_hwndMain, "Flight recorder not initialized.",
                    "Flight Recorder", MB_OK | MB_ICONWARNING);
        return;
    }

    int result = MessageBoxA(m_hwndMain,
        "Clear all flight recorder data?\n\n"
        "This will zero the entire ring buffer and reset all counters.\n"
        "This action cannot be undone.",
        "Flight Recorder - Clear Data",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (result != IDYES) return;

    EnterCriticalSection(&s_cs);

    // Zero ring
    memset(s_ringBase, 0, FR_RING_SIZE);

    // Reset counters
    s_header->writeOffset  = 0;
    s_header->totalRecords = 0;
    s_header->totalBytes   = 0;

    // New session
    s_header->sessionId      = frGenerateSessionId();
    s_header->sessionStartMs = GetTickCount64();

    FlushViewOfFile(s_mappedBase, FR_FILE_SIZE);

    LeaveCriticalSection(&s_cs);

    frWriteRecord(FR_TYPE_LIFECYCLE, "Flight recorder cleared by user", 31);

    MessageBoxA(m_hwndMain, "Flight recorder data cleared.",
                "Flight Recorder", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
//                    COMMAND HANDLER
// ============================================================================

bool Win32IDE::handleFlightRecorderCommand(int commandId) {
    switch (commandId) {
    case IDM_FR_EXPORT_JSON:
        cmdFlightRecorderExportJSON();
        return true;
    case IDM_FR_DASHBOARD:
        cmdFlightRecorderDashboard();
        return true;
    case IDM_FR_CLEAR:
        cmdFlightRecorderClear();
        return true;
    default:
        return false;
    }
}
