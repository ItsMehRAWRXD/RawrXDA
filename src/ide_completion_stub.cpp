#include "ide_completion.h"
#include <string>

namespace IDECompletion {

static std::string g_default_model = "codellama:7b";
static bool g_initialized = false;

void InitializeCompletionEngine(const std::string& default_model) {
    g_default_model = default_model;
    g_initialized = true;
}

void RequestCompletion(const PopupContext& ctx) {
    (void)ctx;
    /* Stub: no-op */
}

void CancelCompletion() {
    /* Stub: no-op */
}

void ShowCompletionPopup(const PopupContext& ctx, const std::string& suggestion) {
    (void)ctx;
    (void)suggestion;
    /* Stub: no-op */
}

void HideCompletionPopup() {
    /* Stub: no-op */
}

void SetCompletionModel(const std::string& model) {
    g_default_model = model;
}

bool IsCompletionEngineReady() {
    return g_initialized;
}

} // namespace IDECompletion
