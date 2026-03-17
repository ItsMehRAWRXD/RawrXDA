// Paste this into Win32IDE::Initialize() or WinMain
#include "RawrXD_Integrated.h"

// Replace existing initialization with:
RawrXD::IntegratedIDE ide;
ide.Initialize();

// If you have existing AgentOrchestrator instance:
auto* orchestrator = RawrXD::GlobalContextExpanded::Get().GetAgentOrchestrator();
ide.InjectAgenticBridge(orchestrator);

// Show window
ide.Show(SW_SHOWMAXIMIZED);
ide.RunMessageLoop();
