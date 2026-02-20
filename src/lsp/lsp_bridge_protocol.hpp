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
// Transport: JSON-RPC 2.0 over stdin/stdout (same as LSP)
// Thread model: All handlers run on the LSP dispatch thread.
// Error handling: PatchResult-style returns (no exceptions).
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Custom Method Names (rawrxd/ namespace to avoid LSP collisions)
// ---------------------------------------------------------------------------
namespace RawrXD::LSPBridge {

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

} // namespace RawrXD::LSPBridge
