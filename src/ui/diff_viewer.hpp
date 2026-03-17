#pragma once
#include <string>
#include <vector>

namespace rawrxd::ui {

struct DiffHunk {
    uint32_t old_start;
    uint32_t old_lines_count; // renamed for clarity
    uint32_t new_start;
    uint32_t new_lines_count;
    std::vector<std::string> old_lines;
    std::vector<std::string> new_lines;
    bool is_addition;
    bool is_deletion;
    bool is_modification;
};

class DiffViewer {
public:
    static DiffViewer& instance() {
        static DiffViewer instance;
        return instance;
    }
    
    // Generate unified diff format
    std::string generateDiff(const std::string& old_content,
                            const std::string& new_content,
                            const std::string& filename = "file");
    
    // Parse diff for rendering
    std::vector<DiffHunk> parseDiff(const std::string& diff_text);
    
    // Render to HTML for WebView2
    std::string renderToHTML(const std::vector<DiffHunk>& hunks);
    
    // Show modal diff dialog
    bool showDiffModal(const std::string& filename,
                      const std::string& original,
                      const std::string& modified);
    
    // Check if user accepted
    bool wasAccepted() const;
    
private:
    bool last_accepted_ = false;
};

} // namespace rawrxd::ui
