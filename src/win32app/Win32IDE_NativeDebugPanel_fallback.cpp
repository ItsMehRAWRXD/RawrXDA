#if defined(_WIN32) && !defined(_MSC_VER)

#include "Win32IDE.h"

namespace {

void emitNativeDebugUnavailable(Win32IDE* ide, const char* commandName) {
    std::string msg = "[Debug] ";
    msg += commandName;
    msg += " is unavailable on this toolchain (MinGW build lane).\n";
    ide->appendToOutput(msg);
}

}  // namespace

void Win32IDE::initPhase12() {
    m_phase12Initialized = false;
    appendToOutput("[Debug] Native debugger core is disabled for MinGW builds.\n");
}

void Win32IDE::shutdownPhase12() {
    m_phase12Initialized = false;
}

void Win32IDE::cmdDbgLaunch() { emitNativeDebugUnavailable(this, "launch"); }
void Win32IDE::cmdDbgAttach() { emitNativeDebugUnavailable(this, "attach"); }
void Win32IDE::cmdDbgDetach() { emitNativeDebugUnavailable(this, "detach"); }
void Win32IDE::cmdDbgGo() { emitNativeDebugUnavailable(this, "go"); }
void Win32IDE::cmdDbgStepOver() { emitNativeDebugUnavailable(this, "step-over"); }
void Win32IDE::cmdDbgStepInto() { emitNativeDebugUnavailable(this, "step-into"); }
void Win32IDE::cmdDbgStepOut() { emitNativeDebugUnavailable(this, "step-out"); }
void Win32IDE::cmdDbgBreak() { emitNativeDebugUnavailable(this, "break"); }
void Win32IDE::cmdDbgKill() { emitNativeDebugUnavailable(this, "kill"); }
void Win32IDE::cmdDbgAddBP() { emitNativeDebugUnavailable(this, "add-breakpoint"); }
void Win32IDE::cmdDbgRemoveBP() { emitNativeDebugUnavailable(this, "remove-breakpoint"); }
void Win32IDE::cmdDbgEnableBP() { emitNativeDebugUnavailable(this, "enable-breakpoint"); }
void Win32IDE::cmdDbgClearBPs() { emitNativeDebugUnavailable(this, "clear-breakpoints"); }
void Win32IDE::cmdDbgListBPs() { emitNativeDebugUnavailable(this, "list-breakpoints"); }
void Win32IDE::cmdDbgAddWatch() { emitNativeDebugUnavailable(this, "add-watch"); }
void Win32IDE::cmdDbgRemoveWatch() { emitNativeDebugUnavailable(this, "remove-watch"); }
void Win32IDE::cmdDbgRegisters() { emitNativeDebugUnavailable(this, "registers"); }
void Win32IDE::cmdDbgStack() { emitNativeDebugUnavailable(this, "stack"); }
void Win32IDE::cmdDbgMemory() { emitNativeDebugUnavailable(this, "memory"); }
void Win32IDE::cmdDbgDisasm() { emitNativeDebugUnavailable(this, "disasm"); }
void Win32IDE::cmdDbgModules() { emitNativeDebugUnavailable(this, "modules"); }
void Win32IDE::cmdDbgThreads() { emitNativeDebugUnavailable(this, "threads"); }
void Win32IDE::cmdDbgSwitchThread() { emitNativeDebugUnavailable(this, "switch-thread"); }
void Win32IDE::cmdDbgEvaluate() { emitNativeDebugUnavailable(this, "evaluate"); }
void Win32IDE::cmdDbgSetRegister() { emitNativeDebugUnavailable(this, "set-register"); }
void Win32IDE::cmdDbgSearchMemory() { emitNativeDebugUnavailable(this, "search-memory"); }
void Win32IDE::cmdDbgSymbolPath() { emitNativeDebugUnavailable(this, "symbol-path"); }
void Win32IDE::cmdDbgStatus() { emitNativeDebugUnavailable(this, "status"); }

#endif
