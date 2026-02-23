#pragma once
// ============================================================================
// ask_mode_handler.hpp — Ask Mode: Q&A with verification and citations
// ============================================================================

#include <string>

namespace RawrXD {

// Optional system prompt for Ask mode: encourage verification and citations.
inline const char* AskModeSystemPrompt() {
    return
        "You are in Ask mode. Answer concisely. When stating facts or code, "
        "cite the source or say 'based on context' when referring to the user's code.";
}

inline std::string AskModeUserPrefix() {
    return "";
}

} // namespace RawrXD
