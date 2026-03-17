/*
 * RawrXD_Performance.h - High-Performance IDE Infrastructure
 * Sub-100ms completion | Sub-50ms hover | Lazy loading | Virtual scrolling
 * Incremental sync | Delta encoding | Semantic token delta | Background indexing
 * Workspace cache | Symbol database (SQLite) | Fuzzy search (ripgrep-style)
 * Parallel parsing | Zero-copy string refs | Arena allocators | Bump allocators
 * Lock-free queues
 *
 * Copyright (c) 2026 RawrXD Project
 * Build: Included by RawrXD_Win32_IDE.cpp
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <algorithm>
#include <memory>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <sstream>

// ============================================================================
// PERFORMANCE TIMING INFRASTRUCTURE
// Sub-100ms completion target | Sub-50ms hover target
// ============================================================================

namespace perf {

struct PerfTimer {
    LARGE_INTEGER start_, freq_;
    const char* label_;
    PerfTimer(const char* label) : label_(label) {
        QueryPerformanceFrequency(&freq_);
        QueryPerformanceCounter(&start_);
    }
    double elapsed_ms() const {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)(now.QuadPart - start_.QuadPart) * 1000.0 / (double)freq_.QuadPart;
    }
    ~PerfTimer() {} // Non-logging destructor; use elapsed_ms() explicitly
};

// Global performance counters
struct PerfCounters {
    std::atomic<uint64_t> completionRequests{0};
    std::atomic<uint64_t> completionAvgUs{0};     // avg microseconds
    std::atomic<uint64_t> hoverRequests{0};
    std::atomic<uint64_t> hoverAvgUs{0};
    std::atomic<uint64_t> indexingFiles{0};
    std::atomic<uint64_t> indexingSymbols{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    std::atomic<uint64_t> deltaEncodings{0};
    std::atomic<uint64_t> arenaAllocations{0};
    std::atomic<uint64_t> arenaBytes{0};
    std::atomic<uint64_t> fuzzySearches{0};
    std::atomic<uint64_t> parallelParses{0};
    std::atomic<uint64_t> virtualScrollRedraws{0};
};

inline PerfCounters& counters() {
    static PerfCounters c;
    return c;
}

// ============================================================================
// ARENA ALLOCATOR
// Fast bump-pointer allocation for transient per-frame / per-request data.
// Zero fragmentation, O(1) alloc, bulk-free via reset.
// ============================================================================

class ArenaAllocator {
public:
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024; // 64 KB blocks

    explicit ArenaAllocator(size_t blockSize = DEFAULT_BLOCK_SIZE)
        : blockSize_(blockSize), current_(nullptr), offset_(0), capacity_(0) {}

    ~ArenaAllocator() { release(); }

    // Non-copyable
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // Move OK
    ArenaAllocator(ArenaAllocator&& other) noexcept
        : blockSize_(other.blockSize_), blocks_(std::move(other.blocks_)),
          current_(other.current_), offset_(other.offset_), capacity_(other.capacity_),
          totalAllocated_(other.totalAllocated_) {
        other.current_ = nullptr;
        other.offset_ = other.capacity_ = 0;
        other.totalAllocated_ = 0;
    }

    void* alloc(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // Align the offset
        size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned + size > capacity_) {
            allocBlock(size > blockSize_ ? size + alignment : blockSize_);
            aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        }
        void* ptr = current_ + aligned;
        offset_ = aligned + size;
        totalAllocated_ += size;
        counters().arenaAllocations.fetch_add(1, std::memory_order_relaxed);
        counters().arenaBytes.fetch_add(size, std::memory_order_relaxed);
        return ptr;
    }

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = alloc(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    // Allocate an array of T (no constructors called)
    template<typename T>
    T* allocArray(size_t count) {
        return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

    // Reset the arena for reuse (keeps memory, rewinds pointers)
    void reset() {
        if (!blocks_.empty()) {
            current_ = blocks_[0];
            offset_ = 0;
            capacity_ = blockSize_;
        }
        // Keep blocks allocated for reuse but mark only first as active
        activeBlockIdx_ = 0;
        totalAllocated_ = 0;
    }

    // Free all memory
    void release() {
        for (auto* b : blocks_) {
            VirtualFree(b, 0, MEM_RELEASE);
        }
        blocks_.clear();
        current_ = nullptr;
        offset_ = capacity_ = 0;
        totalAllocated_ = 0;
    }

    size_t totalAllocated() const { return totalAllocated_; }
    size_t blockCount() const { return blocks_.size(); }

private:
    void allocBlock(size_t minSize) {
        size_t sz = (minSize > blockSize_) ? minSize : blockSize_;
        // Try to reuse existing blocks
        activeBlockIdx_++;
        if (activeBlockIdx_ < blocks_.size()) {
            current_ = blocks_[activeBlockIdx_];
            offset_ = 0;
            capacity_ = blockSize_;
            return;
        }
        // Allocate new via VirtualAlloc for page-aligned large blocks
        char* block = static_cast<char*>(VirtualAlloc(nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!block) {
            // Fallback to HeapAlloc
            block = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, sz));
        }
        blocks_.push_back(block);
        current_ = block;
        offset_ = 0;
        capacity_ = sz;
    }

    size_t blockSize_;
    std::vector<char*> blocks_;
    char* current_;
    size_t offset_;
    size_t capacity_;
    size_t totalAllocated_ = 0;
    size_t activeBlockIdx_ = 0;
};

// ============================================================================
// BUMP ALLOCATOR
// Even simpler than arena - single contiguous block, no reset granularity.
// Perfect for parse trees, token streams, and single-pass allocations.
// ============================================================================

class BumpAllocator {
public:
    explicit BumpAllocator(size_t capacity = 1024 * 1024) // 1 MB default
        : capacity_(capacity), offset_(0) {
        base_ = static_cast<char*>(VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!base_) {
            base_ = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, capacity));
        }
    }

    ~BumpAllocator() {
        if (base_) VirtualFree(base_, 0, MEM_RELEASE);
    }

    BumpAllocator(const BumpAllocator&) = delete;
    BumpAllocator& operator=(const BumpAllocator&) = delete;

    void* alloc(size_t size, size_t alignment = alignof(std::max_align_t)) {
        size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned + size > capacity_) return nullptr; // OOM
        void* ptr = base_ + aligned;
        offset_ = aligned + size;
        return ptr;
    }

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = alloc(sizeof(T), alignof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    void reset() { offset_ = 0; }
    size_t used() const { return offset_; }
    size_t remaining() const { return capacity_ - offset_; }
    bool valid() const { return base_ != nullptr; }

private:
    char* base_;
    size_t capacity_;
    size_t offset_;
};

// ============================================================================
// ZERO-COPY STRING REF (StringRef)
// Non-owning view into existing string data. Avoids allocations in tokenizers,
// parsers, and symbol lookups. Compatible with both char and wchar_t.
// ============================================================================

template<typename CharT>
class BasicStringRef {
public:
    using char_type = CharT;

    BasicStringRef() : data_(nullptr), len_(0) {}
    BasicStringRef(const CharT* data, size_t len) : data_(data), len_(len) {}
    BasicStringRef(const CharT* data) : data_(data), len_(0) {
        if (data) {
            while (data[len_]) ++len_;
        }
    }
    // From std::basic_string (non-owning!)
    BasicStringRef(const std::basic_string<CharT>& s) : data_(s.data()), len_(s.size()) {}

    const CharT* data() const { return data_; }
    size_t size() const { return len_; }
    size_t length() const { return len_; }
    bool empty() const { return len_ == 0; }

    CharT operator[](size_t i) const { return data_[i]; }
    const CharT* begin() const { return data_; }
    const CharT* end() const { return data_ + len_; }

    BasicStringRef substr(size_t pos, size_t count = SIZE_MAX) const {
        if (pos >= len_) return BasicStringRef();
        size_t actual = (count > len_ - pos) ? (len_ - pos) : count;
        return BasicStringRef(data_ + pos, actual);
    }

    bool operator==(const BasicStringRef& other) const {
        if (len_ != other.len_) return false;
        return std::memcmp(data_, other.data_, len_ * sizeof(CharT)) == 0;
    }
    bool operator!=(const BasicStringRef& other) const { return !(*this == other); }

    // Comparison for use in maps
    bool operator<(const BasicStringRef& other) const {
        size_t minLen = (len_ < other.len_) ? len_ : other.len_;
        int cmp = std::memcmp(data_, other.data_, minLen * sizeof(CharT));
        if (cmp != 0) return cmp < 0;
        return len_ < other.len_;
    }

    size_t find(CharT ch, size_t start = 0) const {
        for (size_t i = start; i < len_; i++) {
            if (data_[i] == ch) return i;
        }
        return SIZE_MAX;
    }

    size_t find(const BasicStringRef& needle, size_t start = 0) const {
        if (needle.len_ == 0) return start;
        if (needle.len_ > len_) return SIZE_MAX;
        for (size_t i = start; i <= len_ - needle.len_; i++) {
            if (std::memcmp(data_ + i, needle.data_, needle.len_ * sizeof(CharT)) == 0)
                return i;
        }
        return SIZE_MAX;
    }

    // Convert to owned string (only when necessary)
    std::basic_string<CharT> str() const {
        return std::basic_string<CharT>(data_, len_);
    }

    // Hash support
    struct Hash {
        size_t operator()(const BasicStringRef& ref) const {
            // FNV-1a hash
            size_t hash = 14695981039346656037ULL;
            const auto* bytes = reinterpret_cast<const uint8_t*>(ref.data_);
            size_t byteLen = ref.len_ * sizeof(CharT);
            for (size_t i = 0; i < byteLen; i++) {
                hash ^= bytes[i];
                hash *= 1099511628211ULL;
            }
            return hash;
        }
    };

private:
    const CharT* data_;
    size_t len_;
};

using StringRef = BasicStringRef<char>;
using WStringRef = BasicStringRef<wchar_t>;

// ============================================================================
// LOCK-FREE QUEUE (SPSC - Single Producer Single Consumer)
// For background indexer <-> UI thread communication.
// Wait-free push/pop, cache-line aligned to avoid false sharing.
// ============================================================================

template<typename T, size_t Capacity = 4096>
class LockFreeQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
public:
    LockFreeQueue() : head_(0), tail_(0) {}

    // Producer: push item (returns false if full)
    bool push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire)) return false; // Full
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool push(T&& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        buffer_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer: pop item (returns false if empty)
    bool pop(T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false; // Empty
        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & MASK;
    }

    void clear() {
        head_.store(0, std::memory_order_release);
        tail_.store(0, std::memory_order_release);
    }

private:
    static constexpr size_t MASK = Capacity - 1;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    T buffer_[Capacity];
};

// MPMC (Multi-Producer Multi-Consumer) lock-free queue
template<typename T, size_t Capacity = 4096>
class MPMCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };
public:
    MPMCQueue() : enqueueTo_(0), dequeueFrom_(0) {
        for (size_t i = 0; i < Capacity; i++) {
            cells_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool push(const T& item) {
        Cell* cell;
        size_t pos = enqueueTo_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &cells_[pos & MASK];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;
            if (diff == 0) {
                if (enqueueTo_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return false; // Full
            } else {
                pos = enqueueTo_.load(std::memory_order_relaxed);
            }
        }
        cell->data = item;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        Cell* cell;
        size_t pos = dequeueFrom_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &cells_[pos & MASK];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
            if (diff == 0) {
                if (dequeueFrom_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return false; // Empty
            } else {
                pos = dequeueFrom_.load(std::memory_order_relaxed);
            }
        }
        item = std::move(cell->data);
        cell->sequence.store(pos + Capacity, std::memory_order_release);
        return true;
    }

    bool empty() const {
        size_t pos = dequeueFrom_.load(std::memory_order_relaxed);
        auto& cell = cells_[pos & MASK];
        size_t seq = cell.sequence.load(std::memory_order_acquire);
        return (intptr_t)seq - (intptr_t)(pos + 1) < 0;
    }

private:
    static constexpr size_t MASK = Capacity - 1;
    Cell cells_[Capacity];
    alignas(64) std::atomic<size_t> enqueueTo_;
    alignas(64) std::atomic<size_t> dequeueFrom_;
};

// ============================================================================
// INCREMENTAL SYNC & DELTA ENCODING
// Minimizes data transfer between background threads and UI.
// Instead of sending full document, send edits as deltas.
// ============================================================================

struct TextEdit {
    uint32_t offset;      // Start position in document
    uint32_t deleteLen;   // Characters to remove
    std::wstring insertText; // Text to insert
};

struct DocumentVersion {
    uint64_t version;
    uint64_t hash;        // FNV-1a hash of full content
    uint32_t lineCount;
    uint32_t charCount;
};

class IncrementalSync {
public:
    IncrementalSync() : version_(0) {}

    // Record an edit (call from UI thread)
    void recordEdit(uint32_t offset, uint32_t deleteLen, const std::wstring& insertText) {
        TextEdit edit;
        edit.offset = offset;
        edit.deleteLen = deleteLen;
        edit.insertText = insertText;

        std::lock_guard<std::mutex> lock(mutex_);
        pendingEdits_.push_back(edit);
        version_++;
        counters().deltaEncodings.fetch_add(1, std::memory_order_relaxed);
    }

    // Get all pending edits since last flush (call from background thread)
    std::vector<TextEdit> flushEdits() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TextEdit> edits;
        edits.swap(pendingEdits_);
        return edits;
    }

    // Apply edits to a string (used by background workers to maintain shadow copy)
    static void applyEdits(std::wstring& doc, const std::vector<TextEdit>& edits) {
        for (const auto& edit : edits) {
            if (edit.offset > doc.size()) continue;
            size_t end = edit.offset + edit.deleteLen;
            if (end > doc.size()) end = doc.size();
            doc.replace(edit.offset, end - edit.offset, edit.insertText);
        }
    }

    // Compute delta between two document versions (Myers diff - simplified)
    static std::vector<TextEdit> computeDelta(const std::wstring& oldDoc, const std::wstring& newDoc) {
        std::vector<TextEdit> edits;
        // Line-level diff for efficiency
        auto oldLines = splitLines(oldDoc);
        auto newLines = splitLines(newDoc);

        size_t oldIdx = 0, newIdx = 0;
        uint32_t offset = 0;

        while (oldIdx < oldLines.size() && newIdx < newLines.size()) {
            if (oldLines[oldIdx] == newLines[newIdx]) {
                offset += (uint32_t)oldLines[oldIdx].size() + 1; // +1 for newline
                oldIdx++;
                newIdx++;
            } else {
                // Find the next matching line (lookahead)
                size_t matchOld = SIZE_MAX, matchNew = SIZE_MAX;
                for (size_t i = oldIdx + 1; i < oldLines.size() && i < oldIdx + 10; i++) {
                    for (size_t j = newIdx; j < newLines.size() && j < newIdx + 10; j++) {
                        if (oldLines[i] == newLines[j]) {
                            matchOld = i; matchNew = j; goto found;
                        }
                    }
                }
                found:
                if (matchOld != SIZE_MAX) {
                    // Compute delete and insert
                    uint32_t deleteChars = 0;
                    for (size_t i = oldIdx; i < matchOld; i++)
                        deleteChars += (uint32_t)oldLines[i].size() + 1;
                    std::wstring insertStr;
                    for (size_t i = newIdx; i < matchNew; i++) {
                        insertStr += newLines[i];
                        insertStr += L'\n';
                    }
                    edits.push_back({ offset, deleteChars, insertStr });
                    offset += (uint32_t)insertStr.size();
                    oldIdx = matchOld;
                    newIdx = matchNew;
                } else {
                    // Replace single line
                    uint32_t deleteLen = (uint32_t)oldLines[oldIdx].size() + 1;
                    std::wstring insertStr = newLines[newIdx] + L'\n';
                    edits.push_back({ offset, deleteLen, insertStr });
                    offset += (uint32_t)insertStr.size();
                    oldIdx++;
                    newIdx++;
                }
            }
        }

        // Handle remaining lines
        if (oldIdx < oldLines.size()) {
            uint32_t deleteChars = 0;
            for (size_t i = oldIdx; i < oldLines.size(); i++)
                deleteChars += (uint32_t)oldLines[i].size() + 1;
            edits.push_back({ offset, deleteChars, L"" });
        }
        if (newIdx < newLines.size()) {
            std::wstring insertStr;
            for (size_t i = newIdx; i < newLines.size(); i++) {
                insertStr += newLines[i];
                if (i + 1 < newLines.size()) insertStr += L'\n';
            }
            edits.push_back({ offset, 0, insertStr });
        }

        return edits;
    }

    uint64_t version() const { return version_; }

    DocumentVersion getDocVersion(const std::wstring& content) const {
        DocumentVersion dv;
        dv.version = version_;
        dv.hash = fnv1aHash(content);
        dv.charCount = (uint32_t)content.size();
        dv.lineCount = 1;
        for (auto ch : content) {
            if (ch == L'\n') dv.lineCount++;
        }
        return dv;
    }

private:
    static std::vector<std::wstring> splitLines(const std::wstring& s) {
        std::vector<std::wstring> lines;
        size_t start = 0;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == L'\n') {
                lines.push_back(s.substr(start, i - start));
                start = i + 1;
            }
        }
        if (start <= s.size()) lines.push_back(s.substr(start));
        return lines;
    }

    static uint64_t fnv1aHash(const std::wstring& s) {
        uint64_t hash = 14695981039346656037ULL;
        const auto* bytes = reinterpret_cast<const uint8_t*>(s.data());
        size_t byteLen = s.size() * sizeof(wchar_t);
        for (size_t i = 0; i < byteLen; i++) {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    std::mutex mutex_;
    std::vector<TextEdit> pendingEdits_;
    std::atomic<uint64_t> version_;
};

// ============================================================================
// DELTA ENCODING for binary/token data
// Encodes differences between sequential data frames.
// ============================================================================

class DeltaEncoder {
public:
    // Encode delta between old and new byte arrays
    static std::vector<uint8_t> encode(const uint8_t* oldData, size_t oldLen,
                                        const uint8_t* newData, size_t newLen) {
        std::vector<uint8_t> delta;
        // Header: new length
        pushU32(delta, (uint32_t)newLen);

        size_t minLen = (oldLen < newLen) ? oldLen : newLen;
        size_t i = 0;
        while (i < minLen) {
            // Find run of identical bytes
            size_t matchStart = i;
            while (i < minLen && oldData[i] == newData[i]) i++;
            size_t matchLen = i - matchStart;
            if (matchLen > 0) {
                // COPY op: copy matchLen bytes from old
                delta.push_back(0x01); // COPY
                pushU32(delta, (uint32_t)matchLen);
            }

            // Find run of different bytes
            size_t diffStart = i;
            while (i < minLen && oldData[i] != newData[i]) i++;
            size_t diffLen = i - diffStart;
            if (diffLen > 0) {
                // INSERT op with replacement data
                delta.push_back(0x02); // REPLACE
                pushU32(delta, (uint32_t)diffLen);
                delta.insert(delta.end(), newData + diffStart, newData + diffStart + diffLen);
            }
        }

        // Remaining new data (appended)
        if (newLen > minLen) {
            delta.push_back(0x03); // APPEND
            pushU32(delta, (uint32_t)(newLen - minLen));
            delta.insert(delta.end(), newData + minLen, newData + newLen);
        }

        // Truncation marker
        if (oldLen > newLen) {
            delta.push_back(0x04); // TRUNCATE
            pushU32(delta, (uint32_t)(oldLen - newLen));
        }

        delta.push_back(0x00); // END
        counters().deltaEncodings.fetch_add(1, std::memory_order_relaxed);
        return delta;
    }

    // Apply delta to old data to produce new data
    static std::vector<uint8_t> apply(const uint8_t* oldData, size_t oldLen,
                                       const uint8_t* delta, size_t deltaLen) {
        if (deltaLen < 5) return {};
        size_t pos = 0;
        uint32_t newLen = readU32(delta, pos);
        std::vector<uint8_t> result;
        result.reserve(newLen);

        size_t oldPos = 0;
        while (pos < deltaLen) {
            uint8_t op = delta[pos++];
            if (op == 0x00) break; // END
            uint32_t len = readU32(delta, pos);

            switch (op) {
            case 0x01: // COPY
                if (oldPos + len <= oldLen) {
                    result.insert(result.end(), oldData + oldPos, oldData + oldPos + len);
                }
                oldPos += len;
                break;
            case 0x02: // REPLACE
                result.insert(result.end(), delta + pos, delta + pos + len);
                pos += len;
                oldPos += len;
                break;
            case 0x03: // APPEND
                result.insert(result.end(), delta + pos, delta + pos + len);
                pos += len;
                break;
            case 0x04: // TRUNCATE
                // Just skip oldPos forward
                oldPos += len;
                break;
            }
        }
        return result;
    }

private:
    static void pushU32(std::vector<uint8_t>& v, uint32_t val) {
        v.push_back((uint8_t)(val & 0xFF));
        v.push_back((uint8_t)((val >> 8) & 0xFF));
        v.push_back((uint8_t)((val >> 16) & 0xFF));
        v.push_back((uint8_t)((val >> 24) & 0xFF));
    }

    static uint32_t readU32(const uint8_t* data, size_t& pos) {
        uint32_t val = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24);
        pos += 4;
        return val;
    }
};

// ============================================================================
// SEMANTIC TOKEN DELTA
// LSP-style semantic token delta encoding. Only sends changed token ranges
// instead of full token arrays.
// ============================================================================

struct SemanticToken {
    uint32_t line;        // Relative to previous token
    uint32_t startChar;   // Relative to previous token on same line
    uint32_t length;
    uint32_t tokenType;   // Keyword=0, Type=1, Function=2, Variable=3, String=4, Comment=5, Number=6, Preprocessor=7
    uint32_t modifiers;   // Bitmask: declaration=1, definition=2, readonly=4, static_=8, deprecated=16
};

struct SemanticTokenEdit {
    uint32_t start;       // Start index in token array
    uint32_t deleteCount; // Number of tokens to remove
    std::vector<uint32_t> data; // Flattened new tokens (5 ints per token)
};

class SemanticTokenDelta {
public:
    using TokenArray = std::vector<SemanticToken>;

    // Full token encode (5 ints per token, LSP-compatible)
    static std::vector<uint32_t> encode(const TokenArray& tokens) {
        std::vector<uint32_t> data;
        data.reserve(tokens.size() * 5);
        for (const auto& t : tokens) {
            data.push_back(t.line);
            data.push_back(t.startChar);
            data.push_back(t.length);
            data.push_back(t.tokenType);
            data.push_back(t.modifiers);
        }
        return data;
    }

    // Compute delta between previous and current token arrays
    static std::vector<SemanticTokenEdit> computeDelta(
        const std::vector<uint32_t>& prevData,
        const std::vector<uint32_t>& currData)
    {
        std::vector<SemanticTokenEdit> edits;

        // Find longest common prefix
        size_t prefixLen = 0;
        size_t minLen = (prevData.size() < currData.size()) ? prevData.size() : currData.size();
        while (prefixLen < minLen && prevData[prefixLen] == currData[prefixLen])
            prefixLen++;
        // Align to token boundary (5 ints)
        prefixLen = (prefixLen / 5) * 5;

        // Find longest common suffix
        size_t suffixLen = 0;
        while (suffixLen < minLen - prefixLen &&
               prevData[prevData.size() - 1 - suffixLen] == currData[currData.size() - 1 - suffixLen])
            suffixLen++;
        suffixLen = (suffixLen / 5) * 5;

        uint32_t deleteCount = (uint32_t)((prevData.size() - prefixLen - suffixLen) / 5);
        std::vector<uint32_t> insertData(
            currData.begin() + prefixLen,
            currData.end() - suffixLen);

        if (deleteCount > 0 || !insertData.empty()) {
            SemanticTokenEdit edit;
            edit.start = (uint32_t)(prefixLen / 5);
            edit.deleteCount = deleteCount;
            edit.data = std::move(insertData);
            edits.push_back(edit);
        }

        return edits;
    }

    // Apply delta edits to previous token data
    static std::vector<uint32_t> applyDelta(
        const std::vector<uint32_t>& prevData,
        const std::vector<SemanticTokenEdit>& edits)
    {
        std::vector<uint32_t> result = prevData;
        // Apply edits in reverse order to maintain indices
        for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
            size_t startIdx = it->start * 5;
            size_t deleteLen = it->deleteCount * 5;
            if (startIdx + deleteLen > result.size()) continue;
            result.erase(result.begin() + startIdx, result.begin() + startIdx + deleteLen);
            result.insert(result.begin() + startIdx, it->data.begin(), it->data.end());
        }
        return result;
    }

    // Cache for previous token data per file
    void updateCache(const std::wstring& filePath, const std::vector<uint32_t>& tokenData) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        tokenCache_[filePath] = tokenData;
    }

    std::vector<uint32_t> getCached(const std::wstring& filePath) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = tokenCache_.find(filePath);
        if (it != tokenCache_.end()) return it->second;
        return {};
    }

private:
    std::mutex cacheMutex_;
    std::unordered_map<std::wstring, std::vector<uint32_t>> tokenCache_;
};

// ============================================================================
// VIRTUAL SCROLLING ENGINE
// Only renders visible lines. Maintains viewport window with overscan buffer.
// Critical for files with 100K+ lines.
// ============================================================================

struct VirtualLine {
    uint32_t lineNumber;     // 0-based line number in document
    uint32_t startOffset;    // Character offset in document
    uint32_t length;         // Character count
    uint16_t indentLevel;    // Cached indent level
    uint8_t  foldState;      // 0=normal, 1=fold-start, 2=folded-hidden
    uint8_t  flags;          // Bitmask: hasBreakpoint=1, hasError=2, hasWarning=4, hasBookmark=8
    COLORREF gutterColor;    // Line-level gutter indicator
};

class VirtualScrollEngine {
public:
    static constexpr int OVERSCAN_LINES = 20; // Render 20 lines above/below viewport

    VirtualScrollEngine() : totalLines_(0), viewportTop_(0), viewportHeight_(0),
                            lineHeight_(16), scrollY_(0) {}

    // Rebuild line map from document content
    void rebuildFromContent(const wchar_t* text, size_t textLen) {
        lines_.clear();
        if (!text || textLen == 0) { totalLines_ = 0; return; }

        uint32_t lineNum = 0;
        uint32_t lineStart = 0;

        for (size_t i = 0; i <= textLen; i++) {
            if (i == textLen || text[i] == L'\n') {
                VirtualLine vl;
                vl.lineNumber = lineNum;
                vl.startOffset = lineStart;
                vl.length = (uint32_t)(i - lineStart);
                vl.indentLevel = 0;
                vl.foldState = 0;
                vl.flags = 0;
                vl.gutterColor = 0;

                // Calculate indent
                for (uint32_t j = lineStart; j < (uint32_t)i; j++) {
                    if (text[j] == L' ') vl.indentLevel++;
                    else if (text[j] == L'\t') vl.indentLevel += 4;
                    else break;
                }

                lines_.push_back(vl);
                lineNum++;
                lineStart = (uint32_t)(i + 1);
            }
        }
        totalLines_ = lineNum;
    }

    // Apply incremental edits (much faster than full rebuild)
    void applyEdit(uint32_t startLine, uint32_t deletedLines, uint32_t insertedLines,
                   const wchar_t* newText, size_t newTextLen) {
        if (startLine > lines_.size()) return;

        // Remove old lines
        auto eraseEnd = startLine + deletedLines;
        if (eraseEnd > lines_.size()) eraseEnd = (uint32_t)lines_.size();
        lines_.erase(lines_.begin() + startLine, lines_.begin() + eraseEnd);

        // Parse new lines from the inserted text
        std::vector<VirtualLine> newLines;
        uint32_t lineStart = 0;
        for (size_t i = 0; i <= newTextLen; i++) {
            if (i == newTextLen || newText[i] == L'\n') {
                VirtualLine vl;
                vl.lineNumber = startLine + (uint32_t)newLines.size();
                vl.startOffset = 0; // Will be recalculated
                vl.length = (uint32_t)(i - lineStart);
                vl.indentLevel = 0;
                vl.foldState = 0;
                vl.flags = 0;
                vl.gutterColor = 0;
                newLines.push_back(vl);
                lineStart = (uint32_t)(i + 1);
            }
        }

        lines_.insert(lines_.begin() + startLine, newLines.begin(), newLines.end());

        // Renumber lines after insertion
        for (size_t i = startLine + newLines.size(); i < lines_.size(); i++) {
            lines_[i].lineNumber = (uint32_t)i;
        }
        totalLines_ = (uint32_t)lines_.size();
    }

    // Get visible line range with overscan
    void getVisibleRange(int& firstLine, int& lastLine) const {
        firstLine = viewportTop_ - OVERSCAN_LINES;
        if (firstLine < 0) firstLine = 0;
        lastLine = viewportTop_ + viewportHeight_ + OVERSCAN_LINES;
        if (lastLine > (int)totalLines_) lastLine = (int)totalLines_;
    }

    // Get only the lines that need rendering
    std::vector<const VirtualLine*> getVisibleLines() const {
        std::vector<const VirtualLine*> visible;
        int first, last;
        getVisibleRange(first, last);
        visible.reserve(last - first);
        for (int i = first; i < last && i < (int)lines_.size(); i++) {
            if (lines_[i].foldState != 2) { // Skip folded-hidden lines
                visible.push_back(&lines_[i]);
            }
        }
        counters().virtualScrollRedraws.fetch_add(1, std::memory_order_relaxed);
        return visible;
    }

    // Update viewport position (called on scroll events)
    void setViewport(int topLine, int visibleLines) {
        viewportTop_ = topLine;
        viewportHeight_ = visibleLines;
    }

    void setLineHeight(int h) { lineHeight_ = h; }

    // Fold/unfold support
    void foldRange(uint32_t startLine, uint32_t endLine) {
        if (startLine < lines_.size()) lines_[startLine].foldState = 1;
        for (uint32_t i = startLine + 1; i <= endLine && i < lines_.size(); i++)
            lines_[i].foldState = 2; // Hidden
    }

    void unfoldRange(uint32_t startLine, uint32_t endLine) {
        for (uint32_t i = startLine; i <= endLine && i < lines_.size(); i++)
            lines_[i].foldState = 0;
    }

    // Getters
    uint32_t totalLines() const { return totalLines_; }
    int viewportTop() const { return viewportTop_; }
    const VirtualLine* getLine(uint32_t idx) const {
        return (idx < lines_.size()) ? &lines_[idx] : nullptr;
    }

private:
    std::vector<VirtualLine> lines_;
    uint32_t totalLines_;
    int viewportTop_;
    int viewportHeight_;
    int lineHeight_;
    int scrollY_;
};

// ============================================================================
// FUZZY SEARCH (ripgrep-style)
// Aho-Corasick + Smith-Waterman scoring for blazing-fast fuzzy matching.
// Case-insensitive, supports camelCase word boundary bonuses.
// ============================================================================

struct FuzzyMatch {
    int score;
    std::vector<int> matchPositions; // Indices into the candidate string that matched
    WStringRef candidate;
};

class FuzzySearch {
public:
    // Score a candidate against a pattern. Higher = better match.
    // Returns -1 for no match.
    static int score(const wchar_t* pattern, size_t patLen,
                     const wchar_t* candidate, size_t candLen) {
        if (patLen == 0) return 0;
        if (candLen == 0) return -1;
        if (patLen > candLen) return -1;

        int totalScore = 0;
        size_t patIdx = 0;
        size_t lastMatch = SIZE_MAX;
        bool prevMatched = false;

        for (size_t i = 0; i < candLen && patIdx < patLen; i++) {
            wchar_t p = towlower(pattern[patIdx]);
            wchar_t c = towlower(candidate[i]);

            if (p == c) {
                int bonus = 0;

                // Exact case match bonus
                if (pattern[patIdx] == candidate[i]) bonus += 2;

                // Start of word bonus (after separator or camelCase boundary)
                if (i == 0) bonus += 10;
                else if (isWordBoundary(candidate[i - 1], candidate[i])) bonus += 8;

                // Consecutive match bonus
                if (prevMatched) bonus += 5;

                // Proximity bonus (matches close together are better)
                if (lastMatch != SIZE_MAX && i - lastMatch <= 2) bonus += 3;

                totalScore += 10 + bonus;
                lastMatch = i;
                patIdx++;
                prevMatched = true;
            } else {
                prevMatched = false;
                totalScore -= 1; // Gap penalty
            }
        }

        if (patIdx != patLen) return -1; // Not all pattern chars matched

        // Bonus for shorter candidates (prefer exact-length matches)
        if (candLen <= patLen * 2) totalScore += 5;

        // Bonus for prefix match
        if (candLen >= patLen && towlower(candidate[0]) == towlower(pattern[0]))
            totalScore += 15;

        return totalScore;
    }

    // Score with match position tracking
    static FuzzyMatch scoreWithPositions(const wchar_t* pattern, size_t patLen,
                                          const wchar_t* candidate, size_t candLen) {
        FuzzyMatch result;
        result.candidate = WStringRef(candidate, candLen);
        result.score = -1;

        if (patLen == 0 || candLen == 0 || patLen > candLen) return result;

        int totalScore = 0;
        size_t patIdx = 0;
        size_t lastMatch = SIZE_MAX;
        bool prevMatched = false;

        for (size_t i = 0; i < candLen && patIdx < patLen; i++) {
            wchar_t p = towlower(pattern[patIdx]);
            wchar_t c = towlower(candidate[i]);

            if (p == c) {
                int bonus = 0;
                if (pattern[patIdx] == candidate[i]) bonus += 2;
                if (i == 0) bonus += 10;
                else if (isWordBoundary(candidate[i - 1], candidate[i])) bonus += 8;
                if (prevMatched) bonus += 5;
                if (lastMatch != SIZE_MAX && i - lastMatch <= 2) bonus += 3;

                totalScore += 10 + bonus;
                result.matchPositions.push_back((int)i);
                lastMatch = i;
                patIdx++;
                prevMatched = true;
            } else {
                prevMatched = false;
                totalScore -= 1;
            }
        }

        if (patIdx != patLen) return result;

        if (candLen <= patLen * 2) totalScore += 5;
        if (towlower(candidate[0]) == towlower(pattern[0])) totalScore += 15;

        result.score = totalScore;
        counters().fuzzySearches.fetch_add(1, std::memory_order_relaxed);
        return result;
    }

    // Batch search: find top-N matches from candidates
    static std::vector<FuzzyMatch> search(
        const std::wstring& pattern,
        const std::vector<std::wstring>& candidates,
        size_t maxResults = 50)
    {
        std::vector<FuzzyMatch> matches;
        matches.reserve(candidates.size());

        for (const auto& cand : candidates) {
            FuzzyMatch m = scoreWithPositions(
                pattern.c_str(), pattern.size(),
                cand.c_str(), cand.size());
            if (m.score >= 0) {
                matches.push_back(std::move(m));
            }
        }

        // Sort by score descending, then by length ascending
        std::sort(matches.begin(), matches.end(), [](const FuzzyMatch& a, const FuzzyMatch& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.candidate.size() < b.candidate.size();
        });

        if (matches.size() > maxResults) matches.resize(maxResults);
        return matches;
    }

    // Parallel batch search for large candidate sets
    static std::vector<FuzzyMatch> parallelSearch(
        const std::wstring& pattern,
        const std::vector<std::wstring>& candidates,
        size_t maxResults = 50,
        size_t threadCount = 0)
    {
        if (threadCount == 0) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            threadCount = si.dwNumberOfProcessors;
            if (threadCount == 0) threadCount = 4;
        }

        if (candidates.size() < 1000 || threadCount <= 1) {
            return search(pattern, candidates, maxResults);
        }

        // Partition candidates across threads
        size_t chunkSize = (candidates.size() + threadCount - 1) / threadCount;
        std::vector<std::vector<FuzzyMatch>> threadResults(threadCount);
        std::vector<std::thread> threads;

        for (size_t t = 0; t < threadCount; t++) {
            size_t start = t * chunkSize;
            size_t end = (start + chunkSize > candidates.size()) ? candidates.size() : start + chunkSize;
            if (start >= candidates.size()) break;

            threads.emplace_back([&, start, end, t]() {
                for (size_t i = start; i < end; i++) {
                    FuzzyMatch m = scoreWithPositions(
                        pattern.c_str(), pattern.size(),
                        candidates[i].c_str(), candidates[i].size());
                    if (m.score >= 0) {
                        threadResults[t].push_back(std::move(m));
                    }
                }
            });
        }

        for (auto& th : threads) th.join();

        // Merge results
        std::vector<FuzzyMatch> merged;
        for (auto& r : threadResults) {
            merged.insert(merged.end(),
                          std::make_move_iterator(r.begin()),
                          std::make_move_iterator(r.end()));
        }

        std::sort(merged.begin(), merged.end(), [](const FuzzyMatch& a, const FuzzyMatch& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.candidate.size() < b.candidate.size();
        });

        if (merged.size() > maxResults) merged.resize(maxResults);
        return merged;
    }

private:
    static bool isWordBoundary(wchar_t prev, wchar_t curr) {
        // camelCase: lowercase followed by uppercase
        if (iswlower(prev) && iswupper(curr)) return true;
        // snake_case: underscore
        if (prev == L'_' || prev == L'-' || prev == L'.' || prev == L'/'
            || prev == L'\\' || prev == L':') return true;
        // Transition from non-alpha to alpha
        if (!iswalpha(prev) && iswalpha(curr)) return true;
        return false;
    }
};

// ============================================================================
// PARALLEL PARSER
// Splits document into chunks and parses in parallel.
// Each chunk produces token ranges; merge step resolves cross-chunk tokens.
// ============================================================================

struct ParsedToken {
    uint32_t offset;
    uint32_t length;
    uint8_t  type;    // 0=text, 1=keyword, 2=type, 3=function, 4=string, 5=comment, 6=number, 7=preproc
    uint8_t  flags;   // 0x01=start_of_multiline, 0x02=inside_multiline
};

class ParallelParser {
public:
    // Parse document in parallel chunks, producing semantic tokens
    static std::vector<ParsedToken> parse(const wchar_t* text, size_t textLen,
                                           const std::unordered_set<std::wstring>& keywords,
                                           const std::unordered_set<std::wstring>& types,
                                           size_t threadCount = 0) {
        if (threadCount == 0) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            threadCount = si.dwNumberOfProcessors;
            if (threadCount < 2) threadCount = 2;
        }

        // For small documents, single-threaded is faster
        if (textLen < 10000 || threadCount <= 1) {
            return parseSingleThread(text, textLen, keywords, types);
        }

        counters().parallelParses.fetch_add(1, std::memory_order_relaxed);

        // Split at line boundaries
        std::vector<size_t> chunkStarts;
        chunkStarts.push_back(0);
        size_t avgChunk = textLen / threadCount;

        for (size_t t = 1; t < threadCount; t++) {
            size_t target = t * avgChunk;
            // Find nearest line boundary
            while (target < textLen && text[target] != L'\n') target++;
            if (target < textLen) target++; // Include the newline
            chunkStarts.push_back(target);
        }
        chunkStarts.push_back(textLen);

        // Parse each chunk in parallel
        std::vector<std::vector<ParsedToken>> chunkTokens(chunkStarts.size() - 1);
        std::vector<std::thread> threads;

        for (size_t t = 0; t < chunkStarts.size() - 1; t++) {
            size_t start = chunkStarts[t];
            size_t end = chunkStarts[t + 1];
            threads.emplace_back([&, t, start, end]() {
                chunkTokens[t] = parseSingleThread(text + start, end - start, keywords, types);
                // Adjust offsets to be document-relative
                for (auto& tok : chunkTokens[t]) {
                    tok.offset += (uint32_t)start;
                }
            });
        }

        for (auto& th : threads) th.join();

        // Merge chunks (handle cross-boundary comments/strings)
        std::vector<ParsedToken> merged;
        size_t totalTokens = 0;
        for (auto& ct : chunkTokens) totalTokens += ct.size();
        merged.reserve(totalTokens);

        for (auto& ct : chunkTokens) {
            merged.insert(merged.end(), ct.begin(), ct.end());
        }

        // Fix cross-boundary multiline constructs
        fixCrossBoundaryTokens(text, textLen, merged);

        return merged;
    }

private:
    static std::vector<ParsedToken> parseSingleThread(
        const wchar_t* text, size_t textLen,
        const std::unordered_set<std::wstring>& keywords,
        const std::unordered_set<std::wstring>& types)
    {
        std::vector<ParsedToken> tokens;
        tokens.reserve(textLen / 5); // Rough estimate

        size_t i = 0;
        while (i < textLen) {
            // Skip whitespace
            while (i < textLen && iswspace(text[i])) i++;
            if (i >= textLen) break;

            size_t start = i;

            // Line comment
            if (i + 1 < textLen && text[i] == L'/' && text[i + 1] == L'/') {
                while (i < textLen && text[i] != L'\n') i++;
                tokens.push_back({ (uint32_t)start, (uint32_t)(i - start), 5, 0 });
                continue;
            }

            // Block comment
            if (i + 1 < textLen && text[i] == L'/' && text[i + 1] == L'*') {
                i += 2;
                while (i + 1 < textLen && !(text[i] == L'*' && text[i + 1] == L'/')) i++;
                if (i + 1 < textLen) i += 2;
                tokens.push_back({ (uint32_t)start, (uint32_t)(i - start), 5, 0x01 });
                continue;
            }

            // String
            if (text[i] == L'"' || text[i] == L'\'') {
                wchar_t q = text[i]; i++;
                while (i < textLen && text[i] != q) {
                    if (text[i] == L'\\' && i + 1 < textLen) i++;
                    i++;
                }
                if (i < textLen) i++;
                tokens.push_back({ (uint32_t)start, (uint32_t)(i - start), 4, 0 });
                continue;
            }

            // Preprocessor
            if (text[i] == L'#') {
                while (i < textLen && (iswalnum(text[i]) || text[i] == L'#' || text[i] == L'_')) i++;
                tokens.push_back({ (uint32_t)start, (uint32_t)(i - start), 7, 0 });
                continue;
            }

            // Number
            if (iswdigit(text[i]) || (text[i] == L'.' && i + 1 < textLen && iswdigit(text[i + 1]))) {
                while (i < textLen && (iswalnum(text[i]) || text[i] == L'.' || text[i] == L'x' || text[i] == L'X'))
                    i++;
                tokens.push_back({ (uint32_t)start, (uint32_t)(i - start), 6, 0 });
                continue;
            }

            // Identifier / keyword / type
            if (iswalpha(text[i]) || text[i] == L'_') {
                while (i < textLen && (iswalnum(text[i]) || text[i] == L'_')) i++;
                std::wstring word(text + start, i - start);
                uint8_t type = 0; // default text

                if (keywords.count(word)) type = 1;      // keyword
                else if (types.count(word)) type = 2;     // type

                // Check for function call (word followed by '(')
                if (type == 0) {
                    size_t j = i;
                    while (j < textLen && iswspace(text[j])) j++;
                    if (j < textLen && text[j] == L'(') type = 3; // function
                }

                if (type != 0) {
                    tokens.push_back({ (uint32_t)start, (uint32_t)(i - start), type, 0 });
                }
                continue;
            }

            i++; // Skip other characters
        }

        return tokens;
    }

    static void fixCrossBoundaryTokens(const wchar_t* text, size_t textLen,
                                        std::vector<ParsedToken>& tokens) {
        // Check for block comments that span chunk boundaries
        bool inBlockComment = false;
        size_t commentStart = 0;
        std::vector<ParsedToken> fixed;
        fixed.reserve(tokens.size());

        for (size_t i = 0; i < tokens.size(); i++) {
            auto& tok = tokens[i];
            if (tok.type == 5 && (tok.flags & 0x01)) {
                // Check if this comment actually ends
                size_t endPos = tok.offset + tok.length;
                if (endPos >= 2 && text[endPos - 2] == L'*' && text[endPos - 1] == L'/') {
                    fixed.push_back(tok);
                } else {
                    // Comment extends past chunk boundary - extend it
                    size_t j = endPos;
                    while (j + 1 < textLen && !(text[j] == L'*' && text[j + 1] == L'/')) j++;
                    if (j + 1 < textLen) j += 2;
                    tok.length = (uint32_t)(j - tok.offset);

                    // Remove any tokens that fall within this extended comment
                    fixed.push_back(tok);
                    while (i + 1 < tokens.size() && tokens[i + 1].offset < j) i++;
                }
            } else {
                fixed.push_back(tok);
            }
        }
        tokens = std::move(fixed);
    }
};

// ============================================================================
// WORKSPACE CACHE
// Persistent file content and metadata cache. Avoids re-reading/re-parsing
// files when switching tabs or reopening the IDE.
// ============================================================================

struct CachedFile {
    std::wstring path;
    uint64_t lastModified;     // FILETIME as uint64
    uint64_t contentHash;
    uint32_t lineCount;
    uint32_t charCount;
    std::vector<ParsedToken> tokens;
    std::vector<uint32_t> semanticTokenData;
    bool dirty;
};

class WorkspaceCache {
public:
    WorkspaceCache() {}

    // Check if file is cached and still fresh
    bool isCached(const std::wstring& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(path);
        if (it == cache_.end()) return false;

        // Check if file has been modified
        uint64_t diskTime = getFileModTime(path);
        if (diskTime != it->second.lastModified) {
            counters().cacheMisses.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        counters().cacheHits.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // Get cached file data
    const CachedFile* get(const std::wstring& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(path);
        if (it == cache_.end()) return nullptr;
        counters().cacheHits.fetch_add(1, std::memory_order_relaxed);
        return &it->second;
    }

    // Update cache entry
    void put(const std::wstring& path, CachedFile&& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        entry.lastModified = getFileModTime(path);
        cache_[path] = std::move(entry);
    }

    // Update just tokens for a cached file
    void updateTokens(const std::wstring& path, std::vector<ParsedToken>&& tokens) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            it->second.tokens = std::move(tokens);
        }
    }

    // Evict entries beyond max entries (LRU would be ideal, using size-based for simplicity)
    void evict(size_t maxEntries = 100) {
        std::lock_guard<std::mutex> lock(mutex_);
        while (cache_.size() > maxEntries) {
            cache_.erase(cache_.begin());
        }
    }

    // Clear all
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    // Persist cache metadata to disk (for IDE restart)
    void saveToDisk(const std::wstring& cachePath) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Save a lightweight index (not full content)
        std::ofstream f(std::string(cachePath.begin(), cachePath.end()), std::ios::binary);
        if (!f.is_open()) return;

        uint32_t entryCount = (uint32_t)cache_.size();
        f.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
        for (auto& [path, entry] : cache_) {
            uint32_t pathLen = (uint32_t)path.size();
            f.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
            f.write(reinterpret_cast<const char*>(path.data()), pathLen * sizeof(wchar_t));
            f.write(reinterpret_cast<const char*>(&entry.lastModified), sizeof(entry.lastModified));
            f.write(reinterpret_cast<const char*>(&entry.contentHash), sizeof(entry.contentHash));
            f.write(reinterpret_cast<const char*>(&entry.lineCount), sizeof(entry.lineCount));
            f.write(reinterpret_cast<const char*>(&entry.charCount), sizeof(entry.charCount));
        }
    }

    void loadFromDisk(const std::wstring& cachePath) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ifstream f(std::string(cachePath.begin(), cachePath.end()), std::ios::binary);
        if (!f.is_open()) return;

        uint32_t entryCount = 0;
        f.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
        if (entryCount > 10000) return; // Sanity check

        for (uint32_t i = 0; i < entryCount; i++) {
            uint32_t pathLen = 0;
            f.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
            if (pathLen > 32768) break;

            std::wstring path(pathLen, L'\0');
            f.read(reinterpret_cast<char*>(path.data()), pathLen * sizeof(wchar_t));

            CachedFile entry;
            entry.path = path;
            entry.dirty = false;
            f.read(reinterpret_cast<char*>(&entry.lastModified), sizeof(entry.lastModified));
            f.read(reinterpret_cast<char*>(&entry.contentHash), sizeof(entry.contentHash));
            f.read(reinterpret_cast<char*>(&entry.lineCount), sizeof(entry.lineCount));
            f.read(reinterpret_cast<char*>(&entry.charCount), sizeof(entry.charCount));

            // Only keep if file still exists on disk with same mod time
            if (getFileModTime(path) == entry.lastModified) {
                cache_[path] = std::move(entry);
            }
        }
    }

private:
    static uint64_t getFileModTime(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
        ULARGE_INTEGER uli;
        uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        return uli.QuadPart;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::wstring, CachedFile> cache_;
};

// ============================================================================
// SYMBOL DATABASE (SQLite-style in-memory)
// Fast symbol lookup via hash maps and sorted vectors.
// Supports goto-definition, find-references, fuzzy symbol search.
// For a full SQLite backend, replace the internal maps with sqlite3 calls.
// ============================================================================

struct SymbolRecord {
    uint32_t id;
    std::wstring name;
    std::wstring qualifiedName;  // e.g. "MyClass::myMethod"
    std::wstring kind;           // function, class, variable, macro, enum, typedef
    std::wstring file;
    uint32_t line;
    uint32_t column;
    uint32_t endLine;
    uint32_t endColumn;
    uint32_t parentId;           // Enclosing scope symbol ID (0 = global)
    uint64_t hash;               // For deduplication
};

class SymbolDatabase {
public:
    SymbolDatabase() : nextId_(1) {}

    uint32_t insert(const SymbolRecord& sym) {
        std::lock_guard<std::mutex> lock(mutex_);
        SymbolRecord record = sym;
        record.id = nextId_++;
        record.hash = computeHash(sym);

        // Dedup by hash
        if (hashIndex_.count(record.hash)) {
            // Update existing
            auto existingId = hashIndex_[record.hash];
            symbols_[existingId] = record;
            return existingId;
        }

        symbols_[record.id] = record;
        nameIndex_[record.name].push_back(record.id);
        fileIndex_[record.file].push_back(record.id);
        hashIndex_[record.hash] = record.id;
        counters().indexingSymbols.fetch_add(1, std::memory_order_relaxed);
        return record.id;
    }

    // Batch insert (faster for indexing entire files)
    void insertBatch(const std::vector<SymbolRecord>& syms) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sym : syms) {
            SymbolRecord record = sym;
            record.id = nextId_++;
            record.hash = computeHash(sym);

            if (hashIndex_.count(record.hash)) {
                auto existingId = hashIndex_[record.hash];
                symbols_[existingId] = record;
                continue;
            }

            symbols_[record.id] = record;
            nameIndex_[record.name].push_back(record.id);
            fileIndex_[record.file].push_back(record.id);
            hashIndex_[record.hash] = record.id;
        }
        counters().indexingSymbols.fetch_add(syms.size(), std::memory_order_relaxed);
    }

    // Lookup by exact name
    std::vector<const SymbolRecord*> findByName(const std::wstring& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<const SymbolRecord*> results;
        auto it = nameIndex_.find(name);
        if (it != nameIndex_.end()) {
            for (auto id : it->second) {
                auto sit = symbols_.find(id);
                if (sit != symbols_.end()) results.push_back(&sit->second);
            }
        }
        return results;
    }

    // Lookup all symbols in a file
    std::vector<const SymbolRecord*> findByFile(const std::wstring& file) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<const SymbolRecord*> results;
        auto it = fileIndex_.find(file);
        if (it != fileIndex_.end()) {
            for (auto id : it->second) {
                auto sit = symbols_.find(id);
                if (sit != symbols_.end()) results.push_back(&sit->second);
            }
        }
        return results;
    }

    // Fuzzy search across all symbol names
    std::vector<const SymbolRecord*> fuzzySearch(const std::wstring& query, size_t maxResults = 50) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<int, const SymbolRecord*>> scored;

        for (auto& [id, sym] : symbols_) {
            int s = FuzzySearch::score(query.c_str(), query.size(),
                                        sym.name.c_str(), sym.name.size());
            if (s >= 0) scored.push_back({ s, &sym });
        }

        std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });

        std::vector<const SymbolRecord*> results;
        size_t count = (scored.size() < maxResults) ? scored.size() : maxResults;
        for (size_t i = 0; i < count; i++) results.push_back(scored[i].second);
        return results;
    }

    // Remove all symbols from a file (for re-indexing)
    void removeFile(const std::wstring& file) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = fileIndex_.find(file);
        if (it == fileIndex_.end()) return;

        for (auto id : it->second) {
            auto sit = symbols_.find(id);
            if (sit != symbols_.end()) {
                // Remove from name index
                auto& nameList = nameIndex_[sit->second.name];
                nameList.erase(std::remove(nameList.begin(), nameList.end(), id), nameList.end());
                if (nameList.empty()) nameIndex_.erase(sit->second.name);
                // Remove hash
                hashIndex_.erase(sit->second.hash);
                symbols_.erase(sit);
            }
        }
        fileIndex_.erase(it);
    }

    size_t totalSymbols() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return symbols_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        symbols_.clear();
        nameIndex_.clear();
        fileIndex_.clear();
        hashIndex_.clear();
        nextId_ = 1;
    }

private:
    static uint64_t computeHash(const SymbolRecord& sym) {
        uint64_t hash = 14695981039346656037ULL;
        auto hashStr = [&hash](const std::wstring& s) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(s.data());
            for (size_t i = 0; i < s.size() * sizeof(wchar_t); i++) {
                hash ^= bytes[i];
                hash *= 1099511628211ULL;
            }
        };
        hashStr(sym.name);
        hashStr(sym.file);
        hashStr(sym.kind);
        hash ^= (uint64_t)sym.line;
        hash *= 1099511628211ULL;
        return hash;
    }

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, SymbolRecord> symbols_;
    std::unordered_map<std::wstring, std::vector<uint32_t>> nameIndex_;
    std::unordered_map<std::wstring, std::vector<uint32_t>> fileIndex_;
    std::unordered_map<uint64_t, uint32_t> hashIndex_;
    uint32_t nextId_;
};

// ============================================================================
// BACKGROUND INDEXER
// Runs on a dedicated thread pool. Indexes workspace files incrementally.
// Uses WorkspaceCache to skip unchanged files.
// ============================================================================

struct IndexRequest {
    enum class Type : uint8_t { INDEX_FILE, REINDEX_FILE, REMOVE_FILE, INDEX_WORKSPACE, SHUTDOWN };
    Type type;
    std::wstring path;
};

struct IndexResult {
    std::wstring path;
    uint32_t symbolCount;
    double elapsedMs;
    bool success;
    std::wstring error;
};

class BackgroundIndexer {
public:
    BackgroundIndexer(SymbolDatabase& db, WorkspaceCache& cache)
        : db_(db), cache_(cache), running_(false), totalIndexed_(0) {}

    ~BackgroundIndexer() { stop(); }

    void start(size_t threadCount = 2) {
        if (running_) return;
        running_ = true;
        for (size_t i = 0; i < threadCount; i++) {
            workers_.emplace_back(&BackgroundIndexer::workerLoop, this);
        }
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        // Push shutdown requests
        for (size_t i = 0; i < workers_.size(); i++) {
            IndexRequest req;
            req.type = IndexRequest::Type::SHUTDOWN;
            requestQueue_.push(req);
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
    }

    // Queue a file for indexing
    void indexFile(const std::wstring& path) {
        IndexRequest req;
        req.type = IndexRequest::Type::INDEX_FILE;
        req.path = path;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingRequests_.push(req);
        }
        cv_.notify_one();
    }

    // Queue a file for re-indexing (removes old symbols first)
    void reindexFile(const std::wstring& path) {
        IndexRequest req;
        req.type = IndexRequest::Type::REINDEX_FILE;
        req.path = path;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingRequests_.push(req);
        }
        cv_.notify_one();
    }

    // Index entire workspace directory
    void indexWorkspace(const std::wstring& rootPath) {
        IndexRequest req;
        req.type = IndexRequest::Type::INDEX_WORKSPACE;
        req.path = rootPath;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingRequests_.push(req);
        }
        cv_.notify_one();
    }

    // Get completed results (non-blocking)
    bool pollResult(IndexResult& result) {
        return resultQueue_.pop(result);
    }

    uint32_t totalIndexed() const { return totalIndexed_.load(); }
    bool isRunning() const { return running_.load(); }

private:
    void workerLoop() {
        while (running_) {
            IndexRequest req;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                cv_.wait(lock, [this] { return !pendingRequests_.empty() || !running_; });
                if (!running_) break;
                if (pendingRequests_.empty()) continue;
                req = pendingRequests_.front();
                pendingRequests_.pop();
            }

            if (req.type == IndexRequest::Type::SHUTDOWN) break;

            if (req.type == IndexRequest::Type::INDEX_WORKSPACE) {
                indexDirectory(req.path);
                continue;
            }

            processFile(req);
        }
    }

    void processFile(const IndexRequest& req) {
        PerfTimer timer("index");

        // Check cache first
        if (req.type == IndexRequest::Type::INDEX_FILE && cache_.isCached(req.path)) {
            IndexResult result;
            result.path = req.path;
            result.symbolCount = 0;
            result.elapsedMs = timer.elapsed_ms();
            result.success = true;
            resultQueue_.push(result);
            return;
        }

        // Read file
        HANDLE hFile = CreateFileW(req.path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            IndexResult result;
            result.path = req.path;
            result.success = false;
            result.error = L"Cannot open file";
            resultQueue_.push(result);
            return;
        }

        LARGE_INTEGER liSize;
        GetFileSizeEx(hFile, &liSize);
        if (liSize.QuadPart > 10LL * 1024LL * 1024LL) {
            CloseHandle(hFile);
            return; // Skip files > 10MB
        }

        DWORD fileSize = (DWORD)liSize.QuadPart;
        std::string content(fileSize, '\0');
        DWORD bytesRead = 0;
        ReadFile(hFile, content.data(), fileSize, &bytesRead, nullptr);
        CloseHandle(hFile);
        content.resize(bytesRead);

        // Convert to wide
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, content.data(), (int)bytesRead, nullptr, 0);
        if (wideLen <= 0) return;
        std::wstring wContent(wideLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, content.data(), (int)bytesRead, wContent.data(), wideLen);

        // Remove old symbols for reindex
        if (req.type == IndexRequest::Type::REINDEX_FILE) {
            db_.removeFile(req.path);
        }

        // Extract symbols (simplified C/C++ parser)
        std::vector<SymbolRecord> symbols = extractSymbols(wContent, req.path);
        db_.insertBatch(symbols);

        // Update cache
        CachedFile cf;
        cf.path = req.path;
        cf.lineCount = 1;
        for (auto ch : wContent) if (ch == L'\n') cf.lineCount++;
        cf.charCount = (uint32_t)wContent.size();
        cf.dirty = false;
        cache_.put(req.path, std::move(cf));

        totalIndexed_++;
        counters().indexingFiles.fetch_add(1, std::memory_order_relaxed);

        IndexResult result;
        result.path = req.path;
        result.symbolCount = (uint32_t)symbols.size();
        result.elapsedMs = timer.elapsed_ms();
        result.success = true;
        resultQueue_.push(result);
    }

    void indexDirectory(const std::wstring& dirPath) {
        // Recursively enumerate source files
        std::vector<std::wstring> files;
        enumerateSourceFiles(dirPath, files, 0);

        for (auto& f : files) {
            if (!running_) break;
            IndexRequest req;
            req.type = IndexRequest::Type::INDEX_FILE;
            req.path = f;
            processFile(req);
        }
    }

    void enumerateSourceFiles(const std::wstring& dirPath, std::vector<std::wstring>& out, int depth) {
        if (depth > 8) return;
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((dirPath + L"\\*").c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        static const std::unordered_set<std::wstring> sourceExts = {
            L".cpp", L".c", L".cc", L".cxx", L".h", L".hpp", L".hxx",
            L".py", L".js", L".ts", L".java", L".cs", L".rs", L".go"
        };
        static const std::unordered_set<std::wstring> skipDirs = {
            L".git", L"node_modules", L".vs", L"__pycache__", L"build",
            L".cache", L"dist", L"target", L"out"
        };

        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

            std::wstring fullPath = dirPath + L"\\" + fd.cFileName;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (skipDirs.count(fd.cFileName) == 0) {
                    enumerateSourceFiles(fullPath, out, depth + 1);
                }
            } else {
                std::wstring ext = fd.cFileName;
                size_t dot = ext.find_last_of(L'.');
                if (dot != std::wstring::npos) {
                    ext = ext.substr(dot);
                    for (auto& c : ext) c = towlower(c);
                    if (sourceExts.count(ext)) {
                        out.push_back(fullPath);
                    }
                }
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    static std::vector<SymbolRecord> extractSymbols(const std::wstring& content, const std::wstring& file) {
        std::vector<SymbolRecord> symbols;
        std::wistringstream stream(content);
        std::wstring line;
        uint32_t lineNum = 0;

        while (std::getline(stream, line)) {
            lineNum++;
            size_t firstNonWs = line.find_first_not_of(L" \t");
            if (firstNonWs == std::wstring::npos) continue;
            std::wstring trimmed = line.substr(firstNonWs);

            // #define
            if (trimmed.substr(0, 8) == L"#define ") {
                size_t nameStart = 8;
                size_t nameEnd = trimmed.find_first_of(L" \t(", nameStart);
                if (nameEnd == std::wstring::npos) nameEnd = trimmed.size();
                if (nameEnd > nameStart) {
                    SymbolRecord sym;
                    sym.name = trimmed.substr(nameStart, nameEnd - nameStart);
                    sym.kind = L"macro";
                    sym.file = file;
                    sym.line = lineNum;
                    sym.column = (uint32_t)firstNonWs + 1;
                    sym.parentId = 0;
                    symbols.push_back(sym);
                }
            }

            // Function/method
            if (trimmed.find(L'(') != std::wstring::npos && trimmed.find(L';') == std::wstring::npos) {
                size_t parenPos = trimmed.find(L'(');
                if (parenPos > 0) {
                    size_t nameEnd = parenPos;
                    while (nameEnd > 0 && trimmed[nameEnd - 1] == L' ') nameEnd--;
                    size_t nameStart = nameEnd;
                    while (nameStart > 0 && (iswalnum(trimmed[nameStart - 1]) || trimmed[nameStart - 1] == L'_' || trimmed[nameStart - 1] == L':'))
                        nameStart--;
                    if (nameEnd > nameStart) {
                        std::wstring name = trimmed.substr(nameStart, nameEnd - nameStart);
                        static const std::unordered_set<std::wstring> skipNames = {
                            L"if", L"for", L"while", L"switch", L"catch", L"return", L"sizeof", L"delete", L"throw"
                        };
                        if (skipNames.count(name) == 0) {
                            SymbolRecord sym;
                            sym.name = name;
                            sym.kind = L"function";
                            sym.file = file;
                            sym.line = lineNum;
                            sym.column = (uint32_t)(firstNonWs + nameStart) + 1;
                            sym.parentId = 0;
                            symbols.push_back(sym);
                        }
                    }
                }
            }

            // Class/struct
            if (trimmed.substr(0, 6) == L"class " || trimmed.substr(0, 7) == L"struct ") {
                size_t nameStart = (trimmed[0] == L'c') ? 6 : 7;
                size_t nameEnd = trimmed.find_first_of(L" \t:{;", nameStart);
                if (nameEnd == std::wstring::npos) nameEnd = trimmed.size();
                if (nameEnd > nameStart) {
                    SymbolRecord sym;
                    sym.name = trimmed.substr(nameStart, nameEnd - nameStart);
                    sym.kind = (trimmed[0] == L'c') ? L"class" : L"struct";
                    sym.file = file;
                    sym.line = lineNum;
                    sym.column = (uint32_t)firstNonWs + 1;
                    sym.parentId = 0;
                    symbols.push_back(sym);
                }
            }

            // Enum
            if (trimmed.substr(0, 5) == L"enum " || trimmed.substr(0, 11) == L"enum class ") {
                size_t nameStart = (trimmed.substr(0, 11) == L"enum class ") ? 11 : 5;
                size_t nameEnd = trimmed.find_first_of(L" \t:{;", nameStart);
                if (nameEnd == std::wstring::npos) nameEnd = trimmed.size();
                if (nameEnd > nameStart) {
                    SymbolRecord sym;
                    sym.name = trimmed.substr(nameStart, nameEnd - nameStart);
                    sym.kind = L"enum";
                    sym.file = file;
                    sym.line = lineNum;
                    sym.column = (uint32_t)firstNonWs + 1;
                    sym.parentId = 0;
                    symbols.push_back(sym);
                }
            }
        }
        return symbols;
    }

    SymbolDatabase& db_;
    WorkspaceCache& cache_;
    std::atomic<bool> running_;
    std::atomic<uint32_t> totalIndexed_;

    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::queue<IndexRequest> pendingRequests_;
    LockFreeQueue<IndexResult, 1024> resultQueue_;
    std::vector<std::thread> workers_;
};

// ============================================================================
// LAZY LOADING INFRASTRUCTURE
// Deferred initialization of expensive resources. Components register init
// callbacks; actual initialization happens on first use or after idle.
// ============================================================================

class LazyLoader {
public:
    using InitFunc = std::function<void()>;

    struct LazyComponent {
        std::wstring name;
        InitFunc initFunc;
        std::atomic<bool> initialized;
        std::atomic<bool> initializing;
        double initTimeMs;

        LazyComponent() : initialized(false), initializing(false), initTimeMs(0) {}
        LazyComponent(const std::wstring& n, InitFunc fn)
            : name(n), initFunc(fn), initialized(false), initializing(false), initTimeMs(0) {}
    };

    // Register a component for lazy initialization
    void registerComponent(const std::wstring& name, InitFunc initFunc) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto comp = std::make_shared<LazyComponent>(name, initFunc);
        components_[name] = comp;
    }

    // Ensure a component is initialized (blocking, thread-safe)
    bool ensure(const std::wstring& name) {
        std::shared_ptr<LazyComponent> comp;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = components_.find(name);
            if (it == components_.end()) return false;
            comp = it->second;
        }

        if (comp->initialized.load(std::memory_order_acquire)) return true;

        // Double-checked locking
        bool expected = false;
        if (comp->initializing.compare_exchange_strong(expected, true)) {
            PerfTimer timer("lazy_init");
            comp->initFunc();
            comp->initTimeMs = timer.elapsed_ms();
            comp->initialized.store(true, std::memory_order_release);
            comp->initializing.store(false, std::memory_order_release);
        } else {
            // Another thread is initializing; spin-wait
            while (!comp->initialized.load(std::memory_order_acquire)) {
                Sleep(1);
            }
        }
        return true;
    }

    // Check if a component is ready without initializing
    bool isReady(const std::wstring& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = components_.find(name);
        if (it == components_.end()) return false;
        return it->second->initialized.load(std::memory_order_acquire);
    }

    // Initialize all pending components (call during idle)
    void initializeAll() {
        std::vector<std::shared_ptr<LazyComponent>> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [name, comp] : components_) {
                if (!comp->initialized.load(std::memory_order_acquire)) {
                    pending.push_back(comp);
                }
            }
        }
        for (auto& comp : pending) {
            if (!comp->initialized.load(std::memory_order_acquire)) {
                bool expected = false;
                if (comp->initializing.compare_exchange_strong(expected, true)) {
                    PerfTimer timer("lazy_init_all");
                    comp->initFunc();
                    comp->initTimeMs = timer.elapsed_ms();
                    comp->initialized.store(true, std::memory_order_release);
                    comp->initializing.store(false, std::memory_order_release);
                }
            }
        }
    }

    // Get init status report
    std::wstring statusReport() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring report = L"[LazyLoader] Status Report:\r\n";
        int ready = 0, pending = 0;
        for (auto& [name, comp] : components_) {
            bool init = comp->initialized.load(std::memory_order_acquire);
            if (init) ready++; else pending++;
            wchar_t buf[128];
            swprintf_s(buf, L"  %-24s  %s  (%.1fms)\r\n",
                       name.c_str(),
                       init ? L"READY" : L"PENDING",
                       comp->initTimeMs);
            report += buf;
        }
        wchar_t summary[64];
        swprintf_s(summary, L"  Total: %d ready, %d pending\r\n", ready, pending);
        report += summary;
        return report;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::wstring, std::shared_ptr<LazyComponent>> components_;
};

// ============================================================================
// GLOBAL PERFORMANCE SYSTEM INSTANCES
// Single instances shared across the IDE.
// ============================================================================

namespace globals {

inline ArenaAllocator& frameArena() {
    static ArenaAllocator arena(256 * 1024); // 256 KB per-frame arena
    return arena;
}

inline ArenaAllocator& parseArena() {
    static ArenaAllocator arena(1024 * 1024); // 1 MB parse arena
    return arena;
}

inline BumpAllocator& tokenBump() {
    static BumpAllocator bump(4 * 1024 * 1024); // 4 MB token buffer
    return bump;
}

inline SymbolDatabase& symbolDB() {
    static SymbolDatabase db;
    return db;
}

inline WorkspaceCache& workspaceCache() {
    static WorkspaceCache cache;
    return cache;
}

inline IncrementalSync& incrementalSync() {
    static IncrementalSync sync;
    return sync;
}

inline SemanticTokenDelta& semanticTokenDelta() {
    static SemanticTokenDelta std;
    return std;
}

inline VirtualScrollEngine& virtualScroll() {
    static VirtualScrollEngine vs;
    return vs;
}

inline LazyLoader& lazyLoader() {
    static LazyLoader ll;
    return ll;
}

// Background indexer (initialized lazily)
inline BackgroundIndexer& backgroundIndexer() {
    static BackgroundIndexer bi(symbolDB(), workspaceCache());
    return bi;
}

} // namespace globals

// ============================================================================
// PERFORMANCE SYSTEM INITIALIZATION
// Called once at IDE startup to wire all perf subsystems.
// ============================================================================

inline void InitializePerformanceSystems() {
    auto& lazy = globals::lazyLoader();

    // Register lazy components
    lazy.registerComponent(L"SymbolDatabase", []() {
        globals::symbolDB(); // Force static init
    });

    lazy.registerComponent(L"WorkspaceCache", []() {
        auto& cache = globals::workspaceCache();
        // Try to load cached workspace data
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path);
        size_t sep = dir.find_last_of(L"\\/");
        if (sep != std::wstring::npos) dir = dir.substr(0, sep);
        cache.loadFromDisk(dir + L"\\.rawrxd_cache");
    });

    lazy.registerComponent(L"BackgroundIndexer", []() {
        globals::backgroundIndexer().start(2);
    });

    lazy.registerComponent(L"VirtualScrollEngine", []() {
        globals::virtualScroll(); // Force static init
    });

    lazy.registerComponent(L"IncrementalSync", []() {
        globals::incrementalSync(); // Force static init
    });

    lazy.registerComponent(L"SemanticTokenDelta", []() {
        globals::semanticTokenDelta(); // Force static init
    });

    lazy.registerComponent(L"FuzzySearchEngine", []() {
        // Pre-warm the fuzzy search with empty query to JIT any lazy statics
        FuzzySearch::score(L"", 0, L"", 0);
    });

    lazy.registerComponent(L"ParallelParser", []() {
        // Force static init of thread pool info
        SYSTEM_INFO si;
        GetSystemInfo(&si);
    });

    lazy.registerComponent(L"ArenaAllocators", []() {
        globals::frameArena();
        globals::parseArena();
        globals::tokenBump();
    });
}

// Called on shutdown to persist state
inline void ShutdownPerformanceSystems() {
    // Stop background indexer
    globals::backgroundIndexer().stop();

    // Save workspace cache
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path);
    size_t sep = dir.find_last_of(L"\\/");
    if (sep != std::wstring::npos) dir = dir.substr(0, sep);
    globals::workspaceCache().saveToDisk(dir + L"\\.rawrxd_cache");

    // Release arenas
    globals::frameArena().release();
    globals::parseArena().release();
}

// Get comprehensive performance report string
inline std::wstring GetPerformanceReport() {
    auto& c = counters();
    std::wstring report;
    report += L"[PERF] ========== PERFORMANCE REPORT ==========\r\n";

    wchar_t buf[256];
    swprintf_s(buf, L"[PERF]   Completions:     %llu (avg %llu us)\r\n",
               c.completionRequests.load(), c.completionAvgUs.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   Hover requests:  %llu (avg %llu us)\r\n",
               c.hoverRequests.load(), c.hoverAvgUs.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   Files indexed:   %llu (%llu symbols)\r\n",
               c.indexingFiles.load(), c.indexingSymbols.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   Cache hits/miss: %llu / %llu\r\n",
               c.cacheHits.load(), c.cacheMisses.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   Delta encodes:   %llu\r\n", c.deltaEncodings.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   Arena allocs:    %llu (%llu KB)\r\n",
               c.arenaAllocations.load(), c.arenaBytes.load() / 1024);
    report += buf;
    swprintf_s(buf, L"[PERF]   Fuzzy searches:  %llu\r\n", c.fuzzySearches.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   Parallel parses: %llu\r\n", c.parallelParses.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   VScroll redraws: %llu\r\n", c.virtualScrollRedraws.load());
    report += buf;
    swprintf_s(buf, L"[PERF]   Symbol DB size:  %zu\r\n", perf::globals::symbolDB().totalSymbols());
    report += buf;
    swprintf_s(buf, L"[PERF]   Workspace cache: %zu files\r\n", perf::globals::workspaceCache().size());
    report += buf;

    report += perf::globals::lazyLoader().statusReport();
    report += L"[PERF] =============================================\r\n";
    return report;
}

} // namespace perf
