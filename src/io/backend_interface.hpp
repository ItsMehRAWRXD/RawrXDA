// src/io/backend_interface.hpp
#pragma once
#include <cstdint>
#include <vector>

// RAWRXD v1.1.0 - KERNEL BYPASS INTERFACE
// "The Memory Hierarchy Controller"

enum class IOBackendType {
    IO_URING_LINUX,
    IORING_WINDOWS,
    FALLBACK_MMAP // For compatibility/debug
};

struct IORequest {
    uint64_t file_offset;   // Must be 4K aligned for O_DIRECT
    uint32_t zone_index;    // Which zone in the ring buffer?
    uint32_t zone_offset;   // Offset within that zone
    uint32_t size;          // Must be 4K aligned
    uint64_t request_id;    // User data (TensorID) for tracking
};

struct IOCompletion {
    uint64_t request_id;    // The tensor that is now ready
    int32_t result_code;    // 0 = Success
};

class IDirectIOBackend {
public:
    virtual ~IDirectIOBackend() = default;

    // 1. SETUP: Open file in Direct Mode (O_DIRECT / NO_BUFFERING)
    virtual bool Initialize(const char* filepath) = 0;

    // 2. PINNING: Register the Zone Ring Buffer with the Kernel
    // This maps the user-space zones directly to NVMe DMA targets.
    virtual bool RegisterBuffers(void* ring_base_ptr, size_t total_size, size_t zone_count) = 0;

    // 3. SUBMISSION: Queue a read operation (Non-blocking)
    // Adds to the SQ (Submission Queue) but may not flush immediately.
    virtual void SubmitRead(const IORequest& req) = 0;

    // 4. FLUSH: Kick the doorbell
    // Forces the kernel/hardware to process the queued SQEs.
    virtual void Flush() = 0;

    // 5. POLL: Check for completions (Zero syscalls in SQPOLL mode)
    // Populates the provided vector with completed Tensor IDs.
    virtual int PollCompletions(std::vector<IOCompletion>& out_events) = 0;

    // 6. TEARDOWN
    virtual void Shutdown() = 0;
};

// Factory to spin up the correct backend based on compile target
IDirectIOBackend* CreateIOBackend(IOBackendType type);
