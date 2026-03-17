#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace rawrxd::ui {

struct WarpSymbol {
    std::string name;
    uint32_t line;
    uint32_t column;
    std::string type; // function, class, etc.
};

struct WarpSession {
    std::string id;
    std::string current_file;
    uint32_t total_lines;
    std::vector<WarpSymbol> symbols;
};

class WarpHUD {
public:
    static WarpHUD& instance() {
        static WarpHUD instance;
        return instance;
    }

    // Toggle the high-speed symbol/file HUD (Quick Fix/Search)
    void toggleHUD(bool show = true);

    // Fast symbol indexing for current session
    void indexCurrentFile(const std::string& path, 
                         const std::string& content);

    // Fuzzy search for symbol/file targeting
    std::vector<WarpSymbol> fuzzySearch(const std::string& query);

    // Jump logic to integrated editor
    void jumpToSymbol(const WarpSymbol& symbol);

    // Command Palette integration
    void triggerCommand(const std::string& command_id, 
                        const std::string& context_json);

    // Rendering via WebView2Bridge
    void renderToUI();

private:
    WarpSession session;
    std::vector<WarpSymbol> results;
    bool is_visible = false;
};

} // namespace rawrxd::ui
