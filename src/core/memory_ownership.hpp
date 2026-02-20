// ============================================================================
// memory_ownership.hpp — Memory Ownership Audit & Safe String Infrastructure
// ============================================================================
// Component 8 of 8: Eliminate raw const char* ownership ambiguity.
//
// Provides:
//   - OwnedString   – heap-owning string (replaces raw char* allocations)
//   - StringRef      – non-owning view (replaces const char* function params)
//   - OwnershipTag   – compile-time tag for ownership intent documentation
//   - MemoryAuditor  – runtime tracking of all string allocations & lifetimes
//   - StringPool     – interned string pool for frequently repeated literals
//
// Migration strategy (incremental, non-breaking):
//   Phase 1: New code uses OwnedString/StringRef
//   Phase 2: Audit existing const char* members via MemoryAuditor
//   Phase 3: Convert flagged sites one module at a time
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration from model_memory_hotpatch.hpp
struct PatchResult;

namespace RawrXD {
namespace Memory {

// ============================================================================
// OwnershipTag — compile-time intent markers
// ============================================================================
// Tag structs used to document ownership at call sites:
//
//   void consume(OwnershipTag::Owned, char* p);   // takes ownership
//   void observe(OwnershipTag::Borrowed, const char* p); // borrows, no free
//
struct OwnershipTag {
    struct Owned {};      // Caller transfers ownership to callee
    struct Borrowed {};   // Callee borrows; caller retains ownership
    struct Shared {};     // Ref-counted / pooled; both sides hold refs
    struct Static {};     // Points to .rodata; never freed
};

// ============================================================================
// StringRef — Non-owning string view (replaces const char* parameters)
// ============================================================================
// Light wrapper that carries pointer + length, null-safe.
// Does NOT own memory. Lifetime must not exceed the source.
// Compatible with const char* at construction (implicit conversion).
//
class StringRef {
public:
    constexpr StringRef() : m_data(nullptr), m_size(0) {}
    constexpr StringRef(const char* s) : m_data(s), m_size(s ? strlen_const(s) : 0) {}
    constexpr StringRef(const char* s, size_t len) : m_data(s), m_size(len) {}
    StringRef(const std::string& s) : m_data(s.c_str()), m_size(s.size()) {}

    // Access
    const char* data() const { return m_data; }
    size_t      size() const { return m_size; }
    bool        empty() const { return m_size == 0 || m_data == nullptr; }
    bool        isNull() const { return m_data == nullptr; }

    // C-string (only if underlying memory is null-terminated — caller's guarantee)
    const char* c_str() const { return m_data; }

    // Comparison
    bool operator==(const StringRef& other) const {
        if (m_size != other.m_size) return false;
        if (m_data == other.m_data) return true;
        if (!m_data || !other.m_data) return false;
        return std::memcmp(m_data, other.m_data, m_size) == 0;
    }
    bool operator!=(const StringRef& other) const { return !(*this == other); }

    // Substring (no allocation)
    StringRef substr(size_t offset, size_t count = SIZE_MAX) const {
        if (offset >= m_size) return StringRef();
        size_t actual = (count > m_size - offset) ? (m_size - offset) : count;
        return StringRef(m_data + offset, actual);
    }

    // Conversion
    std::string toString() const {
        if (!m_data) return std::string();
        return std::string(m_data, m_size);
    }

private:
    const char* m_data;
    size_t      m_size;

    // constexpr strlen for compile-time construction
    static constexpr size_t strlen_const(const char* s) {
        size_t len = 0;
        while (s && s[len]) ++len;
        return len;
    }
};

// ============================================================================
// OwnedString — Heap-owning string (replaces char* with ownership)
// ============================================================================
// Manages its own heap allocation. Move-only (no accidental copies).
// Replaces patterns like:
//   char* name = _strdup(src);  // who frees?
// With:
//   OwnedString name = OwnedString::fromCopy(src);  // RAII cleanup guaranteed
//
class OwnedString {
public:
    // Default: empty
    OwnedString() : m_data(nullptr), m_size(0), m_capacity(0) {}

    // Destructor: frees heap memory
    ~OwnedString() { release(); }

    // Move semantics (no copy)
    OwnedString(OwnedString&& other) noexcept
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    OwnedString& operator=(OwnedString&& other) noexcept {
        if (this != &other) {
            release();
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    // Delete copy operations
    OwnedString(const OwnedString&) = delete;
    OwnedString& operator=(const OwnedString&) = delete;

    // Factory: copy from raw pointer
    static OwnedString fromCopy(const char* src) {
        OwnedString s;
        if (src) {
            size_t len = std::strlen(src);
            s.allocate(len + 1);
            std::memcpy(s.m_data, src, len);
            s.m_data[len] = '\0';
            s.m_size = len;
        }
        return s;
    }

    // Factory: copy from pointer + length
    static OwnedString fromCopy(const char* src, size_t len) {
        OwnedString s;
        if (src && len > 0) {
            s.allocate(len + 1);
            std::memcpy(s.m_data, src, len);
            s.m_data[len] = '\0';
            s.m_size = len;
        }
        return s;
    }

    // Factory: from std::string
    static OwnedString fromString(const std::string& src) {
        return fromCopy(src.c_str(), src.size());
    }

    // Factory: take ownership of existing malloc'd pointer
    // WARNING: pointer must have been allocated with malloc/calloc
    static OwnedString takeOwnership(char* ptr, size_t len) {
        OwnedString s;
        s.m_data = ptr;
        s.m_size = len;
        s.m_capacity = len + 1;
        return s;
    }

    // Explicit deep clone
    OwnedString clone() const {
        return fromCopy(m_data, m_size);
    }

    // Access
    const char* c_str() const { return m_data ? m_data : ""; }
    char*       data()        { return m_data; }
    const char* data() const  { return m_data; }
    size_t      size() const  { return m_size; }
    size_t      capacity() const { return m_capacity; }
    bool        empty() const { return m_size == 0; }
    bool        isNull() const { return m_data == nullptr; }

    // Implicit conversion to StringRef (non-owning view)
    operator StringRef() const { return StringRef(m_data, m_size); }

    // Release ownership — returns raw pointer, caller must free
    char* release() {
        char* p = m_data;
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
        return p;
    }

    // Comparison
    bool operator==(const OwnedString& other) const {
        if (m_size != other.m_size) return false;
        if (m_data == other.m_data) return true;
        if (!m_data || !other.m_data) return false;
        return std::memcmp(m_data, other.m_data, m_size) == 0;
    }
    bool operator!=(const OwnedString& other) const { return !(*this == other); }

private:
    char*  m_data;
    size_t m_size;
    size_t m_capacity;

    void allocate(size_t bytes) {
        m_data = static_cast<char*>(std::malloc(bytes));
        m_capacity = bytes;
    }
};

// ============================================================================
// StringPool — Interned string pool for repeated literals
// ============================================================================
// Canonical storage for strings that appear many times (lock names, subsystem
// names, error messages). Returns stable const char* that is valid for the
// pool's lifetime.
//
class StringPool {
public:
    StringPool() = default;
    ~StringPool() = default;

    // Intern a string — returns stable pointer into pool
    const char* intern(const char* s) {
        if (!s) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string key(s);
        auto it = m_pool.find(key);
        if (it != m_pool.end()) {
            it->second.refCount++;
            return it->second.data.c_str();
        }
        InternedEntry entry;
        entry.data = key;
        entry.refCount = 1;
        auto [ins, ok] = m_pool.emplace(key, std::move(entry));
        return ins->second.data.c_str();
    }

    // Intern with ref counting release
    void release(const char* s) {
        if (!s) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pool.find(std::string(s));
        if (it == m_pool.end()) return;
        if (--it->second.refCount == 0) {
            m_pool.erase(it);
        }
    }

    // Check if a string is interned
    bool contains(const char* s) const {
        if (!s) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pool.count(std::string(s)) > 0;
    }

    // Number of unique interned strings
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pool.size();
    }

    // Total memory consumed by pool strings
    size_t totalBytes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t total = 0;
        for (const auto& [k, v] : m_pool) {
            total += v.data.capacity() + sizeof(InternedEntry);
        }
        return total;
    }

private:
    struct InternedEntry {
        std::string data;
        uint32_t    refCount = 0;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, InternedEntry> m_pool;
};

// ============================================================================
// AllocationRecord — tracks a single string allocation site
// ============================================================================
struct AllocationRecord {
    const void* address      = nullptr;  // Heap address
    size_t      size         = 0;        // Allocation size in bytes
    const char* file         = nullptr;  // Source file (__FILE__)
    int         line         = 0;        // Source line (__LINE__)
    const char* functionName = nullptr;  // Function name
    const char* variableName = nullptr;  // Variable / field name
    uint64_t    timestampUs  = 0;        // Allocation time
    bool        freed        = false;    // Whether it has been freed
    uint64_t    freeTimestampUs = 0;     // When freed (if freed)
};

// ============================================================================
// AuditViolation — flagged ownership issue
// ============================================================================
enum class ViolationType : uint8_t {
    DoubleFree          = 0,
    UseAfterFree        = 1,
    LeakedAllocation    = 2,
    NullDereference     = 3,
    UnownedRawPointer   = 4,  // raw const char* with ambiguous ownership
    BufferOverread      = 5,
    MismatchedFree      = 6,  // malloc'd but delete'd, or vice versa
    DanglingReference   = 7,  // StringRef outlived source
    PoolLeakage         = 8,  // Interned string never released
    OwnershipTransfer   = 9   // Ownership moved but original still used
};

static inline const char* violationTypeName(ViolationType t) {
    switch (t) {
        case ViolationType::DoubleFree:        return "DoubleFree";
        case ViolationType::UseAfterFree:      return "UseAfterFree";
        case ViolationType::LeakedAllocation:   return "LeakedAllocation";
        case ViolationType::NullDereference:    return "NullDereference";
        case ViolationType::UnownedRawPointer:  return "UnownedRawPointer";
        case ViolationType::BufferOverread:     return "BufferOverread";
        case ViolationType::MismatchedFree:     return "MismatchedFree";
        case ViolationType::DanglingReference:  return "DanglingReference";
        case ViolationType::PoolLeakage:        return "PoolLeakage";
        case ViolationType::OwnershipTransfer:  return "OwnershipTransfer";
    }
    return "Unknown";
}

struct AuditViolation {
    ViolationType type        = ViolationType::LeakedAllocation;
    const void*   address     = nullptr;
    const char*   file        = nullptr;
    int           line        = 0;
    const char*   description = nullptr;
    uint64_t      timestampUs = 0;
    float         severity    = 0.0f;  // 0.0 = info, 1.0 = critical
};

// ============================================================================
// AuditConfig — configuration for the memory auditor
// ============================================================================
struct AuditConfig {
    bool     enabled                 = true;
    bool     trackAllocations        = true;
    bool     trackFrees              = true;
    bool     detectLeaks             = true;
    bool     detectDoubleFree        = true;
    bool     detectUseAfterFree      = true;
    bool     logViolations           = true;
    size_t   maxRecords              = 65536;
    size_t   maxViolations           = 4096;
    float    leakCheckIntervalSec    = 30.0f;
    // Violation callback (function pointer, not std::function)
    void (*onViolation)(const AuditViolation*) = nullptr;
};

// ============================================================================
// AuditStats — aggregate statistics
// ============================================================================
struct AuditStats {
    std::atomic<uint64_t> totalAllocations{0};
    std::atomic<uint64_t> totalFrees{0};
    std::atomic<uint64_t> activeAllocations{0};
    std::atomic<uint64_t> totalBytesAllocated{0};
    std::atomic<uint64_t> totalBytesFreed{0};
    std::atomic<uint64_t> peakActiveBytes{0};
    std::atomic<uint64_t> violationCount{0};
    std::atomic<uint64_t> leaksDetected{0};
    std::atomic<uint64_t> doubleFreeDetected{0};
    std::atomic<uint64_t> useAfterFreeDetected{0};
};

// ============================================================================
// MemoryAuditor — Runtime tracking of string allocations & lifetimes
// ============================================================================
// Singleton that instruments the codebase to detect ownership violations.
// Enable in debug builds; disable or sample in release.
//
// Usage:
//   RAWRXD_AUDIT_ALLOC(ptr, size);       // register allocation
//   RAWRXD_AUDIT_FREE(ptr);              // register deallocation
//   RAWRXD_AUDIT_RAW_USAGE(ptr, "msg");  // flag suspicious raw pointer
//
class MemoryAuditor {
public:
    static MemoryAuditor& instance();

    // Configuration
    PatchResult configure(const AuditConfig& config);
    AuditConfig getConfig() const;
    PatchResult enable(bool on);
    bool isEnabled() const;

    // Allocation tracking
    PatchResult recordAllocation(const void* ptr, size_t size,
                                  const char* file, int line,
                                  const char* func, const char* varName);
    PatchResult recordFree(const void* ptr,
                            const char* file, int line);

    // Raw pointer flagging — marks a const char* site for audit review
    PatchResult flagRawPointer(const void* ptr, const char* description,
                                const char* file, int line);

    // Queries
    AllocationRecord getRecord(const void* ptr) const;
    bool isAllocated(const void* ptr) const;
    bool wasFreed(const void* ptr) const;
    std::vector<AllocationRecord> activeAllocations() const;
    std::vector<AllocationRecord> leakedAllocations() const;
    std::vector<AuditViolation> allViolations() const;
    std::vector<AuditViolation> violationsByType(ViolationType type) const;

    // Statistics
    AuditStats getStats() const;

    // Leak check — scans for allocations without matching free
    PatchResult runLeakCheck();

    // Reset
    void reset();

    // Export
    std::string exportReportJson() const;
    std::string exportViolationsJson() const;
    std::string exportSummary() const;

private:
    MemoryAuditor();
    ~MemoryAuditor() = default;
    MemoryAuditor(const MemoryAuditor&) = delete;
    MemoryAuditor& operator=(const MemoryAuditor&) = delete;

    void recordViolation(ViolationType type, const void* addr,
                         const char* file, int line,
                         const char* desc, float severity);

    mutable std::mutex m_mutex;
    AuditConfig m_config;
    AuditStats  m_stats;

    // address → record
    std::unordered_map<uintptr_t, AllocationRecord> m_records;
    // freed addresses kept for double-free/use-after-free detection
    std::unordered_map<uintptr_t, AllocationRecord> m_freedRecords;
    // violations
    std::vector<AuditViolation> m_violations;
};

// ============================================================================
// Instrumentation Macros
// ============================================================================
// Gate with a compile-time flag so release builds have zero overhead.
//
#ifdef RAWRXD_MEMORY_AUDIT
    #define RAWRXD_AUDIT_ALLOC(ptr, size) \
        ::RawrXD::Memory::MemoryAuditor::instance().recordAllocation( \
            (ptr), (size), __FILE__, __LINE__, __FUNCTION__, #ptr)

    #define RAWRXD_AUDIT_FREE(ptr) \
        ::RawrXD::Memory::MemoryAuditor::instance().recordFree( \
            (ptr), __FILE__, __LINE__)

    #define RAWRXD_AUDIT_RAW_USAGE(ptr, desc) \
        ::RawrXD::Memory::MemoryAuditor::instance().flagRawPointer( \
            (ptr), (desc), __FILE__, __LINE__)
#else
    #define RAWRXD_AUDIT_ALLOC(ptr, size)       ((void)0)
    #define RAWRXD_AUDIT_FREE(ptr)              ((void)0)
    #define RAWRXD_AUDIT_RAW_USAGE(ptr, desc)   ((void)0)
#endif

// ============================================================================
// Global StringPool accessor
// ============================================================================
StringPool& globalStringPool();

} // namespace Memory
} // namespace RawrXD
