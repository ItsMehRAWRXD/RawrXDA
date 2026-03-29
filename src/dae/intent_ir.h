// ============================================================================
// intent_ir.h — DAE Intent Intermediate Representation
// ============================================================================
// The Intent IR is the typed operation graph that describes what an agent
// wants to do to the workspace.  No workspace mutation occurs until the graph
// has been:
//   1. Validated by IntentIR::Validate()
//   2. Replayed against the ShadowFilesystem
//   3. Committed via ShadowFilesystem::Commit()
//
// Graph Shape:
//   ┌────────────────────────────────────────────────────┐
//   │                   IntentGraph                       │
//   │                                                     │
//   │  [ IntentNode ] ──causal_parents──► [ IntentNode ] │
//   │       │                                             │
//   │  op_type, target_path, payload_hash,                │
//   │  precondition_hash, idempotency_key                │
//   └────────────────────────────────────────────────────┘
//
// Determinism properties:
//   - Topological execution order is canonical (stable sort by id on ties).
//   - Idempotency keys prevent duplicate node execution on replay.
//   - Precondition hash is verified against ShadowFilesystem before apply.
//
// Versioning:
//   IntentGraph carries kSchemaVersion. Old graphs with lower versions
//   are forward-compatible to the extent that unknown op_types are skipped
//   as no-ops unless the node is marked required=true.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <expected>

#include "shadow_fs.h"   // for ContentHash, ShadowError

namespace RawrXD {
namespace DAE {

// ============================================================================
// Schema version — bump on any breaking change
// ============================================================================

constexpr uint32_t kIntentIRSchemaVersion = 1;

// ============================================================================
// Operation types
// ============================================================================

enum class OpType : uint32_t {
    // File mutations
    CreateFile   = 1,
    PatchFile    = 2,
    MovePath     = 3,
    DeletePath   = 4,
    SetMetadata  = 5,

    // Tool invocations
    RunTool      = 10,

    // Control
    NoOp         = 0xFFFF,
};

std::string_view OpTypeName(OpType t) noexcept;

// ============================================================================
// IntentNode
// ============================================================================

struct IntentNode {
    uint64_t     id              = 0;    // Monotonically increasing within graph
    OpType       opType          = OpType::NoOp;
    std::string  targetPath;             // Repo-relative; empty for RunTool
    ContentHash  preconditionHash;       // Expected hash of target before apply
    ContentHash  payloadHash;            // Hash of the mutation payload
    std::string  idempotencyKey;         // Globally unique token; replay deduplication
    std::string  payload;                // Content / diff / tool args (serialised)
    bool         required        = true; // If false: unknown opType → skip, not error

    // Causal ordering: this node must not execute before all parents complete
    std::vector<uint64_t> causalParents;
};

// ============================================================================
// IntentGraph
// ============================================================================

enum class IRError : uint32_t {
    None                    = 0,
    SchemaVersionMismatch   = 1,
    DuplicateNodeId         = 2,
    CyclicDependency        = 3,
    MissingParentRef        = 4,
    EmptyIdempotencyKey     = 5,
    PreconditionFailed      = 6,   // Hash mismatch at validation time
    UnknownRequiredOp       = 7,
};

template<typename T>
using IRResult = std::expected<T, IRError>;

class IntentGraph {
public:
    uint32_t schemaVersion = kIntentIRSchemaVersion;

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /// Append a node; assigns id automatically.
    IRResult<uint64_t> AddNode(IntentNode node);

    /// Return a node by id.
    const IntentNode* FindNode(uint64_t id) const;

    /// All nodes in insertion order.
    const std::vector<IntentNode>& Nodes() const noexcept { return m_nodes; }

    // -------------------------------------------------------------------------
    // Validation (must pass before replay)
    // -------------------------------------------------------------------------

    /// Structural validation: no cycles, no missing parents, all keys present.
    IRResult<void> Validate() const;

    /// Precondition check against a shadow FS: verifies each target file's
    /// current hash matches node.preconditionHash before committing to replay.
    IRResult<void> ValidatePreconditions(const ShadowFilesystem& fs) const;

    // -------------------------------------------------------------------------
    // Topological ordering (stable, canonical)
    // -------------------------------------------------------------------------

    /// Returns node ids in causal topological order. Stable: ties broken by id.
    IRResult<std::vector<uint64_t>> TopologicalOrder() const;

    // -------------------------------------------------------------------------
    // Serialisation (JSON-compatible text, not binary — binary via FlatBuffers TBD)
    // -------------------------------------------------------------------------

    std::string Serialise() const;
    static IRResult<IntentGraph> Deserialise(std::string_view json);

    // -------------------------------------------------------------------------
    // Fingerprint for trace similarity lookup
    // -------------------------------------------------------------------------

    /// Hash of (schema version + ordered op types + idempotency keys).
    ContentHash StructuralFingerprint() const;

private:
    std::vector<IntentNode> m_nodes;
    uint64_t                m_nextId = 1;

    IRResult<void> DetectCycles() const;
};

} // namespace DAE
} // namespace RawrXD
