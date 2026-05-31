// =============================================================================
// ipc/shm_channel.hpp — Zero-copy shared-memory IPC channel
// =============================================================================
// Single-producer / single-consumer lock-free ring built on a Win32 named
// file-mapping.  No heap allocation in the hot path; the entire ring lives
// inside the mapped region.
//
// Usage (writer process):
//   ShmChannel ch("rawrxd.ipc.01", 256/*slots*/, true/*create*/);
//   ch.write({buf, len});          // zero-alloc if payload <= SLOT_BODY
//
// Usage (reader process):
//   ShmChannel ch("rawrxd.ipc.01", 256, false/*open*/);
//   ShmFrame f;
//   if (ch.read(f)) { /* f.data[0..f.size-1] */ }
//
// Thread-safety: single writer, single reader only.  Use two channels for
// bidirectional communication.
// =============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <string>
#include <span>
#include <optional>
#include <vector>

namespace RawrXD::IPC {

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------
static constexpr uint32_t SHM_SLOT_BODY  = 4080;   // bytes of payload per slot
static constexpr uint32_t SHM_MAGIC      = 0x52585043; // "RXPC"
static constexpr uint32_t SHM_VERSION    = 1;

// ---------------------------------------------------------------------------
// On-wire structures (live entirely inside the shared mapping)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct alignas(64) ShmSlot {
    uint32_t size;                    // payload bytes written (0 = empty)
    uint32_t seq;                     // sequence counter (detect torn writes)
    uint8_t  data[SHM_SLOT_BODY];
};

struct ShmHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t capacity;                // number of ShmSlots
    uint32_t _pad;
    alignas(64) std::atomic<uint32_t> write_head; // producer atomically bumps
    alignas(64) std::atomic<uint32_t> read_head;  // consumer atomically bumps
    // ShmSlot ring[] follows immediately
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
// Frame returned from read()  (stack-only, references slot data transiently)
// ---------------------------------------------------------------------------
struct ShmFrame {
    const uint8_t* data = nullptr;
    uint32_t       size = 0;
    uint32_t       seq  = 0;
};

// ---------------------------------------------------------------------------
// ShmChannel
// ---------------------------------------------------------------------------
class ShmChannel {
public:
    ShmChannel() = default;
    ~ShmChannel() { close(); }

    ShmChannel(const ShmChannel&)            = delete;
    ShmChannel& operator=(const ShmChannel&) = delete;

    // -----------------------------------------------------------------------
    // open() — create=true on producer side, false on consumer side.
    // capacity must be a power-of-two and ≥ 2.
    // -----------------------------------------------------------------------
    bool open(const std::string& name, uint32_t capacity_slots, bool create) {
        if (m_view) return true; // already open

        // capacity must be power-of-two
        if (capacity_slots < 2 || (capacity_slots & (capacity_slots - 1)) != 0)
            return false;

        m_capacity = capacity_slots;
        m_mask     = capacity_slots - 1;

        const SIZE_T region = static_cast<SIZE_T>(sizeof(ShmHeader))
                            + static_cast<SIZE_T>(capacity_slots) * sizeof(ShmSlot);

        if (create) {
            m_handle = CreateFileMappingA(
                INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                static_cast<DWORD>(region >> 32),
                static_cast<DWORD>(region & 0xFFFFFFFF),
                name.c_str());
            if (!m_handle) return false;

            m_view = reinterpret_cast<uint8_t*>(
                MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, region));
            if (!m_view) { close(); return false; }

            // Initialise header
            auto* hdr = header();
            hdr->magic     = SHM_MAGIC;
            hdr->version   = SHM_VERSION;
            hdr->capacity  = capacity_slots;
            hdr->write_head.store(0, std::memory_order_relaxed);
            hdr->read_head .store(0, std::memory_order_relaxed);
            // Zero all slots
            std::memset(slots(), 0, capacity_slots * sizeof(ShmSlot));
            m_creator = true;
        } else {
            m_handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
            if (!m_handle) return false;

            m_view = reinterpret_cast<uint8_t*>(
                MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));
            if (!m_view) { close(); return false; }

            auto* hdr = header();
            if (hdr->magic != SHM_MAGIC || hdr->version != SHM_VERSION)
                { close(); return false; }

            m_capacity = hdr->capacity;
            m_mask     = m_capacity - 1;
        }
        return true;
    }

    // Convenience constructor
    ShmChannel(const std::string& name, uint32_t capacity, bool create) {
        open(name, capacity, create);
    }

    bool is_open() const { return m_view != nullptr; }

    // -----------------------------------------------------------------------
    // write() — producer only.  payload must be <= SHM_SLOT_BODY bytes.
    // Returns false if the ring is full (back-pressure).
    // -----------------------------------------------------------------------
    bool write(std::span<const uint8_t> payload) {
        if (!m_view || payload.size() > SHM_SLOT_BODY) return false;

        auto*    hdr = header();
        uint32_t wh  = hdr->write_head.load(std::memory_order_relaxed);
        uint32_t rh  = hdr->read_head .load(std::memory_order_acquire);

        if ((wh - rh) >= m_capacity) return false; // ring full

        ShmSlot& slot = slots()[wh & m_mask];
        // Write seq first to signal in-progress, then data, then committed size
        slot.seq = wh;
        std::memcpy(slot.data, payload.data(), payload.size());
        // Release-store: reader sees consistent data
        std::atomic_thread_fence(std::memory_order_release);
        slot.size = static_cast<uint32_t>(payload.size());

        hdr->write_head.store(wh + 1, std::memory_order_release);
        return true;
    }

    bool write(const void* buf, uint32_t len) {
        return write(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(buf), len));
    }

    // Convenience: write a string (NUL not included)
    bool write(const std::string& s) {
        return write(reinterpret_cast<const uint8_t*>(s.data()),
                     static_cast<uint32_t>(s.size()));
    }

    // -----------------------------------------------------------------------
    // read() — consumer only.  Returns a frame whose .data pointer is valid
    // only until the next read() call (it points into the shared ring).
    // Use read_copy() if you need to retain the bytes longer.
    // -----------------------------------------------------------------------
    std::optional<ShmFrame> read() {
        if (!m_view) return std::nullopt;

        auto*    hdr = header();
        uint32_t rh  = hdr->read_head.load(std::memory_order_relaxed);
        uint32_t wh  = hdr->write_head.load(std::memory_order_acquire);

        if (rh == wh) return std::nullopt; // empty

        ShmSlot& slot = slots()[rh & m_mask];
        // Acquire fence: read size after producer's release fence
        std::atomic_thread_fence(std::memory_order_acquire);
        if (slot.size == 0) return std::nullopt; // slot not yet committed

        ShmFrame f;
        f.data = slot.data;
        f.size = slot.size;
        f.seq  = slot.seq;

        // Advance consumer head AFTER caller processes the frame.
        // Call release_read() explicitly, or use read_copy() for auto-advance.
        m_pending_rh = rh + 1;
        m_has_pending = true;
        return f;
    }

    // Advance the consumer head after the caller has finished with the frame
    void release_read() {
        if (!m_has_pending) return;
        // Zero the slot size so the producer can detect it's been consumed
        slots()[( m_pending_rh - 1) & m_mask].size = 0;
        header()->read_head.store(m_pending_rh, std::memory_order_release);
        m_has_pending = false;
    }

    // Convenience: reads into a caller-owned buffer and auto-releases
    bool read_copy(std::vector<uint8_t>& out) {
        auto opt = read();
        if (!opt) return false;
        out.assign(opt->data, opt->data + opt->size);
        release_read();
        return true;
    }

    bool read_copy(std::string& out) {
        auto opt = read();
        if (!opt) return false;
        out.assign(reinterpret_cast<const char*>(opt->data), opt->size);
        release_read();
        return true;
    }

    void close() {
        if (m_view)   { UnmapViewOfFile(m_view);   m_view   = nullptr; }
        if (m_handle) { CloseHandle(m_handle);      m_handle = nullptr; }
        m_capacity = 0;
    }

    // -----------------------------------------------------------------------
    // Stats
    // -----------------------------------------------------------------------
    uint32_t pending_slots() const {
        if (!m_view) return 0;
        auto* hdr = header();
        return hdr->write_head.load(std::memory_order_acquire)
             - hdr->read_head .load(std::memory_order_acquire);
    }

    uint32_t capacity() const { return m_capacity; }

private:
    ShmHeader* header() const {
        return reinterpret_cast<ShmHeader*>(m_view);
    }
    ShmSlot* slots() const {
        return reinterpret_cast<ShmSlot*>(m_view + sizeof(ShmHeader));
    }

    HANDLE   m_handle    = nullptr;
    uint8_t* m_view      = nullptr;
    uint32_t m_capacity  = 0;
    uint32_t m_mask      = 0;
    bool     m_creator   = false;

    // Pending consumer advance
    uint32_t m_pending_rh  = 0;
    bool     m_has_pending = false;
};

// ---------------------------------------------------------------------------
// Bi-directional pair — owns two channels (one per direction)
// ---------------------------------------------------------------------------
class ShmBiChannel {
public:
    // Server side: creates both channels
    bool open_server(const std::string& base, uint32_t cap = 256) {
        return m_tx.open(base + ".s2c", cap, true)
            && m_rx.open(base + ".c2s", cap, true);
    }
    // Client side: opens both channels
    bool open_client(const std::string& base, uint32_t cap = 256) {
        return m_tx.open(base + ".c2s", cap, true)
            && m_rx.open(base + ".s2c", cap, true);
    }

    bool send(std::span<const uint8_t> data) { return m_tx.write(data); }
    bool send(const std::string& s)          { return m_tx.write(s); }
    std::optional<ShmFrame> recv()           { return m_rx.read(); }
    void release()                           { m_rx.release_read(); }
    bool recv_copy(std::string& out)         { return m_rx.read_copy(out); }

    ShmChannel& tx() { return m_tx; }
    ShmChannel& rx() { return m_rx; }

private:
    ShmChannel m_tx, m_rx;
};

} // namespace RawrXD::IPC
