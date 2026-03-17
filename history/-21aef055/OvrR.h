// =============================================================================
// native_debugger_types.h — Phase 12: Native Debugger Shared Type Definitions
// =============================================================================
// Shared enums, structs, and constants for the RawrXD Native Debugger Engine.
// Used by:
//   - native_debugger_engine.h/.cpp   (DbgEng COM interop)
//   - RawrXD_Debug_Engine.asm          (MASM64 breakpoint kernel)
//   - debug_engine_stubs.cpp           (link stubs)
//   - Win32IDE_NativeDebugPanel.cpp    (IDE integration)
//
// Architecture: C++20 | No exceptions | No Qt | Structured PatchResult pattern
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifndef RAWRXD_NATIVE_DEBUGGER_TYPES_H
#define RAWRXD_NATIVE_DEBUGGER_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>

namespace RawrXD {
namespace Debugger {

// =============================================================================
//                        Result Pattern (no exceptions)
// =============================================================================

struct DebugResult {
    bool        success     = false;
    const char* detail      = "";
    int         errorCode   = 0;

    static DebugResult ok(const char* msg = "OK") {
        DebugResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }
    static DebugResult error(const char* msg, int code = -1) {
        DebugResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// =============================================================================
//                        Debugger State Enums
// =============================================================================

enum class DebugSessionState : uint32_t {
    Idle            = 0,    // No session active
    Attaching       = 1,    // AttachProcess in progress
    Launching       = 2,    // CreateProcess in progress
    Running         = 3,    // Target is running
    Broken          = 4,    // Hit breakpoint / exception — target is paused
    Stepping        = 5,    // Single-stepping
    Detaching       = 6,    // Detach in progress
    Terminated      = 7,    // Target exited or killed
    Error           = 8     // Unrecoverable error
};

enum class BreakpointType : uint32_t {
    Software        = 0,    // INT3 (0xCC) injection
    Hardware        = 1,    // DR0–DR3 hardware breakpoint
    Conditional     = 2,    // Software + expression guard
    DataWatch       = 3,    // Hardware write/read watchpoint
    OneShot         = 4     // Auto-remove after first hit
};

enum class BreakpointState : uint32_t {
    Enabled         = 0,
    Disabled        = 1,
    Pending         = 2,    // Deferred until module loads
    Hit             = 3,    // Currently stopped on this BP
    Removed         = 4
};

enum class StepMode : uint32_t {
    StepOver        = 0,    // Execute next instruction, skip calls
    StepInto        = 1,    // Execute next instruction, enter calls
    StepOut         = 2,    // Run until current function returns
    StepToAddress   = 3,    // Run to specific address
    StepToLine      = 4     // Run to specific source line
};

enum class DebugEventType : uint32_t {
    None                    = 0x00,
    Breakpoint              = 0x01,
    SingleStep              = 0x02,
    Exception               = 0x03,
    CreateThread            = 0x04,
    ExitThread              = 0x05,
    CreateProcess           = 0x06,
    ExitProcess             = 0x07,
    LoadDLL                 = 0x08,
    UnloadDLL               = 0x09,
    OutputDebugString       = 0x0A,
    AccessViolation         = 0x0B,
    StackOverflow           = 0x0C,
    DivideByZero            = 0x0D,
    IllegalInstruction      = 0x0E,
    ModuleLoaded            = 0x0F,
    ModuleUnloaded          = 0x10,
    DataBreakpointHit       = 0x11
};

enum class RegisterClass : uint32_t {
    GeneralPurpose  = 0,    // RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP, R8–R15
    Flags           = 1,    // RFLAGS
    InstructionPtr  = 2,    // RIP
    Segment         = 3,    // CS, DS, ES, FS, GS, SS
    Debug           = 4,    // DR0–DR3, DR6, DR7
    FloatingPoint   = 5,    // XMM0–XMM15, YMM0–YMM15
    SSE             = 6,    // MXCSR
    AVX             = 7     // ZMM0–ZMM31
};

enum class MemoryProtection : uint32_t {
    NoAccess        = 0x01,
    ReadOnly        = 0x02,
    ReadWrite       = 0x04,
    Execute         = 0x10,
    ExecuteRead     = 0x20,
    ExecuteReadWrite= 0x40,
    Guard           = 0x100
};

enum class DisasmSyntax : uint32_t {
    Intel           = 0,
    ATT             = 1,
    MASM            = 2
};

// =============================================================================
//                        Data Structures
// =============================================================================

// ---- Hardware breakpoint DR slot ----
struct HardwareBreakpointSlot {
    uint32_t    slotIndex       = 0;        // DR0=0, DR1=1, DR2=2, DR3=3
    uint64_t    address         = 0;
    uint32_t    sizeBytes       = 1;        // 1, 2, 4, or 8
    uint32_t    condition       = 0;        // 0=exec, 1=write, 3=read/write
    bool        active          = false;
};

// ---- Native breakpoint ----
struct NativeBreakpoint {
    uint32_t            id              = 0;
    BreakpointType      type            = BreakpointType::Software;
    BreakpointState     state           = BreakpointState::Enabled;
    uint64_t            address         = 0;
    std::string         module;                 // e.g. "ntdll.dll"
    std::string         symbol;                 // e.g. "NtCreateFile"
    std::string         sourceFile;             // e.g. "main.cpp"
    int                 sourceLine      = 0;
    std::string         condition;              // Expression for conditional BP
    uint64_t            hitCount        = 0;
    uint64_t            hitLimit        = 0;    // 0 = unlimited
    uint8_t             originalByte    = 0;    // Saved byte for INT3 restore
    HardwareBreakpointSlot hwSlot;              // DR slot info (if hardware)
    std::string         logMessage;             // Print without stopping
    bool                autoDelete      = false;// OneShot behavior
};

// ---- CPU register snapshot ----
struct RegisterSnapshot {
    // General purpose (x64)
    uint64_t    rax     = 0;
    uint64_t    rbx     = 0;
    uint64_t    rcx     = 0;
    uint64_t    rdx     = 0;
    uint64_t    rsi     = 0;
    uint64_t    rdi     = 0;
    uint64_t    rbp     = 0;
    uint64_t    rsp     = 0;
    uint64_t    r8      = 0;
    uint64_t    r9      = 0;
    uint64_t    r10     = 0;
    uint64_t    r11     = 0;
    uint64_t    r12     = 0;
    uint64_t    r13     = 0;
    uint64_t    r14     = 0;
    uint64_t    r15     = 0;

    // Instruction pointer & flags
    uint64_t    rip     = 0;
    uint64_t    rflags  = 0;

    // Segment registers
    uint16_t    cs      = 0;
    uint16_t    ds      = 0;
    uint16_t    es      = 0;
    uint16_t    fs      = 0;
    uint16_t    gs      = 0;
    uint16_t    ss      = 0;

    // Debug registers
    uint64_t    dr0     = 0;
    uint64_t    dr1     = 0;
    uint64_t    dr2     = 0;
    uint64_t    dr3     = 0;
    uint64_t    dr6     = 0;
    uint64_t    dr7     = 0;

    // XMM registers (first 4 for display — full set in raw buffer)
    struct XMMReg {
        uint64_t    low     = 0;
        uint64_t    high    = 0;
    };
    XMMReg      xmm[16] = {};

    // MXCSR
    uint32_t    mxcsr   = 0;

    // Raw CONTEXT buffer pointer (for DbgEng)
    void*       rawContext       = nullptr;
    uint32_t    rawContextSize   = 0;
};

// ---- Stack frame ----
struct NativeStackFrame {
    uint32_t    frameIndex      = 0;
    uint64_t    instructionPtr  = 0;    // RIP
    uint64_t    returnAddress   = 0;
    uint64_t    framePointer    = 0;    // RBP
    uint64_t    stackPointer    = 0;    // RSP
    std::string module;
    std::string function;
    std::string sourceFile;
    int         sourceLine      = 0;
    uint64_t    displacement    = 0;    // Offset from function start
    std::map<std::string, std::string> locals;  // name → formatted value
    bool        hasSource       = false;
};

// ---- Loaded module ----
struct DebugModule {
    std::string name;
    std::string path;
    uint64_t    baseAddress     = 0;
    uint64_t    size            = 0;
    uint32_t    timestamp       = 0;
    uint32_t    checksum        = 0;
    bool        symbolsLoaded   = false;
    std::string symbolPath;
    std::string pdbSignature;
};

// ---- Memory region ----
struct MemoryRegion {
    uint64_t            baseAddress     = 0;
    uint64_t            size            = 0;
    MemoryProtection    protection      = MemoryProtection::NoAccess;
    uint32_t            state           = 0;    // MEM_COMMIT, MEM_RESERVE, MEM_FREE
    uint32_t            type            = 0;    // MEM_IMAGE, MEM_MAPPED, MEM_PRIVATE
    std::string         moduleName;             // If MEM_IMAGE
};

// ---- Disassembled instruction ----
struct DisassembledInstruction {
    uint64_t    address         = 0;
    std::string mnemonic;               // "mov", "call", "jmp", etc.
    std::string operands;               // "rax, [rbp-8]"
    std::string fullText;               // "mov    rax, [rbp-8]"
    std::string bytes;                  // "48 8B 45 F8"
    uint32_t    length          = 0;    // Instruction byte length
    std::string symbol;                 // Resolved symbol if any
    bool        isCall          = false;
    bool        isJump          = false;
    bool        isReturn        = false;
    bool        hasBreakpoint   = false;
    bool        isCurrentIP     = false;
};

// ---- Debug event ----
struct DebugEvent {
    DebugEventType      type            = DebugEventType::None;
    uint64_t            timestamp       = 0;    // GetTickCount64
    uint32_t            processId       = 0;
    uint32_t            threadId        = 0;
    uint64_t            address         = 0;    // Exception address / BP address
    uint32_t            exceptionCode   = 0;
    std::string         description;
    std::string         module;
    std::string         symbol;
    bool                firstChance     = true;
    RegisterSnapshot    registers;
    std::vector<NativeStackFrame> callStack;
};

// ---- Expression evaluation result ----
struct EvalResult {
    bool        success         = false;
    std::string expression;
    std::string value;
    std::string type;
    uint64_t    rawValue        = 0;
    bool        isPointer       = false;
    bool        isFloat         = false;
};

// ---- Watch expression ----
struct NativeWatch {
    uint32_t    id              = 0;
    std::string expression;
    EvalResult  lastResult;
    bool        enabled         = true;
    uint32_t    updateCount     = 0;
};

// ---- Thread info ----
struct DebugThread {
    uint32_t    threadId        = 0;
    uint64_t    startAddress    = 0;
    uint64_t    tebAddress      = 0;
    std::string name;
    bool        isSuspended     = false;
    bool        isCurrent       = false;
    int         priority        = 0;
    RegisterSnapshot registers;
};

// =============================================================================
//                        Session Statistics
// =============================================================================

struct DebugSessionStats {
    uint64_t    totalBreakpointsSet     = 0;
    uint64_t    totalBreakpointHits     = 0;
    uint64_t    totalExceptions         = 0;
    uint64_t    totalSteps              = 0;
    uint64_t    totalMemoryReads        = 0;
    uint64_t    totalMemoryWrites       = 0;
    uint64_t    totalDisassembled       = 0;
    uint64_t    totalEvalsPerformed     = 0;
    uint64_t    modulesLoaded           = 0;
    uint64_t    threadsCreated          = 0;
    uint64_t    sessionsStarted         = 0;
    double      totalSessionTimeMs      = 0.0;
    double      lastEventLatencyMs      = 0.0;
    mutable std::mutex statsMutex;
};

// =============================================================================
//                        Configuration
// =============================================================================

struct DebugConfig {
    // Paths
    std::string symbolPath      = "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";
    std::string sourceSearchPath;
    std::string imagePath;

    // Behavior
    bool        breakOnEntry        = true;     // Break at process entry point
    bool        breakOnDllLoad      = false;    // Break when DLLs load
    bool        breakOnException    = true;     // Break on first-chance exceptions
    bool        breakOnAccessViolation = true;
    bool        autoLoadSymbols     = true;
    bool        enableSourceStepping= true;
    bool        resolveUnloadedSymbols = false;

    // Display
    DisasmSyntax    disasmSyntax    = DisasmSyntax::Intel;
    uint32_t        disasmLines     = 32;       // Lines to disassemble at once
    uint32_t        memoryColumns   = 16;       // Bytes per row in hex dump
    uint32_t        maxStackFrames  = 256;
    uint32_t        maxWatchCount   = 64;

    // Hardware
    uint32_t        hwBreakpointSlots = 4;      // DR0–DR3
    bool            preferHardwareBP  = false;   // Use HW BP when possible

    // Event log
    uint32_t        maxEventHistory = 4096;
};

// =============================================================================
//                        Callback Types
// =============================================================================

// Function pointer callbacks (no std::function — per project rules)
typedef void (*DebugEventCallback)(const DebugEvent* event, void* userData);
typedef void (*DebugOutputCallback)(const char* text, uint32_t category, void* userData);
typedef void (*DebugStateCallback)(DebugSessionState newState, void* userData);
typedef void (*BreakpointHitCallback)(const NativeBreakpoint* bp, const RegisterSnapshot* regs, void* userData);

// =============================================================================
//                    ASM Kernel Interface (extern "C")
// =============================================================================
// These match the MASM64 exports in RawrXD_Debug_Engine.asm.
// Stubs in debug_engine_stubs.cpp provide fallback when .obj is unavailable.

#ifdef __cplusplus
extern "C" {
#endif

// Software breakpoint: inject INT3 at address, return original byte
uint32_t Dbg_InjectINT3(uint64_t targetAddress, uint8_t* outOriginalByte);

// Software breakpoint: restore original byte at address
uint32_t Dbg_RestoreINT3(uint64_t targetAddress, uint8_t originalByte);

// Hardware breakpoint: set DR0–DR3 for target thread
uint32_t Dbg_SetHardwareBreakpoint(uint64_t threadHandle, uint32_t slotIndex,
                                    uint64_t address, uint32_t condition, uint32_t sizeBytes);

// Hardware breakpoint: clear specific DR slot
uint32_t Dbg_ClearHardwareBreakpoint(uint64_t threadHandle, uint32_t slotIndex);

// Single-step: set TF (Trap Flag) in RFLAGS for target thread
uint32_t Dbg_EnableSingleStep(uint64_t threadHandle);

// Single-step: clear TF in RFLAGS
uint32_t Dbg_DisableSingleStep(uint64_t threadHandle);

// Register snapshot: capture full CONTEXT_ALL for target thread
uint32_t Dbg_CaptureContext(uint64_t threadHandle, void* outContextBuffer, uint32_t bufferSize);

// Register write: set a specific register value in thread context
uint32_t Dbg_SetRegister(uint64_t threadHandle, uint32_t registerIndex, uint64_t value);

// Stack walk: RBP-chain walker, fills array of return addresses
uint32_t Dbg_WalkStack(uint64_t processHandle, uint64_t threadHandle,
                        uint64_t* outFrames, uint32_t maxFrames, uint32_t* outFrameCount);

// Memory read: ReadProcessMemory wrapper with page-boundary handling
uint32_t Dbg_ReadMemory(uint64_t processHandle, uint64_t address,
                         void* outBuffer, uint64_t size, uint64_t* outBytesRead);

// Memory write: WriteProcessMemory wrapper with VirtualProtect cycle
uint32_t Dbg_WriteMemory(uint64_t processHandle, uint64_t address,
                          const void* buffer, uint64_t size, uint64_t* outBytesWritten);

// Memory scan: pattern search (Boyer–Moore) in target process memory
uint32_t Dbg_MemoryScan(uint64_t processHandle, uint64_t startAddress, uint64_t regionSize,
                         const void* pattern, uint32_t patternLen, uint64_t* outFoundAddress);

// CRC32 of target memory region (for breakpoint integrity verification)
uint32_t Dbg_MemoryCRC32(uint64_t processHandle, uint64_t address, uint64_t size,
                          uint32_t* outCRC);

// RDTSC timing for profiling debug operations
uint64_t Dbg_RDTSC(void);

#ifdef __cplusplus
}
#endif

} // namespace Debugger
} // namespace RawrXD

#endif // RAWRXD_NATIVE_DEBUGGER_TYPES_H
