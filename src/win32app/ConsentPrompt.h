#pragma once

#include <windows.h>
#include <string>

bool ShowConsentPrompt(HWND owner, const std::string& message);
