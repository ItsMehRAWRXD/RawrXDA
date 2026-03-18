#include "Win32IDE.h"
#include "context/context_mention_parser.h"
#include <windows.h>
#include <string>

// Handler for context mention parser feature
void HandleContextMentionParser(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    RawrXD::Context::ContextMentionParser parser;
    std::string testText = "Check @user and @context for details";
    auto parseResult = parser.Parse(testText);

    std::string msg = "Context Mention Parser Demo\n";
    msg += "Input: " + testText + "\n";
    msg += "Parsed mentions: " + std::to_string(parseResult.mentions.size()) + "\n";

    MessageBoxA(NULL, msg.c_str(), "Context Mention Parser", MB_ICONINFORMATION | MB_OK);
}