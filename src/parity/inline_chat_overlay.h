// inline_chat_overlay.h - Inline chat overlay state/layout controller
// Feature 11/15 (Copilot parity).
#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace rawrxd::parity {

enum class OverlayState { Hidden, Prompt, Streaming, Ready, Applying };

struct OverlayLayout {
    std::int32_t x{0};            // screen/client x
    std::int32_t y{0};
    std::uint32_t width{520};
    std::uint32_t height{72};
    std::uint32_t anchor_line{0};
    std::uint32_t anchor_column{0};
};

struct OverlayTurn {
    std::string prompt;
    std::string response;
    std::uint64_t started_ms{0};
    std::uint64_t finished_ms{0};
};

class InlineChatOverlay {
public:
    void open_at(const OverlayLayout& layout, std::string initial_text = "");
    void close();

    OverlayState state() const;
    OverlayLayout layout() const;

    void set_prompt(std::string s);
    std::string prompt() const;

    void begin_stream();
    void append_stream(std::string_view delta);
    void end_stream();

    void mark_applying();
    void mark_ready();

    std::vector<OverlayTurn> history() const;
    std::string current_response() const;
    std::uint64_t current_elapsed_ms() const;

    // Re-anchor (e.g. after scroll / edits above the insertion point).
    void reanchor(std::uint32_t line, std::uint32_t column);

private:
    mutable std::mutex mu_;
    OverlayState state_{OverlayState::Hidden};
    OverlayLayout layout_{};
    std::string prompt_;
    std::string response_;
    std::uint64_t stream_started_ms_{0};
    std::vector<OverlayTurn> history_;
};

} // namespace rawrxd::parity
