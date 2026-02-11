#include "ConsentPrompt.h"

bool ShowConsentPrompt(HWND owner, const std::string& message) {
    int result = MessageBoxA(owner, message.c_str(), "Confirmation Required", MB_YESNO | MB_ICONQUESTION);
    return result == IDYES;
}
