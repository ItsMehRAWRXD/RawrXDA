#pragma once
#include <cstdint>
#include <cstddef>

namespace rawrxd::ipc {

#pragma pack(push, 1)

enum class MessageType : uint16_t {
    HEARTBEAT    = 0x0000,

    // UI -> Native commands (0x1000 range)
    UI_CMD            = 0x1000,
    REQ_DISASM        = 0x1001,
    REQ_SYMBOL        = 0x1002,
    SET_BP            = 0x1003,
    REQ_READ_MEM      = 0x1004,
    REQ_IMPORTS       = 0x1005,
    REQ_WRITE_MEM     = 0x1006,
    REQ_RESOLVE_NAME  = 0x1007,
    SET_BP_EXT        = 0x1008,
    REQ_WATCH_ADD     = 0x1009,
    REQ_WATCH_REMOVE  = 0x100A,
    REQ_EMIT_CODE     = 0x100B,
    REQ_WATCH         = 0x100C,

    // Swarm (0x1500 range)
    REQ_SWARM_CONNECT   = 0x1501,
    REQ_SWARM_SYNC      = 0x100B, // Phase 14: Requested Update
    REQ_SWARM_INFERENCE = 0x1503,
    REQ_SWARM_SHARD     = 0x1504,

    // Native -> UI data/events (0x2000 range)
    DBG_EVT             = 0x2000,
    MOD_LOAD            = 0x2001,
    DATA_LOG            = 0x2002,
    DATA_MEM            = 0x2003,
    DATA_RESOLVE_RESULT = 0x2004,
    DATA_WATCH_UPDATE   = 0x2005,
    DATA_EMIT_RESULT    = 0x2006,
    DATA_SWARM_HEARTBEAT = 0x2006, // Phase 14: Requested Update

    // Extended payload streams (0x3000 range)
    DATA_SYMBOL        = 0x3001,
    DATA_IMPORTS       = 0x3003,
    DATA_DISASM        = 0x3004,
};

struct MsgEmitCode {
    uint64_t target_address;
    uint32_t source_text_len;
    // Followed by source_text_len bytes UTF8
};

struct MsgEmitResult {
    uint64_t target_address;
    uint32_t bytes_written;
    uint32_t status_code; // 0=Success, 1=Fail
};

struct MsgDisasmChunk {
    uint64_t address;
    uint8_t  raw_bytes[15];
    uint8_t  length;
    char     mnemonic[64];
    uint32_t flags;
};

// ... existing structs ...

struct MsgReadMem {
    uint64_t address;
    uint32_t size;
};

struct MsgWriteMem {
    uint64_t address;
    uint32_t size;
    // Followed by size bytes
};

struct MsgWatchSymbol {
    uint64_t module_base;
    uint16_t name_len;
    // Followed by name
};

struct MsgResolveName {
    uint64_t module_base;
    uint16_t name_len;
    // Followed by name string (UTF8)
};

struct MsgResolveAddr {
    uint64_t address;
};

struct MsgWatchAdd {
    uint64_t address;
    uint32_t size;
    uint16_t label_len;
    // Followed by label string
};

struct MsgResolveResult {
    uint64_t address;
    uint16_t name_len;
    // Followed by name string
};

struct MsgWatchUpdate {
    uint64_t address;
    uint32_t size;
    uint16_t label_len;
    // Followed by label_len bytes label, then 'size' bytes data
};

struct RawrIPCHeader {
    uint32_t magic;      // 0x52415752 "RAWR"
    uint16_t version;    // 0x0001
    MessageType msg_type;
    uint32_t sequence;
    uint64_t timestamp;
    uint32_t payload_len;
    uint32_t crc32;      // CRC32 of payload
};

// Example Payloads
struct MsgDebugEvent {
    uint32_t thread_id;
    uint64_t rip;
    uint64_t registers[16]; // rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8-r15
    uint32_t exception_code;
};

struct MsgModuleLoad {
    uint64_t base_address;
    uint64_t size;
    uint32_t path_len;
    // Followed by path_len bytes of UTF-8 string
};

struct MsgModuleSnapshot {
    uint32_t total_modules;
    // Followed by N instances of MsgModuleLoad + strings (packed)
};

#pragma pack(pop)

// Protocol v1.1 Stability Checks
static_assert(sizeof(RawrIPCHeader) == 28, "RawrIPCHeader size mismatch - check packing");
static_assert(offsetof(RawrIPCHeader, msg_type) == 6, "RawrIPCHeader offset mismatch: msg_type");
static_assert(offsetof(RawrIPCHeader, payload_len) == 20, "RawrIPCHeader offset mismatch: payload_len");

static_assert(sizeof(MsgResolveName) == 10, "MsgResolveName size mismatch");
static_assert(sizeof(MsgWatchAdd) == 14, "MsgWatchAdd size mismatch");
static_assert(sizeof(MsgWatchUpdate) == 14, "MsgWatchUpdate size mismatch");

static constexpr uint32_t RAWR_IPC_MAGIC = 0x52415752;

} // namespace rawrxd::ipc
