// cmd_k_inline_edit.h - Ctrl/Cmd-K inline natural-language edit invocation
// Feature 3/15 (Cursor parity).
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <mutex>

namespace rawrxd::parity {

struct InlineEditRequest {
    std::string file_path;
    std::string language;
    std::string selection;       // original selected text (may be empty → insert at cursor)
    std::string surrounding;     // nearby context (clamped)
    std::string instruction;     // natural-language instruction
    std::uint32_t selection_start_line{0};
    std::uint32_t selection_end_line{0};
};

struct InlineEditResult {
    std::string new_text;        // replacement for the selection
    std::string unified_diff;    // optional preview
    bool        ok{false};
    std::string error;
    std::uint64_t latency_ms{0};
};

using InlineEditModelFn = std::function<InlineEditResult(const InlineEditRequest&)>;

class CmdKInlineEdit {
public:
    void set_model(InlineEditModelFn fn);

    InlineEditResult run(const InlineEditRequest& req);

    // Undo stack — callers can revert the last applied edit.
    void record_applied(const std::string& file, const std::string& before, const std::string& after);
    std::optional<std::pair<std::string, std::string>> pop_last_applied();

    std::size_t history_size() const;

private:
    mutable std::mutex mu_;
    InlineEditModelFn model_;
    struct Entry { std::string file, before, after; };
    std::vector<Entry> history_;
};

// Build a unified diff for preview (standalone helper).
std::string make_unified_diff(std::string_view before,
                              std::string_view after,
                              std::string_view label);

} // namespace rawrxd::parity
