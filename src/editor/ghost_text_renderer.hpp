#pragma once
#include <string>
#include <vector>
#include <functional>

namespace rawrxd::editor {

struct GhostSuggestion {
    std::string text;
    uint32_t start_line;
    uint32_t start_column;
    uint32_t end_line;
    uint32_t end_column;
    float confidence;
    std::string model_source; // "120B-local" or "800B-swarm"
};

class GhostTextRenderer {
public:
    using SuggestionCallback = std::function<void(const std::vector<GhostSuggestion>&)>;
    
    static GhostTextRenderer& instance();
    
    // Initialize with native edit control or WebView2 Monaco
    void initialize(void* hwnd_editor);
    
    // Real-time completion request
    void requestCompletion(const std::string& current_line, 
                          uint32_t cursor_line,
                          uint32_t cursor_col);
    
    // Render ghost text (gray, italic, after cursor)
    void renderSuggestion(const GhostSuggestion& suggestion);
    
    // Accept/reject
    void acceptSuggestion();
    void rejectSuggestion();
    void nextSuggestion(); // Cycle if multiple
    
    // Check if ghost text active
    bool hasActiveSuggestion() const;
    
private:
    void* editor_hwnd_ = nullptr;
    GhostSuggestion active_suggestion_;
    std::vector<GhostSuggestion> suggestion_queue_;
    
    // MASM64 fast-path for suggestion ranking
    extern "C" void rawrxd_rank_suggestions_asm(const char* context, 
                                                 const char* candidates,
                                                 float* scores,
                                                 uint32_t count);
};

} // namespace rawrxd::editor
