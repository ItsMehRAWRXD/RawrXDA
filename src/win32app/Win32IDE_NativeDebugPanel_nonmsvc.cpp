#if !defined(_MSC_VER)

#include "Win32IDE.h"

#include <windows.h>

namespace {
void logDebugUnavailable(const char* action) {
    OutputDebugStringA("[RawrXD][Phase12] Native debugger unavailable on non-MSVC toolchain: ");
    OutputDebugStringA(action);
    OutputDebugStringA("\n");
}
}  // namespace

void Win32IDE::initPhase12() {
    m_phase12Initialized = false;
    logDebugUnavailable("initPhase12");
}

void Win32IDE::shutdownPhase12() {
    m_phase12Initialized = false;
    logDebugUnavailable("shutdownPhase12");
}

void Win32IDE::cmdDbgStepOver() {
    logDebugUnavailable("cmdDbgStepOver");
}

void Win32IDE::cmdDbgStepInto() {
    logDebugUnavailable("cmdDbgStepInto");
}

void Win32IDE::cmdDbgStepOut() {
    logDebugUnavailable("cmdDbgStepOut");
}

void Win32IDE::cmdDbgBreak() {
    logDebugUnavailable("cmdDbgBreak");
}

void Win32IDE::cmdDbgLaunch() {
    logDebugUnavailable("cmdDbgLaunch");
}

void Win32IDE::cmdDbgAttach() {
    logDebugUnavailable("cmdDbgAttach");
}

void Win32IDE::cmdDbgDetach() {
    logDebugUnavailable("cmdDbgDetach");
}

void Win32IDE::cmdDbgGo() {
    logDebugUnavailable("cmdDbgGo");
}

void Win32IDE::cmdDbgAddBP() {
    logDebugUnavailable("cmdDbgAddBP");
}

void Win32IDE::cmdDbgRemoveBP() {
    logDebugUnavailable("cmdDbgRemoveBP");
}

void Win32IDE::cmdDbgEnableBP() {
    logDebugUnavailable("cmdDbgEnableBP");
}

void Win32IDE::cmdDbgClearBPs() {
    logDebugUnavailable("cmdDbgClearBPs");
}

void Win32IDE::cmdDbgListBPs() {
    logDebugUnavailable("cmdDbgListBPs");
}

void Win32IDE::cmdDbgAddWatch() {
    logDebugUnavailable("cmdDbgAddWatch");
}

void Win32IDE::cmdDbgRemoveWatch() {
    logDebugUnavailable("cmdDbgRemoveWatch");
}

void Win32IDE::cmdDbgSwitchThread() {
    logDebugUnavailable("cmdDbgSwitchThread");
}

void Win32IDE::cmdDbgEvaluate() {
    logDebugUnavailable("cmdDbgEvaluate");
}

void Win32IDE::cmdDbgKill() {
    logDebugUnavailable("cmdDbgKill");
}

void Win32IDE::cmdDbgRegisters() {
    logDebugUnavailable("cmdDbgRegisters");
}

void Win32IDE::cmdDbgStack() {
    logDebugUnavailable("cmdDbgStack");
}

void Win32IDE::cmdDbgMemory() {
    logDebugUnavailable("cmdDbgMemory");
}

void Win32IDE::cmdDbgDisasm() {
    logDebugUnavailable("cmdDbgDisasm");
}

void Win32IDE::cmdDbgModules() {
    logDebugUnavailable("cmdDbgModules");
}

void Win32IDE::cmdDbgThreads() {
    logDebugUnavailable("cmdDbgThreads");
}

void Win32IDE::cmdDbgSetRegister() {
    logDebugUnavailable("cmdDbgSetRegister");
}

void Win32IDE::cmdDbgSearchMemory() {
    logDebugUnavailable("cmdDbgSearchMemory");
}

void Win32IDE::cmdDbgSymbolPath() {
    logDebugUnavailable("cmdDbgSymbolPath");
}

void Win32IDE::cmdDbgStatus() {
    logDebugUnavailable("cmdDbgStatus");
}

#endif  // !defined(_MSC_VER)
