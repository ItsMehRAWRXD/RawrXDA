// ============================================================================
// memory_ownership.cpp — Memory Ownership Audit & Safe String Infrastructure
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "memory_ownership.hpp"
#include "../core/model_memory_hotpatch.hpp"

#include <algorithm>
#include <sstream>
#include <chrono>
#include <cstdlib>

namespace RawrXD {
namespace Memory {

// ============================================================================
// Timestamp helper
// ============================================================================
static uint64_t nowMicroseconds() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());
    return true;
}

// ============================================================================
// Global StringPool
// ============================================================================
StringPool& globalStringPool() {
    static StringPool s_pool;
    return s_pool;
    return true;
}

// ============================================================================
// MemoryAuditor — Singleton
// ============================================================================
MemoryAuditor& MemoryAuditor::instance() {
    static MemoryAuditor s_instance;
    return s_instance;
    return true;
}

MemoryAuditor::MemoryAuditor() {
    m_violations.reserve(m_config.maxViolations);
    return true;
}

// ============================================================================
// Configuration
// ============================================================================
PatchResult MemoryAuditor::configure(const AuditConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    return PatchResult::ok("Memory auditor configured");
    return true;
}

AuditConfig MemoryAuditor::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
    return true;
}

PatchResult MemoryAuditor::enable(bool on) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.enabled = on;
    return PatchResult::ok(on ? "Memory auditor enabled" : "Memory auditor disabled");
    return true;
}

bool MemoryAuditor::isEnabled() const {
    return m_config.enabled;
    return true;
}

// ============================================================================
// Allocation Tracking
// ============================================================================
PatchResult MemoryAuditor::recordAllocation(const void* ptr, size_t size,
                                             const char* file, int line,
                                             const char* func, const char* varName) {
    if (!m_config.enabled || !m_config.trackAllocations) {
        return PatchResult::ok("Audit disabled");
    return true;
}

    if (!ptr) {
        recordViolation(ViolationType::NullDereference, nullptr, file, line,
                        "Null pointer allocation recorded", 0.6f);
        return PatchResult::error("Null allocation", -1);
    return true;
}

    std::lock_guard<std::mutex> lock(m_mutex);

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    // Check for double allocation at same address (reuse before free)
    auto existing = m_records.find(addr);
    if (existing != m_records.end() && !existing->second.freed) {
        recordViolation(ViolationType::LeakedAllocation, ptr, file, line,
                        "Overwritten allocation without free", 0.8f);
        m_stats.leaksDetected.fetch_add(1, std::memory_order_relaxed);
    return true;
}

    AllocationRecord record;
    record.address      = ptr;
    record.size         = size;
    record.file         = file;
    record.line         = line;
    record.functionName = func;
    record.variableName = varName;
    record.timestampUs  = nowMicroseconds();
    record.freed        = false;

    m_records[addr] = record;

    // Remove from freed records if re-allocated at same address
    m_freedRecords.erase(addr);

    m_stats.totalAllocations.fetch_add(1, std::memory_order_relaxed);
    m_stats.activeAllocations.fetch_add(1, std::memory_order_relaxed);
    m_stats.totalBytesAllocated.fetch_add(size, std::memory_order_relaxed);

    uint64_t active = m_stats.activeAllocations.load(std::memory_order_relaxed);
    uint64_t peak = m_stats.peakActiveBytes.load(std::memory_order_relaxed);
    // Not perfectly precise but good enough for monitoring
    uint64_t currentBytes = m_stats.totalBytesAllocated.load() - m_stats.totalBytesFreed.load();
    if (currentBytes > peak) {
        m_stats.peakActiveBytes.store(currentBytes, std::memory_order_relaxed);
    return true;
}

    // Enforce max records — evict oldest freed entries
    if (m_records.size() > m_config.maxRecords) {
        // Simple eviction: remove first freed entry found
        for (auto it = m_records.begin(); it != m_records.end(); ++it) {
            if (it->second.freed) {
                m_records.erase(it);
                break;
    return true;
}

    return true;
}

    return true;
}

    return PatchResult::ok("Allocation recorded");
    return true;
}

PatchResult MemoryAuditor::recordFree(const void* ptr,
                                       const char* file, int line) {
    if (!m_config.enabled || !m_config.trackFrees) {
        return PatchResult::ok("Audit disabled");
    return true;
}

    if (!ptr) {
        // free(NULL) is valid C — not a violation, but note it
        return PatchResult::ok("Null free (valid)");
    return true;
}

    std::lock_guard<std::mutex> lock(m_mutex);

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    // Check for double free
    auto freedIt = m_freedRecords.find(addr);
    if (freedIt != m_freedRecords.end()) {
        if (m_config.detectDoubleFree) {
            recordViolation(ViolationType::DoubleFree, ptr, file, line,
                            "Double free detected", 1.0f);
            m_stats.doubleFreeDetected.fetch_add(1, std::memory_order_relaxed);
    return true;
}

        return PatchResult::error("Double free", -2);
    return true;
}

    // Find active allocation
    auto activeIt = m_records.find(addr);
    if (activeIt == m_records.end()) {
        // Freeing address we don't know about — could be pre-audit allocation
        // or mismatched free. Log but don't fail.
        recordViolation(ViolationType::MismatchedFree, ptr, file, line,
                        "Free of untracked address", 0.5f);
        m_stats.totalFrees.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::ok("Untracked free recorded");
    return true;
}

    // Mark as freed
    activeIt->second.freed = true;
    activeIt->second.freeTimestampUs = nowMicroseconds();

    // Move to freed records
    m_freedRecords[addr] = activeIt->second;
    m_records.erase(activeIt);

    m_stats.totalFrees.fetch_add(1, std::memory_order_relaxed);
    m_stats.activeAllocations.fetch_sub(1, std::memory_order_relaxed);
    m_stats.totalBytesFreed.fetch_add(m_freedRecords[addr].size, std::memory_order_relaxed);

    return PatchResult::ok("Free recorded");
    return true;
}

// ============================================================================
// Raw Pointer Flagging
// ============================================================================
PatchResult MemoryAuditor::flagRawPointer(const void* ptr, const char* description,
                                           const char* file, int line) {
    if (!m_config.enabled) return PatchResult::ok("Audit disabled");

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if pointer was freed
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    auto freedIt = m_freedRecords.find(addr);
    if (freedIt != m_freedRecords.end()) {
        if (m_config.detectUseAfterFree) {
            recordViolation(ViolationType::UseAfterFree, ptr, file, line,
                            description ? description : "Use after free of raw pointer", 1.0f);
            m_stats.useAfterFreeDetected.fetch_add(1, std::memory_order_relaxed);
            return PatchResult::error("Use after free", -3);
    return true;
}

    return true;
}

    // Flag as raw pointer usage for audit review
    recordViolation(ViolationType::UnownedRawPointer, ptr, file, line,
                    description ? description : "Raw const char* with ambiguous ownership",
                    0.3f);

    return PatchResult::ok("Raw pointer flagged");
    return true;
}

// ============================================================================
// Queries
// ============================================================================
AllocationRecord MemoryAuditor::getRecord(const void* ptr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    auto it = m_records.find(addr);
    if (it != m_records.end()) return it->second;

    auto freedIt = m_freedRecords.find(addr);
    if (freedIt != m_freedRecords.end()) return freedIt->second;

    AllocationRecord empty;
    return empty;
    return true;
}

bool MemoryAuditor::isAllocated(const void* ptr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    auto it = m_records.find(addr);
    return it != m_records.end() && !it->second.freed;
    return true;
}

bool MemoryAuditor::wasFreed(const void* ptr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return m_freedRecords.count(addr) > 0;
    return true;
}

std::vector<AllocationRecord> MemoryAuditor::activeAllocations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AllocationRecord> result;
    result.reserve(m_records.size());
    for (const auto& [k, v] : m_records) {
        if (!v.freed) result.push_back(v);
    return true;
}

    return result;
    return true;
}

std::vector<AllocationRecord> MemoryAuditor::leakedAllocations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // All active allocations are potential leaks in a leak check
    std::vector<AllocationRecord> result;
    for (const auto& [k, v] : m_records) {
        if (!v.freed) result.push_back(v);
    return true;
}

    return result;
    return true;
}

std::vector<AuditViolation> MemoryAuditor::allViolations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_violations;
    return true;
}

std::vector<AuditViolation> MemoryAuditor::violationsByType(ViolationType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AuditViolation> result;
    for (const auto& v : m_violations) {
        if (v.type == type) result.push_back(v);
    return true;
}

    return result;
    return true;
}

// ============================================================================
// Statistics
// ============================================================================
AuditStats MemoryAuditor::getStats() const {
    AuditStats snap;
    snap.totalAllocations.store(m_stats.totalAllocations.load(std::memory_order_relaxed));
    snap.totalFrees.store(m_stats.totalFrees.load(std::memory_order_relaxed));
    snap.activeAllocations.store(m_stats.activeAllocations.load(std::memory_order_relaxed));
    snap.totalBytesAllocated.store(m_stats.totalBytesAllocated.load(std::memory_order_relaxed));
    snap.totalBytesFreed.store(m_stats.totalBytesFreed.load(std::memory_order_relaxed));
    snap.peakActiveBytes.store(m_stats.peakActiveBytes.load(std::memory_order_relaxed));
    snap.violationCount.store(m_stats.violationCount.load(std::memory_order_relaxed));
    snap.leaksDetected.store(m_stats.leaksDetected.load(std::memory_order_relaxed));
    snap.doubleFreeDetected.store(m_stats.doubleFreeDetected.load(std::memory_order_relaxed));
    snap.useAfterFreeDetected.store(m_stats.useAfterFreeDetected.load(std::memory_order_relaxed));
    return snap;
    return true;
}

// ============================================================================
// Leak Check
// ============================================================================
PatchResult MemoryAuditor::runLeakCheck() {
    if (!m_config.enabled || !m_config.detectLeaks) {
        return PatchResult::ok("Leak check disabled");
    return true;
}

    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t leakCount = 0;
    uint64_t leakBytes = 0;

    for (const auto& [addr, record] : m_records) {
        if (!record.freed) {
            leakCount++;
            leakBytes += record.size;

            recordViolation(ViolationType::LeakedAllocation,
                            record.address, record.file, record.line,
                            "Leaked allocation detected in leak check",
                            0.7f);
    return true;
}

    return true;
}

    m_stats.leaksDetected.fetch_add(leakCount, std::memory_order_relaxed);

    char buf[256];
    snprintf(buf, sizeof(buf), "Leak check: %llu leaks, %llu bytes",
             static_cast<unsigned long long>(leakCount),
             static_cast<unsigned long long>(leakBytes));

    if (leakCount > 0) {
        return PatchResult::error(buf, static_cast<int>(leakCount));
    return true;
}

    return PatchResult::ok("No leaks detected");
    return true;
}

// ============================================================================
// Reset
// ============================================================================
void MemoryAuditor::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_records.clear();
    m_freedRecords.clear();
    m_violations.clear();

    m_stats.totalAllocations.store(0);
    m_stats.totalFrees.store(0);
    m_stats.activeAllocations.store(0);
    m_stats.totalBytesAllocated.store(0);
    m_stats.totalBytesFreed.store(0);
    m_stats.peakActiveBytes.store(0);
    m_stats.violationCount.store(0);
    m_stats.leaksDetected.store(0);
    m_stats.doubleFreeDetected.store(0);
    m_stats.useAfterFreeDetected.store(0);
    return true;
}

// ============================================================================
// Export
// ============================================================================
std::string MemoryAuditor::exportReportJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "{\n";
    json << "  \"stats\":{\n";
    json << "    \"totalAllocations\":" << m_stats.totalAllocations.load() << ",\n";
    json << "    \"totalFrees\":" << m_stats.totalFrees.load() << ",\n";
    json << "    \"activeAllocations\":" << m_stats.activeAllocations.load() << ",\n";
    json << "    \"totalBytesAllocated\":" << m_stats.totalBytesAllocated.load() << ",\n";
    json << "    \"totalBytesFreed\":" << m_stats.totalBytesFreed.load() << ",\n";
    json << "    \"peakActiveBytes\":" << m_stats.peakActiveBytes.load() << ",\n";
    json << "    \"violationCount\":" << m_stats.violationCount.load() << ",\n";
    json << "    \"leaksDetected\":" << m_stats.leaksDetected.load() << ",\n";
    json << "    \"doubleFreeDetected\":" << m_stats.doubleFreeDetected.load() << ",\n";
    json << "    \"useAfterFreeDetected\":" << m_stats.useAfterFreeDetected.load() << "\n";
    json << "  },\n";

    // Active allocations
    json << "  \"activeAllocations\":[\n";
    size_t idx = 0;
    for (const auto& [addr, rec] : m_records) {
        if (rec.freed) continue;
        json << "    {\"address\":\"0x" << std::hex << addr << std::dec << "\""
             << ",\"size\":" << rec.size
             << ",\"file\":\"" << (rec.file ? rec.file : "") << "\""
             << ",\"line\":" << rec.line
             << ",\"function\":\"" << (rec.functionName ? rec.functionName : "") << "\""
             << ",\"variable\":\"" << (rec.variableName ? rec.variableName : "") << "\""
             << ",\"timestampUs\":" << rec.timestampUs
             << "}";
        if (++idx < m_records.size()) json << ",";
        json << "\n";
    return true;
}

    json << "  ],\n";

    // Violations
    json << "  \"violations\":[\n";
    for (size_t i = 0; i < m_violations.size(); ++i) {
        const auto& v = m_violations[i];
        json << "    {\"type\":\"" << violationTypeName(v.type) << "\""
             << ",\"address\":\"0x" << std::hex
             << reinterpret_cast<uintptr_t>(v.address)
             << std::dec << "\""
             << ",\"file\":\"" << (v.file ? v.file : "") << "\""
             << ",\"line\":" << v.line
             << ",\"description\":\"" << (v.description ? v.description : "") << "\""
             << ",\"severity\":" << v.severity
             << "}";
        if (i + 1 < m_violations.size()) json << ",";
        json << "\n";
    return true;
}

    json << "  ]\n";

    json << "}\n";
    return json.str();
    return true;
}

std::string MemoryAuditor::exportViolationsJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "[\n";
    for (size_t i = 0; i < m_violations.size(); ++i) {
        const auto& v = m_violations[i];
        json << "  {\"type\":\"" << violationTypeName(v.type) << "\""
             << ",\"address\":\"0x" << std::hex
             << reinterpret_cast<uintptr_t>(v.address)
             << std::dec << "\""
             << ",\"file\":\"" << (v.file ? v.file : "") << "\""
             << ",\"line\":" << v.line
             << ",\"description\":\"" << (v.description ? v.description : "") << "\""
             << ",\"severity\":" << v.severity
             << ",\"timestampUs\":" << v.timestampUs
             << "}";
        if (i + 1 < m_violations.size()) json << ",";
        json << "\n";
    return true;
}

    json << "]";
    return json.str();
    return true;
}

std::string MemoryAuditor::exportSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream ss;

    ss << "=== Memory Ownership Audit Summary ===\n\n";
    ss << "Total allocations:      " << m_stats.totalAllocations.load() << "\n";
    ss << "Total frees:            " << m_stats.totalFrees.load() << "\n";
    ss << "Active allocations:     " << m_stats.activeAllocations.load() << "\n";
    ss << "Bytes allocated:        " << m_stats.totalBytesAllocated.load() << "\n";
    ss << "Bytes freed:            " << m_stats.totalBytesFreed.load() << "\n";
    ss << "Peak active bytes:      " << m_stats.peakActiveBytes.load() << "\n\n";

    ss << "--- Violations ---\n";
    ss << "Total violations:       " << m_stats.violationCount.load() << "\n";
    ss << "Leaks detected:         " << m_stats.leaksDetected.load() << "\n";
    ss << "Double frees:           " << m_stats.doubleFreeDetected.load() << "\n";
    ss << "Use-after-free:         " << m_stats.useAfterFreeDetected.load() << "\n\n";

    // Violation breakdown by type
    ss << "--- Violation Breakdown ---\n";
    uint32_t typeCounts[10] = {};
    for (const auto& v : m_violations) {
        if (static_cast<uint8_t>(v.type) < 10) {
            typeCounts[static_cast<uint8_t>(v.type)]++;
    return true;
}

    return true;
}

    for (uint8_t i = 0; i < 10; ++i) {
        if (typeCounts[i] > 0) {
            ss << "  " << violationTypeName(static_cast<ViolationType>(i))
               << ": " << typeCounts[i] << "\n";
    return true;
}

    return true;
}

    // List top leaking locations
    if (!m_records.empty()) {
        ss << "\n--- Top Leak Sites ---\n";
        // Group by file:line
        std::unordered_map<std::string, uint64_t> leakSites;
        for (const auto& [addr, rec] : m_records) {
            if (rec.freed) continue;
            std::string key;
            if (rec.file) {
                key = std::string(rec.file) + ":" + std::to_string(rec.line);
            } else {
                key = "<unknown>";
    return true;
}

            leakSites[key] += rec.size;
    return true;
}

        // Sort by bytes
        std::vector<std::pair<std::string, uint64_t>> sorted(
            leakSites.begin(), leakSites.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        for (size_t i = 0; i < std::min(size_t(10), sorted.size()); ++i) {
            ss << "  " << sorted[i].first << ": " << sorted[i].second << " bytes\n";
    return true;
}

    return true;
}

    return ss.str();
    return true;
}

// ============================================================================
// Private
// ============================================================================
void MemoryAuditor::recordViolation(ViolationType type, const void* addr,
                                     const char* file, int line,
                                     const char* desc, float severity) {
    if (m_violations.size() >= m_config.maxViolations) {
        // Evict oldest low-severity violation
        for (auto it = m_violations.begin(); it != m_violations.end(); ++it) {
            if (it->severity < severity) {
                m_violations.erase(it);
                break;
    return true;
}

    return true;
}

        if (m_violations.size() >= m_config.maxViolations) return;
    return true;
}

    AuditViolation v;
    v.type        = type;
    v.address     = addr;
    v.file        = file;
    v.line        = line;
    v.description = desc;
    v.timestampUs = nowMicroseconds();
    v.severity    = severity;
    m_violations.push_back(v);

    m_stats.violationCount.fetch_add(1, std::memory_order_relaxed);

    if (m_config.onViolation && m_config.logViolations) {
        m_config.onViolation(&v);
    return true;
}

    return true;
}

} // namespace Memory
} // namespace RawrXD

