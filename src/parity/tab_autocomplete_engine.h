// tab_autocomplete_engine.h - Multi-line ghost-text tab-autocomplete predictor
// Feature 2/15 (Cursor parity).
#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>

namespace rawrxd::parity {

struct TabCompletionRequest {
    std::string file_path;
    std::string language;      // e.g. "cpp", "py"
    std::string prefix;        // text up to the cursor
    std::string suffix;        // text after the cursor (may be empty)
    std::uint32_t cursor_line{0};
    std::uint32_t cursor_col{0};
    std::uint32_t max_lines{8};
    std::uint32_t max_chars{512};
};

struct TabCompletion {
    std::string text;                       // raw inserted text (may span multiple lines)
    std::uint32_t lines{0};                 // newline count + 1 if non-empty
    std::uint64_t request_epoch_ms{0};      // stamped on creation
    bool accepted{false};                   // toggled when user accepts
    std::string source{"heuristic"};        // heuristic/llm/cache
};

using TabModelFn = std::function<std::optional<std::string>(const TabCompletionRequest&)>;

class TabAutocompleteEngine {
public:
    // Debounce keystrokes so we don't hammer the model on every char.
    void set_debounce(std::chrono::milliseconds d);

    // Model callback (sync). Engine handles cache + debounce + cancellation.
    void set_model(TabModelFn fn);

    // Request a completion. Returns the best currently-known suggestion.
    // `out_request_id` is set to the monotonic id; callers may ignore older ids.
    std::optional<TabCompletion> request(const TabCompletionRequest& req,
                                         std::uint64_t* out_request_id = nullptr);

    // Accept the active suggestion. Returns the accepted completion (if any).
    std::optional<TabCompletion> accept_active();

    // Reject/clear the active suggestion.
    void dismiss_active();

    // Stats.
    std::uint64_t total_requests() const;
    std::uint64_t cache_hits() const;
    std::uint64_t acceptances() const;

private:
    std::string cache_key(const TabCompletionRequest& r) const;
    std::optional<std::string> heuristic(const TabCompletionRequest& r) const;

    mutable std::mutex mu_;
    TabModelFn                                       model_;
    std::chrono::milliseconds                        debounce_{60};
    std::chrono::steady_clock::time_point            last_fire_{};
    std::uint64_t                                    next_id_{1};
    std::uint64_t                                    total_{0};
    std::uint64_t                                    hits_{0};
    std::uint64_t                                    accepted_{0};
    std::optional<TabCompletion>                     active_;
    // Tiny LRU — keeps hot prefix keys only.
    static constexpr std::size_t kCacheCap = 32;
    std::vector<std::pair<std::string, TabCompletion>> cache_;
};

} // namespace rawrxd::parity
