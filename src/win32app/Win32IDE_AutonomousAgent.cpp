#include "Win32IDE.h"
#include "AutonomousAgent.h"
#include <windows.h>

// Handler for Autonomous Agent feature
void HandleAutonomousAgent(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    AutonomousAgent::Initialize();
    AutonomousAgent* agent = AutonomousAgent::Instance();
    if (!agent) {
        MessageBoxA(NULL, "Failed to initialize Autonomous Agent", "Autonomous Agent", MB_ICONERROR | MB_OK);
        return;
    }
    if (!agent->IsRunning()) {
        agent->SetIDEWindow(ide->getMainWindow());
        agent->SetIDEProcessId(GetCurrentProcessId());
        if (!agent->Start()) {
            MessageBoxA(NULL, "Failed to start Autonomous Agent", "Autonomous Agent", MB_ICONERROR | MB_OK);
            return;
        }
    }

    // Show agent status
    std::string status = "Autonomous Agent Active\n\n";
    status += "Capabilities:\n";
    status += "- Autonomous decision making\n";
    status += "- Self-directed task execution\n";
    status += "- Adaptive learning\n";
    status += "- Goal-oriented behavior\n";
    status += "- Context awareness\n";

    MessageBoxA(NULL, status.c_str(), "Autonomous Agent", MB_ICONINFORMATION | MB_OK);
}
