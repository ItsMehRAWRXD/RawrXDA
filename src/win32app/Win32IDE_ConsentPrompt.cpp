#include "Win32IDE.h"
#include "ConsentPrompt.h"
#include <windows.h>

// Handler for Consent Prompt feature
void HandleConsentPrompt(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    // Show consent status
    std::string status = "Consent Prompt Active\n\n";
    status += "User consent and privacy management:\n";
    status += "- Data collection consent\n";
    status += "- Privacy policy acknowledgment\n";
    status += "- Feature usage permissions\n";
    status += "- Telemetry opt-in/opt-out\n";
    status += "- Security policy compliance\n";

    if (!ShowConsentPrompt(ide->getMainWindow(), status)) {
        MessageBoxA(NULL, "Consent was not granted by user", "Consent Prompt", MB_ICONWARNING | MB_OK);
    }
}
