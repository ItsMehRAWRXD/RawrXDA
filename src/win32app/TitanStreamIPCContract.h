#pragma once

// TitanStreamIPCContract.h
//
// Frozen streaming IPC contract for two-process inference:
//   Process A: model worker (background)
//   Process B: IDE frontend
//
// Transport (no JSON in hot path):
//   - Shared memory ring buffer for token chunks
//   - Two events: data_ready, space_ready
//   - One control mailbox for lifecycle (load, infer, cancel, shutdown)
//
// This file defines only ABI/layout and state machine primitives.
// Implementation code (mapping/events/threads) must adhere to this contract.

#include <cstdint>
#include <cstddef>

namespace RawrXD::TitanStreamIPC
{

static constexpr uint32_t kMagic = 0x49505258u;   // 'XRPI'
static constexpr uint32_t kVersion = 1u;
static constexpr uint32_t kMaxModelPathBytes = 512u;
static constexpr uint32_t kMaxPromptBytes = 32u * 1024u;
static constexpr uint32_t kDefaultSlotCount = 256u;
static constexpr uint32_t kDefaultSlotBytes = 1024u;

enum class TransportState : uint32_t
{
    Uninitialized = 0,
    Ready = 1,
    LoadingModel = 2,
    InferActive = 3,
    Cancelling = 4,
    Draining = 5,
    WorkerStopped = 6,
    Faulted = 7
};

enum class MailboxCommand : uint32_t
{
    None = 0,
    LoadModel = 1,
    Infer = 2,
    Cancel = 3,
    Shutdown = 4,
    Ping = 5
};

enum class MailboxStatus : uint32_t
{
    Idle = 0,
    Posted = 1,
    Acked = 2,
    Completed = 3,
    Rejected = 4
};

enum class FailureCode : uint32_t
{
    None = 0,
    InvalidContract = 1,
    WorkerCrashed = 2,
    ModelLoadFailed = 3,
    InferFailed = 4,
    InferTimedOut = 5,
    CancelTimedOut = 6,
    RingCorruption = 7,
    RingOverrun = 8,
    MailboxProtocolError = 9,
    UnsupportedCommand = 10,
    InternalError = 11
};

enum SlotFlags : uint32_t
{
    SlotNone = 0,
    SlotHasUtf8Bytes = 1u << 0,
    SlotHasTokenIds = 1u << 1,
    SlotIsFirstToken = 1u << 2,
    SlotIsFinalChunk = 1u << 3,
    SlotHasWarning = 1u << 4,
    SlotHasError = 1u << 5
};

enum MailboxFlags : uint32_t
{
    MailboxNone = 0,
    MailboxWarmWorkerReuse = 1u << 0,
    MailboxProfileFast = 1u << 1,
    MailboxProfileBalanced = 1u << 2,
    MailboxProfileDeep = 1u << 3,
    MailboxReuseKv = 1u << 4
};

struct alignas(64) ControlBlock
{
    uint32_t magic;                  // kMagic
    uint32_t version;                // kVersion
    uint32_t headerBytes;            // sizeof(ControlBlock)
    uint32_t totalSharedBytes;       // full mapping size

    uint32_t state;                  // TransportState
    uint32_t failureCode;            // FailureCode
    uint32_t workerPid;
    uint32_t frontendPid;

    uint32_t writeSlot;              // worker-owned write cursor
    uint32_t readSlot;               // frontend-owned read cursor
    uint32_t slotCount;              // ring capacity in slots
    uint32_t slotBytes;              // payload bytes per slot

    uint32_t requestSeq;             // monotonic infer request id
    uint32_t emittedSeq;             // last emitted slot sequence id
    uint32_t lastAckSeq;             // frontend ack for ring consumption
    uint32_t reserved0;

    uint64_t heartbeatQpcWorker;     // worker heartbeat
    uint64_t heartbeatQpcFrontend;   // frontend heartbeat
    uint64_t lastStateChangeQpc;
    uint64_t reserved1;
};

struct alignas(64) Mailbox
{
    uint32_t command;                // MailboxCommand
    uint32_t status;                 // MailboxStatus
    uint32_t seq;                    // command sequence id
    uint32_t flags;                  // MailboxFlags

    uint32_t maxTokens;              // infer cap
    uint32_t timeoutMs;              // infer timeout/cancel budget
    uint32_t promptBytes;            // used bytes in promptUtf8
    uint32_t modelPathBytes;         // used bytes in modelPathUtf8

    char modelPathUtf8[kMaxModelPathBytes];
    char promptUtf8[kMaxPromptBytes];
};

struct alignas(32) SlotHeader
{
    uint32_t requestSeq;
    uint32_t slotSeq;
    uint32_t flags;                  // SlotFlags
    uint32_t byteCount;              // bytes in payload[]

    uint32_t tokenCount;             // optional token id count
    uint32_t errorCode;              // worker-local detail (0 if none)
    uint32_t reserved0;
    uint32_t reserved1;
};

struct alignas(64) MetricsBlock
{
    uint32_t requestSeq;
    uint32_t queueDepth;             // instantaneous ring fill
    uint32_t queueDepthHighWater;
    uint32_t droppedChunks;

    uint32_t firstTokenMs;
    uint32_t tokenIntervalP50Us;
    uint32_t tokenIntervalP95Us;
    uint32_t emittedTokens;

    uint64_t startedQpc;
    uint64_t firstTokenQpc;
    uint64_t completedQpc;
    uint64_t reserved0;
};

// Shared memory layout (single mapping):
// [ControlBlock][Mailbox][MetricsBlock][SlotHeader * slotCount][slot payload bytes]
//
// Event contract:
// - data_ready : worker signals after writing one or more slots.
// - space_ready: frontend signals after advancing readSlot.

inline constexpr uint32_t RingNext(uint32_t index, uint32_t slotCount) noexcept
{
    return (slotCount == 0u) ? 0u : ((index + 1u) % slotCount);
}

inline constexpr bool RingIsFull(uint32_t writeSlot, uint32_t readSlot, uint32_t slotCount) noexcept
{
    return RingNext(writeSlot, slotCount) == readSlot;
}

inline constexpr bool RingIsEmpty(uint32_t writeSlot, uint32_t readSlot) noexcept
{
    return writeSlot == readSlot;
}

inline constexpr size_t SlotHeaderRegionBytes(uint32_t slotCount) noexcept
{
    return static_cast<size_t>(slotCount) * sizeof(SlotHeader);
}

inline constexpr size_t SlotPayloadRegionBytes(uint32_t slotCount, uint32_t slotBytes) noexcept
{
    return static_cast<size_t>(slotCount) * static_cast<size_t>(slotBytes);
}

inline constexpr size_t MinimumSharedBytes(uint32_t slotCount, uint32_t slotBytes) noexcept
{
    return sizeof(ControlBlock) + sizeof(Mailbox) + sizeof(MetricsBlock) +
           SlotHeaderRegionBytes(slotCount) + SlotPayloadRegionBytes(slotCount, slotBytes);
}

inline constexpr bool ValidateContract(const ControlBlock& c) noexcept
{
    return c.magic == kMagic && c.version == kVersion &&
           c.slotCount > 1u && c.slotBytes > 0u &&
           c.headerBytes >= sizeof(ControlBlock);
}

} // namespace RawrXD::TitanStreamIPC
