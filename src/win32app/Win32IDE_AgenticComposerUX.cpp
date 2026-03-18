#include "Win32IDE.h"
#include "agentic/agentic_composer_ux.h"
#include <windows.h>
#include <string>

// Handler for agentic composer UX feature
void HandleAgenticComposerUX(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    // Get the global composer UX instance (assuming it's a singleton)
    static RawrXD::Agentic::AgenticComposerUX composer;
    static bool initialized = false;

    if (!initialized) {
        RawrXD::Agentic::ComposerUICallbacks callbacks;
        // Set up basic callbacks (could be expanded)
        composer.Initialize(callbacks);
        initialized = true;
    }

    // Start a demo session
    uint64_t sessionId = composer.StartSession("Demo Agentic Composition", "default-model");

    // Show session info
    auto* session = composer.GetCurrentSession();
    std::string info = "Agentic Composer UX Session Started\n";
    info += "Session ID: " + std::to_string(sessionId) + "\n";
    info += "Title: " + session->title + "\n";
    info += "Model: " + session->modelName + "\n";

    MessageBoxA(NULL, info.c_str(), "Agentic Composer UX", MB_ICONINFORMATION | MB_OK);
}