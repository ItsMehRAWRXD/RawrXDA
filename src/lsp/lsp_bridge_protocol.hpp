// ============================================================================
// lsp_bridge_protocol.hpp — LSP Bridge Protocol Definitions
// ============================================================================
// Defines custom JSON-RPC methods that connect the ASM engine and the
// three-layer hotpatch system to the IDE layer.
//
// Standard LSP methods are handled by RawrXD_LSPServer.
// These custom methods extend the protocol for GGUF/hotpatch workspace:
//
//   rawrxd/hotpatch/list         — enumerate active patches across all layers
//   rawrxd/hotpatch/apply        — apply a patch via UnifiedHotpatchManager
//   rawrxd/hotpatch/revert       — revert a previously applied patch
//   rawrxd/hotpatch/diagnostics  — get hotpatch conflict/validation diagnostics
//   rawrxd/gguf/modelInfo        — query GGUF model metadata
//   rawrxd/gguf/tensorList       — enumerate tensors in a loaded model
//   rawrxd/gguf/validate         — run structural validation on a GGUF file
//   rawrxd/workspace/symbols     — get hotpatch-aware symbol table
//   rawrxd/workspace/stats       — get hotpatch manager statistics
//
// Transport: JSON-RPC 2.0 over stdin/stdout or named pipes
// Thread model: All handlers run on the LSP dispatch thread.
// Error handling: PatchResult-style returns (no exceptions).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#ifdef ERROR_INTERNAL_ERROR
#undef ERROR_INTERNAL_ERROR
#endif

// ---------------------------------------------------------------------------
// JSON-RPC 2.0 Constants
// ---------------------------------------------------------------------------
namespace RawrXD::LSPBridge {

constexpr const char* JSONRPC_VERSION = "2.0";

// Standard LSP error codes
constexpr int ERROR_PARSE_ERROR      = -32700;
constexpr int ERROR_INVALID_REQUEST  = -32600;
constexpr int ERROR_METHOD_NOT_FOUND = -32601;
constexpr int ERROR_INVALID_PARAMS   = -32602;
constexpr int ERROR_INTERNAL_ERROR   = -32603;

// LSP-specific error codes
constexpr int ERROR_SERVER_NOT_INITIALIZED = -32002;
constexpr int ERROR_UNKNOWN_ERROR_CODE     = -32001;
constexpr int ERROR_REQUEST_FAILED         = -32803;
constexpr int ERROR_REQUEST_CANCELLED      = -32800;
constexpr int ERROR_CONTENT_MODIFIED       = -32801;

// RawrXD custom error codes (4000-4999 range)
constexpr int ERROR_HOTPATCH_FAILED        = -4001;
constexpr int ERROR_GGUF_INVALID           = -4002;
constexpr int ERROR_SYMBOL_NOT_FOUND       = -4003;
constexpr int ERROR_LAYER_NOT_FOUND        = -4004;
constexpr int ERROR_PATCH_CONFLICT         = -4005;

// ---------------------------------------------------------------------------
// Custom Method Names (rawrxd/ namespace to avoid LSP collisions)
// ---------------------------------------------------------------------------

// Hotpatch layer methods
constexpr const char* METHOD_HOTPATCH_LIST        = "rawrxd/hotpatch/list";
constexpr const char* METHOD_HOTPATCH_APPLY       = "rawrxd/hotpatch/apply";
constexpr const char* METHOD_HOTPATCH_REVERT      = "rawrxd/hotpatch/revert";
constexpr const char* METHOD_HOTPATCH_DIAGNOSTICS = "rawrxd/hotpatch/diagnostics";

// GGUF model methods
constexpr const char* METHOD_GGUF_MODEL_INFO      = "rawrxd/gguf/modelInfo";
constexpr const char* METHOD_GGUF_TENSOR_LIST     = "rawrxd/gguf/tensorList";
constexpr const char* METHOD_GGUF_VALIDATE        = "rawrxd/gguf/validate";

// Workspace methods
constexpr const char* METHOD_WORKSPACE_SYMBOLS    = "rawrxd/workspace/symbols";
constexpr const char* METHOD_WORKSPACE_STATS      = "rawrxd/workspace/stats";

// Notifications (server → client)
constexpr const char* NOTIFY_HOTPATCH_EVENT       = "rawrxd/hotpatch/event";
constexpr const char* NOTIFY_GGUF_LOAD_PROGRESS   = "rawrxd/gguf/loadProgress";
constexpr const char* NOTIFY_DIAGNOSTIC_REFRESH   = "rawrxd/diagnostics/refresh";

// ---------------------------------------------------------------------------
// Hotpatch Layer Identifiers
// ---------------------------------------------------------------------------
enum class HotpatchLayer : uint8_t {
    Memory  = 0,    // Layer 1: RAM patching (VirtualProtect)
    Byte    = 1,    // Layer 2: GGUF binary modification (mmap)
    Server  = 2,    // Layer 3: Inference request/response injection
    All     = 255,  // Query all layers
};

// ---------------------------------------------------------------------------
// Hotpatch Symbol Kind — extends LSP SymbolKind for hotpatch-specific items
// ---------------------------------------------------------------------------
enum class HotpatchSymbolKind : uint8_t {
    MemoryPatch     = 0,    // Active memory patch
    BytePatch       = 1,    // Byte-level GGUF modification
    ServerPatch     = 2,    // Server transform function
    GGUFTensor      = 3,    // Model tensor
    GGUFMetadata    = 4,    // Model metadata key
    PatchPreset     = 5,    // Saved preset collection
    InjectionPoint  = 6,    // Server injection point (pre/post/stream)
    ProxyRewrite    = 7,    // Proxy hotpatcher rewrite rule
};

// ---------------------------------------------------------------------------
// Hotpatch Diagnostic Severity (mirrors LSP DiagnosticSeverity)
// ---------------------------------------------------------------------------
enum class HotpatchDiagSeverity : uint8_t {
    Error       = 1,
    Warning     = 2,
    Information = 3,
    Hint        = 4,
};

// ---------------------------------------------------------------------------
// Hotpatch Diagnostic Codes — machine-readable codes for IDE quick-fixes
// ---------------------------------------------------------------------------
enum class HotpatchDiagCode : uint16_t {
    // Memory layer
    OverlappingPatch        = 1001,
    UnalignedAccess         = 1002,
    ProtectionFailure       = 1003,
    PatchSizeTooLarge       = 1004,
    
    // Byte layer
    OffsetOutOfBounds       = 2001,
    PatternNotFound         = 2002,
    FileMappingFailure      = 2003,
    SizeMismatch            = 2004,
    GGUFMagicInvalid        = 2005,
    GGUFVersionMismatch     = 2006,
    TensorChecksum          = 2007,
    
    // Server layer
    DuplicatePatchName      = 3001,
    NullTransform           = 3002,
    TransformTimeout        = 3003,
    InjectionConflict       = 3004,
    
    // Cross-layer
    PresetCorrupted         = 4001,
    LayerConflict           = 4002,
    StateDesync             = 4003,
};

// ---------------------------------------------------------------------------
// HotpatchSymbolEntry — Symbol exported to the LSP bridge
// ---------------------------------------------------------------------------
struct HotpatchSymbolEntry {
    const char*         name;           // Symbol name
    HotpatchSymbolKind  kind;           // Type classification
    HotpatchLayer       layer;          // Which hotpatch layer owns it
    const char*         detail;         // Human-readable description
    const char*         filePath;       // Associated file (GGUF path, source, etc.)
    uint32_t            line;           // Line in source (0 if N/A)
    uint32_t            startChar;      // Start column (0 if N/A)
    uint32_t            endChar;        // End column (0 if N/A)
    uint64_t            address;        // Memory address (for memory patches)
    size_t              size;           // Patch/tensor size in bytes
    uint64_t            hash;           // FNV-1a hash for fast lookup
    bool                active;         // Whether currently applied
};

// ---------------------------------------------------------------------------
// HotpatchDiagEntry — Diagnostic from the hotpatch validation engine
// ---------------------------------------------------------------------------
struct HotpatchDiagEntry {
    HotpatchDiagSeverity severity;
    HotpatchDiagCode     code;
    const char*          message;
    const char*          source;        // "rawrxd-memory", "rawrxd-byte", "rawrxd-server"
    HotpatchLayer        layer;
    const char*          relatedPatch;  // Name of the patch that caused the issue
    uint64_t             address;       // Relevant address (0 if N/A)
    size_t               offset;        // Relevant file offset (0 if N/A)
};

// ---------------------------------------------------------------------------
// GGUFModelSummary — Model info returned by gguf/modelInfo
// ---------------------------------------------------------------------------
struct GGUFModelSummary {
    const char*     filePath;
    uint32_t        version;            // GGUF version
    uint64_t        fileSize;
    uint32_t        tensorCount;
    uint32_t        metadataCount;
    const char*     architecture;       // e.g., "llama", "gpt2"
    uint32_t        contextLength;
    uint32_t        embeddingLength;
    uint32_t        headCount;
    uint32_t        layerCount;
    const char*     quantType;          // e.g., "Q4_K_M"
    uint64_t        paramCount;         // Total parameters
};

// ---------------------------------------------------------------------------
// GGUFTensorEntry — Single tensor descriptor
// ---------------------------------------------------------------------------
struct GGUFTensorEntry {
    const char*     name;               // Tensor name (e.g., "blk.0.attn_q.weight")
    uint32_t        type;               // GGML type enum
    const char*     typeName;           // Human-readable type (e.g., "Q4_K_M")
    uint64_t        offset;             // File offset
    uint64_t        size;               // Size in bytes
    uint32_t        dims[4];            // Dimensions (ne[0..3])
    uint32_t        nDims;              // Number of dimensions used
    uint64_t        hash;               // FNV-1a hash of name
};

// ---------------------------------------------------------------------------
// LSP Bridge Capability Flags — advertised in initialize response
// ---------------------------------------------------------------------------
struct LSPBridgeCapabilities {
    bool hotpatchList           : 1;
    bool hotpatchApply          : 1;
    bool hotpatchRevert         : 1;
    bool hotpatchDiagnostics    : 1;
    bool ggufModelInfo          : 1;
    bool ggufTensorList         : 1;
    bool ggufValidate           : 1;
    bool workspaceSymbols       : 1;
    bool workspaceStats         : 1;
    bool eventNotifications     : 1;
};

// ---------------------------------------------------------------------------
// PatchResult — Universal result type for hotpatch operations
// ---------------------------------------------------------------------------
struct PatchResult {
    bool        success;
    const char* detail;
    int         errorCode;
    
    static PatchResult ok(const char* msg = "OK") {
        PatchResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }
    
    static PatchResult error(const char* msg, int code = -1) {
        PatchResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ---------------------------------------------------------------------------
// HotpatchEvent — Event structure for callback notifications
// ---------------------------------------------------------------------------
struct HotpatchEvent {
    enum class Type : uint8_t {
        MemoryPatchApplied = 0,
        MemoryPatchReverted = 1,
        BytePatchApplied = 2,
        BytePatchFailed = 3,
        ServerPatchAdded = 4,
        ServerPatchRemoved = 5,
        PresetLoaded = 6,
        PresetSaved = 7
    };
    
    Type        type;
    uint64_t    timestamp;      // Milliseconds since epoch
    uint64_t    sequenceId;     // Monotonic event counter
    const char* detail;         // Human-readable description
    const char* patchName;      // Name of the affected patch
    HotpatchLayer layer;        // Affected layer
    uint64_t    address;        // Relevant address (for memory patches)
    size_t      size;           // Relevant size
};

// ---------------------------------------------------------------------------
// LSPBridgeRequest — Parsed incoming request
// ---------------------------------------------------------------------------
struct LSPBridgeRequest {
    int         id;             // JSON-RPC request ID (-1 for notifications)
    const char* method;         // Method name
    const char* params;         // Raw JSON params string
    bool        isNotification; // True if no response expected
};

// ---------------------------------------------------------------------------
// LSPBridgeResponse — Outgoing response structure
// ---------------------------------------------------------------------------
struct LSPBridgeResponse {
    int         id;             // Must match request ID
    bool        isError;        // True if error response
    int         errorCode;      // JSON-RPC error code (only if isError)
    const char* errorMessage;   // Error message (only if isError)
    const char* result;         // Result JSON string (only if !isError)
};

// ---------------------------------------------------------------------------
// Utility Functions
// ---------------------------------------------------------------------------

// FNV-1a hash for fast symbol lookup
inline uint64_t fnv1a_hash(const char* str) {
    uint64_t hash = 14695981039346656037ULL;
    if (str) {
        while (*str) {
            hash ^= static_cast<uint8_t>(*str++);
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

// Convert hotpatch layer to string
inline const char* layerToString(HotpatchLayer layer) {
    switch (layer) {
        case HotpatchLayer::Memory: return "memory";
        case HotpatchLayer::Byte:   return "byte";
        case HotpatchLayer::Server: return "server";
        case HotpatchLayer::All:    return "all";
        default:                    return "unknown";
    }
}

// Parse layer from string
inline HotpatchLayer layerFromString(const char* str) {
    if (!str) return HotpatchLayer::All;
    if (str[0] == 'm' || str[0] == 'M') return HotpatchLayer::Memory;
    if (str[0] == 'b' || str[0] == 'B') return HotpatchLayer::Byte;
    if (str[0] == 's' || str[0] == 'S') return HotpatchLayer::Server;
    return HotpatchLayer::All;
}

} // namespace RawrXD::LSPBridge
