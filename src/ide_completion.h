#pragma once

#include "ollama_integration.h"
#include <windows.h>
#include <string>
#include <functional>

namespace IDECompletion {

// Completion popup context
struct PopupContext {
    HWND hParentWnd;
    int x, y;                      // Screen coordinates
    std::string current_line;      // Text before cursor
    std::string model;             // Selected Ollama model
    std::function<void(const std::string&)> on_select; // Callback when user accepts
};

// Initialize IDE completion system
void InitializeCompletionEngine(const std::string& default_model = "codellama:7b");

// Request completions for current editor position
// This triggers async query to Ollama
void RequestCompletion(const PopupContext& ctx);

// Cancel pending completion request
void CancelCompletion();

// Show completion popup
void ShowCompletionPopup(const PopupContext& ctx, const std::string& suggestion);

// Hide completion popup
void HideCompletionPopup();

// Set model to use for completions
void SetCompletionModel(const std::string& model);

// Get status of Ollama connection
bool IsCompletionEngineReady();

} // namespace IDECompletion
