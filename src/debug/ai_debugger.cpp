// =============================================================================
// ai_debugger.cpp — AI-Assisted Debug Agent (NativeDebuggerEngine Backend)
// =============================================================================
// Production debug agent that uses the DbgEng COM-based NativeDebuggerEngine
// to provide AI-assisted debugging capabilities:
//
//   - Launch/Attach to processes with automatic symbol resolution
//   - AI-suggested breakpoints based on crash context and code patterns
//   - Exception analysis with natural language explanations
//   - Register/memory state snapshot formatting for LLM consumption
//   - Crash dump triage with root cause hypothesis generation
//   - Stack trace summarization and call chain reasoning
//
// Architecture: Stateless query model. Each operation queries the debugger
//               engine singleton and returns structured analysis.
// Dependencies: NativeDebuggerEngine (src/core/)
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "../core/native_debugger_engine.h"
#include "../../include/debug/ai_debugger.h"
#include "../agentic/SovereignInferenceClient.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Debug {

// =============================================================================
// kKnownExceptions — Known Windows exception codes with human descriptions
// (ExceptionInfo struct defined in ai_debugger.h)
// =============================================================================
static const ExceptionInfo kKnownExceptions[] = {
    { 0xC0000005, "ACCESS_VIOLATION",
      "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.",
      "Null pointer dereference, use-after-free, buffer overrun, or accessing freed memory." },
    { 0xC0000094, "INTEGER_DIVIDE_BY_ZERO",
      "The thread tried to divide an integer value by an integer divisor of zero.",
      "Division where the divisor was not validated before use." },
    { 0xC00000FD, "STACK_OVERFLOW",
      "The thread used up its stack.",
      "Infinite recursion, deeply nested function calls, or large stack allocations in loops." },
    { 0xC0000409, "STATUS_STACK_BUFFER_OVERRUN",
      "The system detected an overrun of a stack-based buffer.",
      "Buffer overflow detected by /GS security cookie check. Possible exploit attempt or off-by-one error." },
    { 0xC0000374, "HEAP_CORRUPTION",
      "A heap has been corrupted.",
      "Double-free, heap buffer overrun, use-after-free, or cross-heap deallocation." },
    { 0xC000001D, "ILLEGAL_INSTRUCTION",
      "The thread tried to execute an illegal instruction.",
      "Corrupted code pointer, JIT code generation bug, or deliberate anti-debug trap." },
    { 0xC0000096, "PRIVILEGED_INSTRUCTION",
      "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.",
      "Ring-0 instruction in user mode, typically from corrupted control flow." },
    { 0x80000003, "BREAKPOINT",
      "A breakpoint has been reached.",
      "Deliberate INT3 (debugger breakpoint, assertion failure, or __debugbreak())." },
    { 0x80000004, "SINGLE_STEP",
      "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.",
      "Single-step tracing active (debugger stepping)." },
    { 0xC0000008, "INVALID_HANDLE",
      "An invalid handle was specified.",
      "Using a closed handle, double-closing, or handle value corruption." },
    { 0xE06D7363, "CPP_EXCEPTION",
      "C++ exception thrown (Microsoft Visual C++ runtime).",
      "Unhandled C++ throw expression." },
    { 0xC00000C5, "IN_PAGE_ERROR",
      "The instruction at the address referenced memory that could not be paged in.",
      "Memory-mapped file became unavailable (network disconnect, USB removal, corrupted page file)." },
};

static const ExceptionInfo* FindException(uint32_t code) {
    for (const auto& ex : kKnownExceptions) {
        if (ex.code == code) return &ex;
    }
    return nullptr;
}

// =============================================================================
// AIDebugAgent — The production AI-assisted debugger
// =============================================================================
#pragma optimize("", off)  // Preserve external-linkage symbols

// Forward declarations of file-scope helpers (defined after member functions)
static std::string FormatStackForAI(const std::vector<Debugger::NativeStackFrame>&);
static std::string FormatRegistersForAI(const Debugger::RegisterSnapshot&);
static std::string GenerateHypothesis(const AIDebugAgent::ExceptionAnalysis&,
                                       const std::vector<Debugger::NativeStackFrame>&,
                                       const Debugger::RegisterSnapshot&);
static std::vector<std::string> GenerateSuggestedActions(
        const AIDebugAgent::ExceptionAnalysis&,
        const std::vector<Debugger::NativeStackFrame>&);
static std::string InterpretMemoryContents(const std::vector<uint8_t>&,
                                            uint64_t,
                                            const AIDebugAgent::MemoryAnalysis&);
static std::string AnnotateInstruction(const Debugger::DisassembledInstruction&,
                                        Debugger::NativeDebuggerEngine&);
static const char* DebugStateToString(Debugger::DebugSessionState);

// ---- Singleton Access ----
AIDebugAgent& AIDebugAgent::Instance() {
    static AIDebugAgent instance;
    return instance;
}

// ---- Session Management ----
AIDebugAgent::LaunchResult AIDebugAgent::LaunchTarget(const std::string& exePath,
                               const std::string& args,
                               const std::string& workDir) {
        auto& engine = Debugger::NativeDebuggerEngine::Instance();

        if (!engine.isInitialized()) {
            Debugger::DebugConfig cfg;
            cfg.symbolPath = "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";
            cfg.enableSourceStepping = true;
            cfg.maxEventHistory = 500;
            auto r = engine.initialize(cfg);
            if (!r.success) {
                return {false, std::string("Engine init failed: ") + r.detail, 0};
            }
        }

        auto r = engine.launchProcess(exePath, args, workDir);
        if (!r.success) {
            return {false, std::string("Launch failed: ") + r.detail, 0};
        }

        return {true, "Process launched successfully", engine.getTargetPID()};
    }

AIDebugAgent::LaunchResult AIDebugAgent::AttachToProcess(uint32_t pid) {
        auto& engine = Debugger::NativeDebuggerEngine::Instance();

        if (!engine.isInitialized()) {
            Debugger::DebugConfig cfg;
            cfg.symbolPath = "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";
            cfg.enableSourceStepping = true;
            cfg.maxEventHistory = 500;
            auto r = engine.initialize(cfg);
            if (!r.success) {
                return {false, std::string("Engine init failed: ") + r.detail, 0};
            }
        }

        auto r = engine.attachToProcess(pid);
        if (!r.success) {
            return {false, std::string("Attach failed: ") + r.detail, 0};
        }

        return {true, "Attached to process", pid};
    }

// ---- Exception Analysis ----
AIDebugAgent::ExceptionAnalysis AIDebugAgent::AnalyzeLastException() {
        auto& engine = Debugger::NativeDebuggerEngine::Instance();
        ExceptionAnalysis result{};

        if (engine.getState() != Debugger::DebugSessionState::Broken) {
            result.hypothesis = "No active debug break. The target must be in Broken state to analyze.";
            return result;
        }

        // Get last event
        const auto* lastEvent = engine.getLastEvent();
        if (!lastEvent) {
            result.hypothesis = "No debug events recorded.";
            return result;
        }

        result.exceptionCode = lastEvent->exceptionCode;
        result.faultAddress = lastEvent->address;

        // Look up known exception
        const auto* exInfo = FindException(lastEvent->exceptionCode);
        if (exInfo) {
            result.exceptionName = exInfo->name;
            result.description = exInfo->description;
            result.commonCause = exInfo->commonCause;
        } else {
            char codeBuf[32];
            snprintf(codeBuf, sizeof(codeBuf), "0x%08X", lastEvent->exceptionCode);
            result.exceptionName = codeBuf;
            result.description = "Unknown exception code.";
        }

        // Resolve fault address to symbol
        std::string sym;
        uint64_t displacement = 0;
        if (engine.resolveSymbol(result.faultAddress, sym, displacement).success) {
            result.faultSymbol = sym;
            if (displacement > 0) {
                char dispBuf[32];
                snprintf(dispBuf, sizeof(dispBuf), "+0x%llx", (unsigned long long)displacement);
                result.faultSymbol += dispBuf;
            }
        }

        // Find module
        const auto* mod = engine.findModuleByAddress(result.faultAddress);
        if (mod) {
            result.faultModule = mod->name;
        }

        // Capture stack trace
        std::vector<Debugger::NativeStackFrame> frames;
        if (engine.walkStack(frames, 32).success) {
            result.stackSummary = FormatStackForAI(frames);
        }

        // Capture registers
        Debugger::RegisterSnapshot regs;
        if (engine.captureRegisters(regs).success) {
            result.registerSummary = FormatRegistersForAI(regs);
        }

        // Generate hypothesis based on exception type and context
        result.hypothesis = GenerateHypothesis(result, frames, regs);
        result.suggestedActions = GenerateSuggestedActions(result, frames);

        return result;
    }

    // ---- Structured Output for LLM Consumption ----
std::string AIDebugAgent::FormatAnalysisForLLM(const AIDebugAgent::ExceptionAnalysis& analysis) {
        std::ostringstream out;

        out << "## Debug Analysis Report\n\n";

        out << "### Exception\n";
        out << "- **Code:** `0x" << std::hex << std::setfill('0') << std::setw(8)
            << analysis.exceptionCode << std::dec << "` (" << analysis.exceptionName << ")\n";
        out << "- **Description:** " << analysis.description << "\n";
        out << "- **Common Cause:** " << analysis.commonCause << "\n\n";

        out << "### Fault Location\n";
        out << "- **Address:** `0x" << std::hex << analysis.faultAddress << std::dec << "`\n";
        if (!analysis.faultSymbol.empty())
            out << "- **Symbol:** `" << analysis.faultSymbol << "`\n";
        if (!analysis.faultModule.empty())
            out << "- **Module:** `" << analysis.faultModule << "`\n";
        out << "\n";

        if (!analysis.stackSummary.empty()) {
            out << "### Call Stack\n```\n" << analysis.stackSummary << "```\n\n";
        }

        if (!analysis.registerSummary.empty()) {
            out << "### Registers\n```\n" << analysis.registerSummary << "```\n\n";
        }

        if (!analysis.hypothesis.empty()) {
            out << "### Root Cause Hypothesis\n" << analysis.hypothesis << "\n\n";
        }

        if (!analysis.suggestedActions.empty()) {
            out << "### Suggested Actions\n";
            for (size_t i = 0; i < analysis.suggestedActions.size(); i++) {
                out << (i + 1) << ". " << analysis.suggestedActions[i] << "\n";
            }
        }

        return out.str();
    }

    // ---- Breakpoint Suggestions ----
std::vector<AIDebugAgent::BreakpointSuggestion> AIDebugAgent::SuggestBreakpoints(const std::string& context) {
        std::vector<BreakpointSuggestion> suggestions;

        // Universal exception-catching breakpoints
        suggestions.push_back({
            "ntdll!KiUserExceptionDispatcher",
            "Catches all user-mode exceptions before handlers execute",
            Debugger::BreakpointType::Software
        });

        suggestions.push_back({
            "ntdll!RtlRaiseException",
            "Catches explicit exception raises (C++ throw, RaiseException)",
            Debugger::BreakpointType::Software
        });

        // Context-specific suggestions
        std::string lower = context;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });

        if (lower.find("crash") != std::string::npos ||
            lower.find("access violation") != std::string::npos) {
            suggestions.push_back({
                "ntdll!RtlpHeapHandleError",
                "Catches heap corruption before it manifests as an AV",
                Debugger::BreakpointType::Software
            });
            suggestions.push_back({
                "verifier!AVrfpDphShouldFaultInject",
                "Application Verifier fault injection point (if enabled)",
                Debugger::BreakpointType::Software
            });
        }

        if (lower.find("memory") != std::string::npos ||
            lower.find("heap") != std::string::npos ||
            lower.find("leak") != std::string::npos) {
            suggestions.push_back({
                "ntdll!RtlFreeHeap",
                "Track heap free operations to find double-frees",
                Debugger::BreakpointType::Software
            });
            suggestions.push_back({
                "kernel32!HeapAlloc",
                "Track heap allocation patterns",
                Debugger::BreakpointType::Software
            });
        }

        if (lower.find("thread") != std::string::npos ||
            lower.find("deadlock") != std::string::npos ||
            lower.find("race") != std::string::npos) {
            suggestions.push_back({
                "ntdll!RtlAcquireSRWLockExclusive",
                "Track SRW lock acquisition for deadlock analysis",
                Debugger::BreakpointType::Software
            });
            suggestions.push_back({
                "kernel32!WaitForSingleObject",
                "Track synchronization waits",
                Debugger::BreakpointType::Software
            });
        }

        if (lower.find("dll") != std::string::npos ||
            lower.find("load") != std::string::npos) {
            suggestions.push_back({
                "ntdll!LdrLoadDll",
                "Catches all DLL loading events",
                Debugger::BreakpointType::Software
            });
        }

        return suggestions;
    }

    // ---- Memory Analysis ----
AIDebugAgent::MemoryAnalysis AIDebugAgent::AnalyzeMemoryRegion(uint64_t address, uint32_t size) {
        auto& engine = Debugger::NativeDebuggerEngine::Instance();
        MemoryAnalysis result{};
        result.address = address;

        if (size > 4096) size = 4096;  // Hard cap

        std::vector<uint8_t> buffer(size);
        uint64_t bytesRead = 0;
        if (!engine.readMemory(address, buffer.data(), size, &bytesRead).success) {
            result.interpretation = "Memory at this address is not readable.";
            return result;
        }
        buffer.resize(static_cast<size_t>(bytesRead));

        // Format hex dump
        result.hexDump = engine.formatHexDump(address, buffer.data(), bytesRead, 16);

        // Query memory region info
        std::vector<Debugger::MemoryRegion> regions;
        if (engine.queryMemoryRegions(regions).success) {
            for (const auto& region : regions) {
                if (address >= region.baseAddress &&
                    address < region.baseAddress + region.size) {
                    result.isCodeRegion = (static_cast<uint32_t>(region.protection) & 0x10) != 0 ||
                                          (static_cast<uint32_t>(region.protection) & 0x20) != 0;  // PAGE_EXECUTE*
                    // Heuristic: stack regions are private, committed, guard-paged
                    result.isStackRegion = (region.type == 0x20000);  // MEM_PRIVATE
                    result.isHeapRegion = (region.type == 0x20000) && !result.isStackRegion;
                    break;
                }
            }
        }

        // Resolve nearest symbol
        std::string sym;
        uint64_t disp = 0;
        if (engine.resolveSymbol(address, sym, disp).success) {
            result.nearestSymbol = sym;
            if (disp > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "+0x%llx", (unsigned long long)disp);
                result.nearestSymbol += buf;
            }
        }

        // Interpret contents
        result.interpretation = InterpretMemoryContents(buffer, address, result);

        return result;
    }

    // ---- Disassembly with Annotation ----
std::string AIDebugAgent::DisassembleWithAnnotations(uint64_t address, uint32_t lineCount) {
        auto& engine = Debugger::NativeDebuggerEngine::Instance();

        std::vector<Debugger::DisassembledInstruction> insns;
        if (!engine.disassembleAt(address, lineCount, insns).success) {
            return "Failed to disassemble at address.";
        }

        std::ostringstream out;
        for (const auto& insn : insns) {
            // Address
            char addrBuf[20];
            snprintf(addrBuf, sizeof(addrBuf), "%016llx", (unsigned long long)insn.address);
            out << addrBuf << "  ";

            // Mnemonic + operands
            out << std::left << std::setw(40) << (insn.mnemonic + " " + insn.operands);

            // Annotation for interesting instructions
            std::string annotation = AnnotateInstruction(insn, engine);
            if (!annotation.empty()) {
                out << " ; " << annotation;
            }

            out << "\n";
        }

        return out.str();
    }

    // ---- Full Debug Snapshot for LLM ----
std::string AIDebugAgent::CaptureDebugSnapshot() {
        auto& engine = Debugger::NativeDebuggerEngine::Instance();

        if (!engine.isInitialized()) {
            return "Debug engine not initialized.";
        }

        std::ostringstream out;
        out << "# Debug Session Snapshot\n\n";

        // Session info
        auto state = engine.getState();
        out << "**State:** " << DebugStateToString(state) << "\n";
        out << "**Target:** " << engine.getTargetName()
            << " (PID: " << engine.getTargetPID() << ")\n\n";

        if (state == Debugger::DebugSessionState::Broken) {
            // Exception analysis
            auto analysis = AnalyzeLastException();
            out << FormatAnalysisForLLM(analysis);
        } else if (state == Debugger::DebugSessionState::Running) {
            out << "*Target is running. Use Break to pause execution.*\n";
        } else if (state == Debugger::DebugSessionState::Idle) {
            out << "*No active debug session. Use Launch or Attach to start.*\n";
        }

        // Breakpoint list
        const auto& bps = engine.getBreakpoints();
        if (!bps.empty()) {
            out << "\n### Active Breakpoints\n";
            for (const auto& bp : bps) {
                out << "- #" << bp.id << " at `0x" << std::hex << bp.address << std::dec << "`";
                if (!bp.symbol.empty()) out << " (" << bp.symbol << ")";
                out << (bp.state == Debugger::BreakpointState::Enabled ? " [enabled]" : " [disabled]");
                out << "\n";
            }
        }

        // Module list summary
        std::vector<Debugger::DebugModule> modules;
        if (engine.enumerateModules(modules).success && !modules.empty()) {
            out << "\n### Loaded Modules (" << modules.size() << ")\n";
            int shown = 0;
            for (const auto& mod : modules) {
                if (shown >= 15) {
                    out << "  ... and " << (modules.size() - shown) << " more\n";
                    break;
                }
                out << "- `" << mod.name << "` at 0x" << std::hex << mod.baseAddress
                    << std::dec << " (" << (mod.size / 1024) << " KB)";
                if (mod.symbolsLoaded) out << " [symbols]";
                out << "\n";
                shown++;
            }
        }

        return out.str();
    }

// ---- Hypothesis Generation ----
static std::string GenerateHypothesis(const AIDebugAgent::ExceptionAnalysis& analysis,
                                    const std::vector<Debugger::NativeStackFrame>& frames,
                                    const Debugger::RegisterSnapshot& regs) {
        std::ostringstream hyp;

        if (analysis.exceptionCode == 0xC0000005) {
            // ACCESS_VIOLATION — check if read or write, check address patterns
            if (analysis.faultAddress < 0x10000) {
                hyp << "**Null pointer dereference.** The fault address `0x"
                    << std::hex << analysis.faultAddress << std::dec
                    << "` is near zero, indicating a null pointer access. ";

                if (analysis.faultAddress == 0) {
                    hyp << "Exact null — a pointer was never initialized.";
                } else {
                    hyp << "Small offset from null — likely a struct member access through a null pointer "
                        << "(offset " << analysis.faultAddress << " bytes from struct base).";
                }
            } else if (analysis.faultAddress >= 0xFFFF000000000000ULL) {
                hyp << "**Kernel address access from user mode.** The fault address `0x"
                    << std::hex << analysis.faultAddress << std::dec
                    << "` is in kernel space. This typically indicates use of an uninitialized "
                    << "or corrupted pointer with sign extension.";
            } else {
                hyp << "**Invalid memory access** at `0x" << std::hex << analysis.faultAddress
                    << std::dec << "`. ";

                // Check if it might be use-after-free (0xfeeefeee pattern)
                if ((analysis.faultAddress & 0xFFFFFFFF) == 0xFEEEFEEE ||
                    (analysis.faultAddress & 0xFFFFFFFF) == 0xDDDDDDDD) {
                    hyp << "The address contains a debug fill pattern, suggesting **use-after-free**.";
                } else if ((analysis.faultAddress & 0xFFFFFFFF) == 0xCDCDCDCD) {
                    hyp << "The address contains `0xCDCDCDCD` (uninitialized heap), suggesting "
                        << "**use of uninitialized memory**.";
                } else if ((analysis.faultAddress & 0xFFFFFFFF) == 0xCCCCCCCC) {
                    hyp << "The address contains `0xCCCCCCCC` (uninitialized stack), suggesting "
                        << "**use of uninitialized local variable**.";
                } else {
                    hyp << "Could be a buffer overrun, dangling pointer, or index out of bounds.";
                }
            }
        } else if (analysis.exceptionCode == 0xC00000FD) {
            hyp << "**Stack overflow.** ";

            // Check for recursion in stack
            if (frames.size() >= 2) {
                std::unordered_map<std::string, int> symbolCounts;
                for (const auto& frame : frames) {
                    symbolCounts[frame.function]++;
                }
                for (const auto& [sym, count] : symbolCounts) {
                    if (count > 5 && !sym.empty()) {
                        hyp << "Function `" << sym << "` appears " << count
                            << " times in the stack — this is likely **infinite recursion**.";
                        break;
                    }
                }
            }
        } else if (analysis.exceptionCode == 0xC0000409) {
            hyp << "**Stack buffer overrun detected** by the /GS security cookie check. "
                << "A function's return address or other protected stack data was overwritten. "
                << "This is a strong indicator of a buffer overflow vulnerability in `"
                << analysis.faultSymbol << "`.";
        } else if (analysis.exceptionCode == 0xC0000374) {
            hyp << "**Heap corruption detected.** A heap metadata structure was damaged. "
                << "The corruption likely occurred in a previous allocation/free operation, "
                << "not necessarily at the current call site.";
        } else if (analysis.exceptionCode == 0xE06D7363) {
            hyp << "**Unhandled C++ exception.** A `throw` statement was executed "
                << "without a matching `catch` handler in the call stack.";
        } else {
            hyp << "Exception `" << analysis.exceptionName << "` at `"
                << analysis.faultSymbol << "`. " << analysis.commonCause;
        }

        return hyp.str();
    }

    // ---- Suggested Actions ----
static std::vector<std::string> GenerateSuggestedActions(
            const AIDebugAgent::ExceptionAnalysis& analysis,
            const std::vector<Debugger::NativeStackFrame>& frames) {
        std::vector<std::string> actions;

        // Universal actions
        actions.push_back("Examine the call stack to trace the execution path leading to the crash.");

        if (analysis.exceptionCode == 0xC0000005) {
            if (analysis.faultAddress < 0x10000) {
                actions.push_back("Check if the source pointer was validated before dereferencing.");
                actions.push_back("Set a data breakpoint on the pointer variable to track when it becomes null.");
            } else {
                actions.push_back("Enable Page Heap (gflags /p /enable <exe> /full) to catch heap corruptions earlier.");
                actions.push_back("Run with Application Verifier to detect memory errors at the point of corruption.");
            }
        } else if (analysis.exceptionCode == 0xC00000FD) {
            actions.push_back("Check recursion base case — is the termination condition reachable?");
            actions.push_back("Increase stack size with /STACK linker option if recursion depth is legitimate.");
            actions.push_back("Consider converting recursive algorithm to iterative.");
        } else if (analysis.exceptionCode == 0xC0000409) {
            actions.push_back("Review all buffer operations in the flagged function for bounds checking.");
            actions.push_back("Enable Address Sanitizer (/fsanitize=address) for precise buffer overrun detection.");
        } else if (analysis.exceptionCode == 0xC0000374) {
            actions.push_back("Enable Page Heap for precise heap corruption localization.");
            actions.push_back("Review recent malloc/free sequences for double-free or mismatched allocator usage.");
        }

        if (!frames.empty() && frames[0].function.find("!") != std::string::npos) {
            actions.push_back("Set a breakpoint at `" + frames[0].function +
                              "` and step through to observe the failing path.");
        }

        return actions;
    }

// ---- Stack Formatting for AI ----
static std::string FormatStackForAI(const std::vector<Debugger::NativeStackFrame>& frames) {
        std::ostringstream out;
        for (size_t i = 0; i < frames.size() && i < 32; i++) {
            char buf[20];
            snprintf(buf, sizeof(buf), "#%-3zu 0x%016llx", i,
                     (unsigned long long)frames[i].instructionPtr);
            out << buf;
            if (!frames[i].function.empty()) {
                out << " " << frames[i].function;
            }
            if (!frames[i].sourceFile.empty()) {
                out << " [" << frames[i].sourceFile << ":"
                    << frames[i].sourceLine << "]";
            }
            out << "\n";
        }
        return out.str();
    }

// ---- Register Formatting for AI ----
static std::string FormatRegistersForAI(const Debugger::RegisterSnapshot& regs) {
        std::ostringstream out;
        char buf[256];

        snprintf(buf, sizeof(buf),
                 "RAX=%016llx RBX=%016llx RCX=%016llx RDX=%016llx\n"
                 "RSI=%016llx RDI=%016llx RBP=%016llx RSP=%016llx\n"
                 "R8 =%016llx R9 =%016llx R10=%016llx R11=%016llx\n"
                 "R12=%016llx R13=%016llx R14=%016llx R15=%016llx\n"
                 "RIP=%016llx RFLAGS=%016llx\n",
                 (unsigned long long)regs.rax, (unsigned long long)regs.rbx,
                 (unsigned long long)regs.rcx, (unsigned long long)regs.rdx,
                 (unsigned long long)regs.rsi, (unsigned long long)regs.rdi,
                 (unsigned long long)regs.rbp, (unsigned long long)regs.rsp,
                 (unsigned long long)regs.r8, (unsigned long long)regs.r9,
                 (unsigned long long)regs.r10, (unsigned long long)regs.r11,
                 (unsigned long long)regs.r12, (unsigned long long)regs.r13,
                 (unsigned long long)regs.r14, (unsigned long long)regs.r15,
                 (unsigned long long)regs.rip, (unsigned long long)regs.rflags);
        out << buf;

        return out.str();
    }

// ---- Memory Interpretation ----
static std::string InterpretMemoryContents(const std::vector<uint8_t>& data,
                                         uint64_t baseAddr,
                                         const AIDebugAgent::MemoryAnalysis& context) {
        if (data.empty()) return "No data available.";

        std::ostringstream out;

        // Check for fill patterns
        bool allSame = true;
        for (size_t i = 1; i < data.size(); i++) {
            if (data[i] != data[0]) { allSame = false; break; }
        }

        if (allSame && data.size() >= 8) {
            if (data[0] == 0xCC) out << "Uninitialized stack (0xCC fill — MSVC debug).\n";
            else if (data[0] == 0xCD) out << "Uninitialized heap (0xCD fill — MSVC debug).\n";
            else if (data[0] == 0xDD) out << "Freed heap memory (0xDD fill — MSVC debug).\n";
            else if (data[0] == 0xFE) out << "Freed heap memory (0xFEEE fill — Win32 HeapFree).\n";
            else if (data[0] == 0x00) out << "Zero-filled memory (unallocated or zeroed).\n";
            else out << "Uniform fill: 0x" << std::hex << (int)data[0] << std::dec << ".\n";
            return out.str();
        }

        // Check for string content
        int printable = 0;
        for (auto b : data) {
            if (b >= 0x20 && b < 0x7F) printable++;
        }
        if (data.size() >= 4 && (float)printable / (float)data.size() > 0.75f) {
            out << "ASCII text content detected: \"";
            for (size_t i = 0; i < std::min(data.size(), (size_t)80); i++) {
                if (data[i] >= 0x20 && data[i] < 0x7F) out << (char)data[i];
                else out << '.';
            }
            out << "\"\n";
        }

        // Check for pointer-like values (8-byte aligned 64-bit values in reasonable range)
        if (data.size() >= 8) {
            int pointerCount = 0;
            for (size_t i = 0; i + 7 < data.size(); i += 8) {
                uint64_t val = 0;
                memcpy(&val, &data[i], 8);
                if (val > 0x10000 && val < 0x7FFFFFFFFFFF) pointerCount++;
            }
            if (pointerCount > 0) {
                out << "Contains " << pointerCount << " potential pointer value(s).\n";
            }
        }

        if (context.isCodeRegion) out << "Region contains executable code.\n";
        if (context.isStackRegion) out << "Region is on the stack.\n";
        if (context.isHeapRegion) out << "Region is in the heap.\n";

        return out.str();
    }

// ---- Instruction Annotation ----
static std::string AnnotateInstruction(const Debugger::DisassembledInstruction& insn,
                                     Debugger::NativeDebuggerEngine& engine) {
        // Annotate call targets
        if (insn.mnemonic == "call" || insn.mnemonic == "jmp") {
            // Try to resolve as address
            uint64_t targetAddr = 0;
            if (insn.operands.size() > 2 && insn.operands.substr(0, 2) == "0x") {
                targetAddr = strtoull(insn.operands.c_str(), nullptr, 16);
            }
            if (targetAddr > 0) {
                std::string sym;
                uint64_t disp = 0;
                if (engine.resolveSymbol(targetAddr, sym, disp).success) {
                    return sym;
                }
            }
        }

        // Annotate ret instructions
        if (insn.mnemonic == "ret") {
            return "function return";
        }

        // Annotate INT3
        if (insn.mnemonic == "int3" || insn.mnemonic == "int" ) {
            return "breakpoint / debug trap";
        }

        return {};
    }

// ---- Debug State to String ----
static const char* DebugStateToString(Debugger::DebugSessionState state) {
        switch (state) {
            case Debugger::DebugSessionState::Idle:        return "Idle";
            case Debugger::DebugSessionState::Attaching:   return "Attaching";
            case Debugger::DebugSessionState::Launching:   return "Launching";
            case Debugger::DebugSessionState::Running:     return "Running";
            case Debugger::DebugSessionState::Broken:      return "Broken (paused)";
            case Debugger::DebugSessionState::Stepping:    return "Stepping";
            case Debugger::DebugSessionState::Detaching:   return "Detaching";
            case Debugger::DebugSessionState::Terminated:  return "Terminated";
            case Debugger::DebugSessionState::Error:       return "Error";
            default: return "Unknown";
        }
    }
// ---- Sovereign Inference Integration ----
void AIDebugAgent::SetInferenceClient(
    std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> client) {
    m_inferenceClient = std::move(client);
}

bool AIDebugAgent::HasInferenceClient() const {
    return m_inferenceClient != nullptr && m_inferenceClient->IsLoaded();
}

std::string AIDebugAgent::GenerateAIHypothesis(const ExceptionAnalysis& analysis) {
    if (!HasInferenceClient()) {
        return GenerateHypothesis(analysis, {}, {});  // Fallback to static rules
    }

    std::string prompt = FormatAnalysisForLLM(analysis);
    prompt += "\n\nBased on this analysis, provide a concise hypothesis (1-2 sentences) about the root cause.";

    std::vector<RawrXD::Agent::ChatMessage> messages;
    messages.push_back({"system", "You are an expert debugger. Analyze crash reports and provide root cause hypotheses."});
    messages.push_back({"user", prompt});

    auto res = m_inferenceClient->ChatSync(messages);
    if (res.success && !res.response.empty()) {
        return res.response;
    }
    return GenerateHypothesis(analysis, {}, {});  // Fallback
}

std::vector<std::string> AIDebugAgent::GenerateAISuggestedActions(const ExceptionAnalysis& analysis) {
    if (!HasInferenceClient()) {
        return GenerateSuggestedActions(analysis, {});  // Fallback to static rules
    }

    std::string prompt = FormatAnalysisForLLM(analysis);
    prompt += "\n\nList 3-5 specific actionable steps to diagnose or fix this issue.";

    std::vector<RawrXD::Agent::ChatMessage> messages;
    messages.push_back({"system", "You are an expert debugger. Suggest concrete debugging steps."});
    messages.push_back({"user", prompt});

    auto res = m_inferenceClient->ChatSync(messages);
    if (res.success && !res.response.empty()) {
        std::vector<std::string> actions;
        std::istringstream iss(res.response);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && (line[0] == '-' || line[0] == '*')) {
                actions.push_back(line.substr(1));
            }
        }
        if (!actions.empty()) return actions;
    }
    return GenerateSuggestedActions(analysis, {});  // Fallback
}

std::vector<AIDebugAgent::BreakpointSuggestion> AIDebugAgent::GenerateAIBreakpointSuggestions(
    const std::string& context) {
    std::vector<BreakpointSuggestion> suggestions;

    if (!HasInferenceClient()) {
        return suggestions;  // No AI available
    }

    std::string prompt = "Code context:\n" + context +
        "\n\nSuggest strategic breakpoints for debugging this code. "
        "List function names and why they are important.";

    std::vector<RawrXD::Agent::ChatMessage> messages;
    messages.push_back({"system", "You are an expert debugger. Suggest strategic breakpoints."});
    messages.push_back({"user", prompt});

    auto res = m_inferenceClient->ChatSync(messages);
    if (res.success && !res.response.empty()) {
        std::istringstream iss(res.response);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && (line[0] == '-' || line[0] == '*')) {
                BreakpointSuggestion bp;
                bp.symbol = line.substr(1);
                bp.reason = "AI-suggested strategic breakpoint";
                bp.type = Debugger::BreakpointType::Software;
                suggestions.push_back(bp);
            }
        }
    }
    return suggestions;
}

#pragma optimize("", on)

} // namespace Debug
} // namespace RawrXD
