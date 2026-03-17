// ============================================================================
// RawrXD_LSPServer.h — Phase 27: Embedded LSP Server (JSON-RPC 2.0 / stdio)
// ============================================================================
// PURPOSE: Provide an LSP 3.17-compliant server that exposes RawrXD's own
//          symbol database to *any* editor speaking the Language Server Protocol.
//          The existing Win32IDE_LSPClient.cpp connects *outward* to clangd /
//          pyright / typescript-language-server.  This server runs *inward*,
//          serving workspace symbols, document symbols, hover, semantic tokens,
//          and diagnostics from our own index.
//
// TRANSPORT: JSON-RPC 2.0 over stdin/stdout (stdio mode, default)
//            or named-pipe (\\.\pipe\rawrxd-lsp) for in-process attachment.
//
// THREADING: Dedicated reader thread + single dispatch thread.
//            All index access protected by std::shared_mutex (readers/writer).
//
// DEPENDENCIES:
//   - nlohmann/json (already linked via Win32IDE.h)
//   - context/indexer.h (extended to SymbolDatabase backing store)
//   - <shared_mutex>, <thread>, <atomic>, <condition_variable>
//
// BUILD: Added to WIN32IDE_SOURCES in CMakeLists.txt.
//        New IDM commands in 9200+ range.
//
// Copyright (c) 2025 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <optional>

// Forward-declare nlohmann::basic_json (the actual underlying type for 'json').
// nlohmann::json is usually: basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator, adl_serializer>
// To avoid strict forward-declaration issues with the new ABI-versioned nlohmann,
// we'll just include the header if needed or use the namespace-qualified name in the .cpp.
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace RawrXD {
namespace LSPServer {

// ============================================================================
// LSP SYMBOL KIND (subset matching LSP 3.17 spec)
// ============================================================================
enum class SymbolKind : int {
    File          = 1,
    Module        = 2,
    Namespace     = 3,
    Package       = 4,
    Class         = 5,
    Method        = 6,
    Property      = 7,
    Field         = 8,
    Constructor   = 9,
    Enum          = 10,
    Interface     = 11,
    Function      = 12,
    Variable      = 13,
    Constant      = 14,
    String        = 15,
    Number        = 16,
    Boolean       = 17,
    Array         = 18,
    Object        = 19,
    Key           = 20,
    Null          = 21,
    EnumMember    = 22,
    Struct        = 23,
    Event         = 24,
    Operator      = 25,
    TypeParameter = 26
};

// ============================================================================
// LSP SEMANTIC TOKEN TYPES (subset)
// ============================================================================
enum class SemanticTokenType : int {
    Namespace    = 0,
    Type         = 1,
    Class        = 2,
    Enum         = 3,
    Interface    = 4,
    Struct       = 5,
    TypeParam    = 6,
    Parameter    = 7,
    Variable     = 8,
    Property     = 9,
    EnumMember   = 10,
    Event        = 11,
    Function     = 12,
    Method       = 13,
    Macro        = 14,
    Keyword      = 15,
    Modifier     = 16,
    Comment      = 17,
    String       = 18,
    Number       = 19,
    Regexp       = 20,
    Operator     = 21
};

// ============================================================================
// LSP DIAGNOSTIC SEVERITY
// ============================================================================
enum class DiagnosticSeverity : int {
    Error       = 1,
    Warning     = 2,
    Information = 3,
    Hint        = 4
};

// ============================================================================
// LSP POSITION / RANGE / LOCATION (mirrors Win32IDE.h types)
// ============================================================================
struct Position {
    int line      = 0;   // 0-based
    int character = 0;   // 0-based, UTF-16 offset
};

struct Range {
    Position start;
    Position end;
};

struct Location {
    std::string uri;
    Range       range;
};

// ============================================================================
// INDEXED SYMBOL — extended from context/indexer.h Symbol
// ============================================================================
struct IndexedSymbol {
    std::string  name;
    SymbolKind   kind          = SymbolKind::Variable;
    std::string  detail;          // signature, e.g. "void foo(int x)"
    std::string  containerName;   // enclosing class/namespace
    std::string  filePath;        // absolute path on disk
    int          line            = 0;   // 0-based
    int          startChar       = 0;
    int          endChar         = 0;
    uint64_t     hash            = 0;   // FNV-1a of name+file+line for dedup
};

// ============================================================================
// DOCUMENT STATE — per-open-file tracking (didOpen / didChange / didClose)
// ============================================================================
struct DocumentState {
    std::string uri;
    std::string languageId;
    int         version         = 0;
    std::string content;           // full text (synced via incremental updates)
    bool        dirty           = false;
    uint64_t    lastIndexedHash = 0;
};

// ============================================================================
// DIAGNOSTIC ENTRY — server-side diagnostic to be published
// ============================================================================
struct DiagnosticEntry {
    Range               range;
    DiagnosticSeverity  severity     = DiagnosticSeverity::Information;
    std::string         code;
    std::string         source;      // "rawrxd"
    std::string         message;
};

// ============================================================================
// SERVER STATISTICS — runtime counters
// ============================================================================
struct ServerStats {
    uint64_t totalRequests          = 0;
    uint64_t totalNotifications     = 0;
    uint64_t totalErrors            = 0;
    uint64_t symbolsIndexed         = 0;
    uint64_t filesTracked           = 0;
    uint64_t hoverRequests          = 0;
    uint64_t completionRequests     = 0;
    uint64_t definitionRequests     = 0;
    uint64_t referenceRequests      = 0;
    uint64_t documentSymbolRequests = 0;
    uint64_t workspaceSymbolRequests = 0;
    uint64_t semanticTokenRequests  = 0;
    uint64_t bytesRead              = 0;
    uint64_t bytesWritten           = 0;
    double   avgResponseMs          = 0.0;
    uint64_t uptime_seconds         = 0;
};

// ============================================================================
// SERVER CONFIGURATION
// ============================================================================
struct ServerConfig {
    // Transport
    bool        useStdio            = true;    // false → named pipe
    std::string pipeName            = "\\\\.\\pipe\\rawrxd-lsp";
    
    // Capabilities to advertise
    bool        enableSemanticTokens = true;
    bool        enableHover          = true;
    bool        enableCompletion     = true;
    bool        enableDefinition     = true;
    bool        enableReferences     = true;
    bool        enableDocumentSymbol = true;
    bool        enableWorkspaceSymbol = true;
    bool        enableDiagnostics    = true;
    
    // Indexer tuning
    int         indexThrottleMs      = 200;    // debounce didChange re-index
    int         maxSymbolResults     = 500;    // cap on workspace/symbol results
    int         maxCompletionItems   = 100;
    
    // Workspace
    std::string rootUri;                       // e.g. "file:///D:/rawrxd"
    std::string rootPath;                      // e.g. "D:/rawrxd"
};

// ============================================================================
// LSP SERVER STATE MACHINE
// ============================================================================
enum class ServerState {
    Created      = 0,
    Initializing = 1,
    Running      = 2,
    ShuttingDown = 3,
    Stopped      = 4,
    Error        = 5
};

// ============================================================================
// REQUEST HANDLER SIGNATURE
// ============================================================================
// Handler receives (id, method, params-json) and returns a result-json.
// If the handler returns std::nullopt, the server sends an error response.
using RequestHandler = std::function<
    std::optional<nlohmann::json>(int id, const std::string& method, const nlohmann::json& params)
>;

// Notification handler: no return (fire-and-forget).
using NotificationHandler = std::function<
    void(const std::string& method, const nlohmann::json& params)
>;

// ============================================================================
// RawrXDLSPServer — THE MAIN CLASS
// ============================================================================
class RawrXDLSPServer {
public:
    // ---- Lifecycle ----
    RawrXDLSPServer();
    ~RawrXDLSPServer();

    // Non-copyable, non-movable (owns threads + handles)
    RawrXDLSPServer(const RawrXDLSPServer&)            = delete;
    RawrXDLSPServer& operator=(const RawrXDLSPServer&) = delete;
    RawrXDLSPServer(RawrXDLSPServer&&)                 = delete;
    RawrXDLSPServer& operator=(RawrXDLSPServer&&)      = delete;

    // ---- Configuration ----
    void configure(const ServerConfig& config);
    const ServerConfig& getConfig() const { return m_config; }

    // ---- Start / Stop ----
    bool start();                          // Launches reader + dispatch threads
    void stop();                           // Graceful shutdown
    bool isRunning() const { return m_state.load() == ServerState::Running; }
    ServerState getState() const { return m_state.load(); }

    // ---- Main loop (blocking, for stdio mode) ----
    void runBlocking();                    // Blocks until shutdown received

    // ---- In-process message injection (for Win32IDE bridge) ----
    void injectMessage(const std::string& jsonRpcMessage);
    std::string pollOutgoing();            // Returns next outgoing message, or ""

    // ---- Symbol database operations ----
    void rebuildIndex();                                   // Full re-index from rootPath
    void indexSingleFile(const std::string& filePath);     // Incremental single-file index
    void removeFileFromIndex(const std::string& filePath);
    size_t getIndexedSymbolCount() const;
    size_t getTrackedFileCount() const;

    // ---- Diagnostics publishing ----
    void publishDiagnostics(const std::string& uri, const std::vector<DiagnosticEntry>& diagnostics);
    void clearDiagnostics(const std::string& uri);

    // ---- Stats ----
    ServerStats getStats() const;
    std::string getStatsString() const;

    // ---- Custom Handler Registration (LSP Bridge Extension Point) ----
    // Register a handler for a custom JSON-RPC method (e.g., "rawrxd/hotpatch/list").
    // The handler is called on the dispatch thread.
    void registerRequestHandler(const std::string& method, RequestHandler handler);
    void registerNotificationHandler(const std::string& method, NotificationHandler handler);
    void unregisterRequestHandler(const std::string& method);
    void unregisterNotificationHandler(const std::string& method);

    // ---- Public notification sending (for bridge/hotpatch layers) ----
    void sendNotification(const std::string& method, const nlohmann::json& params);

private:
    // ---- JSON-RPC 2.0 framing (Content-Length header) ----
    bool readMessage(std::string& outMessage);             // Reads from stdin / pipe
    void writeMessage(const std::string& jsonBody);        // Writes to stdout / pipe
    void writeResponse(int id, const nlohmann::json& result);
    void writeError(int id, int code, const std::string& message);
    void writeNotification(const std::string& method, const nlohmann::json& params);

    // ---- Dispatch ----
    void dispatchMessage(const std::string& rawJson);
    void handleRequest(int id, const std::string& method, const nlohmann::json& params);
    void handleNotification(const std::string& method, const nlohmann::json& params);

    // ---- LSP Request Handlers (return result JSON) ----
    nlohmann::json handleInitialize(int id, const nlohmann::json& params);
    nlohmann::json handleShutdown(int id, const nlohmann::json& params);
    nlohmann::json handleTextDocumentHover(int id, const nlohmann::json& params);
    nlohmann::json handleTextDocumentCompletion(int id, const nlohmann::json& params);
    nlohmann::json handleTextDocumentDefinition(int id, const nlohmann::json& params);
    nlohmann::json handleTextDocumentReferences(int id, const nlohmann::json& params);
    nlohmann::json handleTextDocumentDocumentSymbol(int id, const nlohmann::json& params);
    nlohmann::json handleWorkspaceSymbol(int id, const nlohmann::json& params);
    nlohmann::json handleTextDocumentSemanticTokensFull(int id, const nlohmann::json& params);

    // ---- LSP Notification Handlers ----
    void handleInitialized(const nlohmann::json& params);
    void handleExit(const nlohmann::json& params);
    void handleDidOpen(const nlohmann::json& params);
    void handleDidChange(const nlohmann::json& params);
    void handleDidClose(const nlohmann::json& params);
    void handleDidSave(const nlohmann::json& params);

    // ---- Symbol indexing engine ----
    void indexFileContent(const std::string& filePath, const std::string& content);
    void indexFileFromDisk(const std::string& filePath);
    SymbolKind kindFromString(const std::string& kindStr) const;
    std::string kindToString(SymbolKind kind) const;
    int kindToLSPInt(SymbolKind kind) const;

    // ---- URI / path helpers ----
    std::string filePathToUri(const std::string& path) const;
    std::string uriToFilePath(const std::string& uri) const;
    bool isCodeFile(const std::string& path) const;

    // ---- Semantic token encoding ----
    std::vector<uint32_t> encodeSemanticTokens(const std::string& content) const;

    // ---- Thread entry points ----
    void readerThreadFunc();
    void dispatchThreadFunc();

    // ---- State ----
    std::atomic<ServerState>           m_state{ServerState::Created};
    ServerConfig                       m_config;
    ServerStats                        m_stats;
    mutable std::mutex                 m_statsMutex;
    bool                               m_shutdownRequested = false;
    bool                               m_exitRequested     = false;

    // ---- Symbol database (readers/writer lock) ----
    std::vector<IndexedSymbol>         m_symbols;
    mutable std::shared_mutex          m_symbolMutex;

    // ---- Open documents ----
    std::unordered_map<std::string, DocumentState> m_openDocuments;  // uri → state
    mutable std::mutex                 m_docMutex;

    // ---- Message queues ----
    std::vector<std::string>           m_incomingQueue;
    std::mutex                         m_incomingMutex;
    std::condition_variable            m_incomingCV;

    std::vector<std::string>           m_outgoingQueue;
    std::mutex                         m_outgoingMutex;

    // ---- Threads ----
    std::thread                        m_readerThread;
    std::thread                        m_dispatchThread;

    // ---- Custom handlers (extension point) ----
    std::map<std::string, RequestHandler>      m_customRequestHandlers;
    std::map<std::string, NotificationHandler> m_customNotificationHandlers;

    // ---- I/O handles (for named-pipe mode) ----
    void*                              m_hPipe = nullptr;  // HANDLE, cast to void*
    
    // ---- Capability negotiation state ----
    bool                               m_clientSupportsSemanticTokens = false;
    bool                               m_clientSupportsWorkspaceSymbol = false;
    int                                m_clientProcessId = 0;
    std::string                        m_clientName;
    std::string                        m_clientVersion;
};

} // namespace LSPServer
} // namespace RawrXD
