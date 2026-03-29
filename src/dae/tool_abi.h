// ============================================================================
// tool_abi.h — DAE Tool ABI: Structured Invocation Contract
// ============================================================================
// All tools invoked by the DAE agent must satisfy this ABI.  Key guarantees:
//
//   1. Structured-only returns (ToolResult<T>) — no untyped stdout as control.
//   2. Idempotency key per invocation — replay deduplication is enforced.
//   3. Timeouts are policy-driven (ToolPolicy), not embedded in the tool.
//   4. Every error carries a typed ToolError, not a message string.
//   5. Tools must declare their SideEffectClass so the replay engine can
//      decide whether re-execution is safe.
//
// Tool registration:
//   Tools self-register via RAWRXD_REGISTER_TOOL(name, fn) at static-init.
//   The ToolRegistry is queried by the replay engine during execution.
//
// No exceptions. Fail closed.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <expected>
#include <chrono>

namespace RawrXD {
namespace DAE {

// ============================================================================
// Tool error taxonomy
// ============================================================================

enum class ToolError : uint32_t {
    None              = 0,
    NotFound          = 1,    // Tool name unknown
    InvalidArgs       = 2,    // Argument schema validation failed
    Timeout           = 3,    // Exceeded ToolPolicy::timeoutMs
    IdempotencyViolation = 4, // Key already used; replay returned cached result
    SideEffectDenied  = 5,    // Policy forbids this side-effect class in context
    ExecutionFailed   = 6,    // Tool ran but reported internal failure
    PolicyViolation   = 7,
};

// ============================================================================
// Side-effect classes — replay policy decisions
// ============================================================================

enum class SideEffectClass : uint32_t {
    ReadOnly    = 0,   // Safe to re-invoke on replay without restriction
    WriteLocal  = 1,   // Writes to shadow FS only — safe on replay
    WriteReal   = 2,   // Would write the real workspace — blocked until Commit
    Network     = 3,   // External I/O — policy-gated, not replayed by default
    Process     = 4,   // Spawns a subprocess — policy-gated
};

// ============================================================================
// ToolResult<T>
// ============================================================================

template<typename T>
using ToolResult = std::expected<T, ToolError>;

// ============================================================================
// Tool invocation context
// ============================================================================

struct ToolInvocation {
    std::string  toolName;
    std::string  idempotencyKey;   // Must be non-empty; UUID recommended
    std::string  argsJson;         // Tool-specific arguments as JSON object
    bool         dryRun = false;   // If true: validate args, do not execute
};

// ============================================================================
// Tool response envelope
// ============================================================================

struct ToolResponse {
    ToolError    status        = ToolError::None;
    std::string  resultJson;       // Structured result (tool-specific schema)
    std::string  diagnostics;      // Deterministic diagnostics (ordered, bounded)
    bool         fromCache = false; // True if result was served from idempotency cache
    std::chrono::microseconds elapsed{0};
};

using ToolResult_v = ToolResult<ToolResponse>;

// ============================================================================
// Tool function signature
// ============================================================================

using ToolFn = std::function<ToolResult_v(const ToolInvocation&)>;

// ============================================================================
// Tool descriptor
// ============================================================================

struct ToolDescriptor {
    std::string     name;
    std::string     description;   // Single-line, used for similarity lookup
    SideEffectClass sideEffects    = SideEffectClass::ReadOnly;
    ToolFn          fn;
};

// ============================================================================
// Policy — determines timeout and side-effect permissions per context
// ============================================================================

struct ToolPolicy {
    uint32_t    timeoutMs         = 5000;
    bool        allowNetwork      = false;
    bool        allowProcess      = false;
    bool        allowRealWrite    = false;   // Must be false until after Commit
};

// ============================================================================
// ToolRegistry — thread-safe, lock-free reads after init
// ============================================================================

class ToolRegistry {
public:
    static ToolRegistry& Instance();

    void Register(ToolDescriptor desc);
    const ToolDescriptor* Find(std::string_view name) const;

    /// Invoke a tool, enforcing policy and idempotency caching.
    ToolResult_v Invoke(const ToolInvocation& inv,
                        const ToolPolicy&     policy);

    /// Clear the idempotency cache (call between replay runs).
    void ClearCache();

    /// All registered tool names (sorted).
    std::vector<std::string> RegisteredNames() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    ToolRegistry();
    ~ToolRegistry();
};

// ============================================================================
// Self-registration macro
//
// Usage:
//   RAWRXD_REGISTER_TOOL("read_file", SideEffectClass::ReadOnly, [](auto& inv){
//       ...
//       return ToolResponse{ ... };
//   });
// ============================================================================

struct ToolAutoRegistrar {
    ToolAutoRegistrar(ToolDescriptor desc) {
        ToolRegistry::Instance().Register(std::move(desc));
    }
};

#define RAWRXD_REGISTER_TOOL(name_, desc_, effects_, fn_)          \
    static ::RawrXD::DAE::ToolAutoRegistrar                        \
    _rawrxd_tool_reg_##__LINE__ {                                   \
        ::RawrXD::DAE::ToolDescriptor{ name_, desc_, effects_, fn_ } \
    }

} // namespace DAE
} // namespace RawrXD
