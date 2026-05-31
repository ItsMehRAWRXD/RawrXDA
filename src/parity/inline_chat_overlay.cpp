// inline_chat_overlay.cpp - Implementation
#include "inline_chat_overlay.h"

namespace rawrxd::parity {

namespace {
std::uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

void InlineChatOverlay::open_at(const OverlayLayout& layout, std::string initial_text) {
    std::lock_guard lk(mu_);
    layout_ = layout;
    state_ = OverlayState::Prompt;
    prompt_ = std::move(initial_text);
    response_.clear();
}

void InlineChatOverlay::close() {
    std::lock_guard lk(mu_);
    if (state_ == OverlayState::Streaming || state_ == OverlayState::Ready) {
        OverlayTurn t; t.prompt = prompt_; t.response = response_;
        t.started_ms = stream_started_ms_; t.finished_ms = now_ms();
        history_.push_back(std::move(t));
        if (history_.size() > 64) history_.erase(history_.begin());
    }
    state_ = OverlayState::Hidden;
    prompt_.clear();
    response_.clear();
    stream_started_ms_ = 0;
}

OverlayState InlineChatOverlay::state() const { std::lock_guard lk(mu_); return state_; }
OverlayLayout InlineChatOverlay::layout() const { std::lock_guard lk(mu_); return layout_; }

void InlineChatOverlay::set_prompt(std::string s) {
    std::lock_guard lk(mu_);
    prompt_ = std::move(s);
    if (state_ == OverlayState::Hidden) state_ = OverlayState::Prompt;
}

std::string InlineChatOverlay::prompt() const { std::lock_guard lk(mu_); return prompt_; }

void InlineChatOverlay::begin_stream() {
    std::lock_guard lk(mu_);
    state_ = OverlayState::Streaming;
    response_.clear();
    stream_started_ms_ = now_ms();
}

void InlineChatOverlay::append_stream(std::string_view delta) {
    std::lock_guard lk(mu_);
    if (state_ != OverlayState::Streaming) return;
    response_.append(delta);
    if (response_.size() > (1u << 20)) response_.resize(1u << 20);
}

void InlineChatOverlay::end_stream() {
    std::lock_guard lk(mu_);
    if (state_ == OverlayState::Streaming) state_ = OverlayState::Ready;
}

void InlineChatOverlay::mark_applying() {
    std::lock_guard lk(mu_);
    state_ = OverlayState::Applying;
}

void InlineChatOverlay::mark_ready() {
    std::lock_guard lk(mu_);
    state_ = OverlayState::Ready;
}

std::vector<OverlayTurn> InlineChatOverlay::history() const {
    std::lock_guard lk(mu_); return history_;
}

std::string InlineChatOverlay::current_response() const {
    std::lock_guard lk(mu_); return response_;
}

std::uint64_t InlineChatOverlay::current_elapsed_ms() const {
    std::lock_guard lk(mu_);
    if (stream_started_ms_ == 0) return 0;
    return now_ms() - stream_started_ms_;
}

void InlineChatOverlay::reanchor(std::uint32_t line, std::uint32_t column) {
    std::lock_guard lk(mu_);
    layout_.anchor_line = line;
    layout_.anchor_column = column;
}

} // namespace rawrxd::parity
