// cursor_rules_loader.h - Parse/enforce `.cursorrules` project directives
// Feature 1/15 (Cursor parity).
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <mutex>

namespace rawrxd::parity {

struct CursorRule {
    std::string id;            // stable short id derived from hash of body
    std::string section;       // optional section heading (e.g. "testing")
    std::string body;          // rule text (trimmed)
    int         priority{0};   // higher applied first
    bool        must{false};   // MUST/REQUIRED markers elevate to hard-constraint
};

class CursorRulesLoader {
public:
    // Loads `.cursorrules` (and legacy `.cursor/rules`) from the given root.
    // Returns number of rules loaded. Previous contents are replaced.
    std::size_t load(const std::filesystem::path& workspace_root);

    // Reload from last-used root; no-op if never loaded.
    std::size_t reload();

    // Returns rules ordered by priority desc, stable.
    std::vector<CursorRule> rules() const;

    // Joins rule bodies into a system-prompt suffix, respecting `max_chars`.
    std::string render_system_prompt(std::size_t max_chars = 4096) const;

    // True if any MUST rule contains `needle` (case-insensitive).
    bool has_hard_constraint(std::string_view needle) const;

    bool loaded() const;
    std::filesystem::path source_path() const;

private:
    std::size_t parse_buffer(std::string_view text);

    mutable std::mutex        mu_;
    std::vector<CursorRule>   rules_;
    std::filesystem::path     root_;
    std::filesystem::path     source_;
    bool                      loaded_{false};
};

} // namespace rawrxd::parity
