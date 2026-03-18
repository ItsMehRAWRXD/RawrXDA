#include "Win32IDE.h"
#include "IDEDiagnosticAutoHealer.h"
#include <windows.h>

// Handler for IDE Diagnostic AutoHealer feature
void HandleIDEDiagnosticAutoHealer(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    auto& healer = IDEDiagnosticAutoHealer::Instance();
    if (!healer.IsRunning()) {
        healer.StartFullDiagnostic();
    }

    // Show healer status
    std::string status = "IDE Diagnostic AutoHealer Active\n\n";
    status += "Self-healing diagnostic system:\n";
    status += "- Automatic error detection\n";
    status += "- Diagnostic report generation\n";
    status += "- Self-repair mechanisms\n";
    status += "- Performance monitoring\n";
    status += "- Health status reporting\n";

    MessageBoxA(NULL, status.c_str(), "IDE Diagnostic AutoHealer", MB_ICONINFORMATION | MB_OK);
}
