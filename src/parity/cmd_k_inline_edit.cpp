// cmd_k_inline_edit.cpp - Implementation
#include "cmd_k_inline_edit.h"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace rawrxd::parity {

namespace {

std::vector<std::string> split_lines(std::string_view s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        auto nl = s.find('\n', start);
        if (nl == std::string_view::npos) {
            out.emplace_back(s.substr(start));
            break;
        }
        out.emplace_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

} // namespace

std::string make_unified_diff(std::string_view before,
                              std::string_view after,
                              std::string_view label) {
    // Minimal LCS-based unified diff (good enough for preview).
    auto a = split_lines(before);
    auto b = split_lines(after);
    const std::size_t n = a.size(), m = b.size();
    std::vector<std::vector<std::uint16_t>> dp(n + 1, std::vector<std::uint16_t>(m + 1, 0));
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j)
            dp[i + 1][j + 1] = (a[i] == b[j])
                                   ? static_cast<std::uint16_t>(dp[i][j] + 1)
                                   : std::max(dp[i + 1][j], dp[i][j + 1]);
    std::vector<char> ops;
    std::vector<std::string> lines;
    std::size_t i = n, j = m;
    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) { ops.push_back(' '); lines.push_back(a[i - 1]); --i; --j; }
        else if (dp[i - 1][j] >= dp[i][j - 1]) { ops.push_back('-'); lines.push_back(a[i - 1]); --i; }
        else { ops.push_back('+'); lines.push_back(b[j - 1]); --j; }
    }
    while (i > 0) { ops.push_back('-'); lines.push_back(a[--i]); }
    while (j > 0) { ops.push_back('+'); lines.push_back(b[--j]); }
    std::reverse(ops.begin(), ops.end());
    std::reverse(lines.begin(), lines.end());

    std::ostringstream os;
    os << "--- " << label << "\n+++ " << label << "\n";
    for (std::size_t k = 0; k < ops.size(); ++k) os << ops[k] << lines[k] << "\n";
    return os.str();
}

void CmdKInlineEdit::set_model(InlineEditModelFn fn) {
    std::lock_guard lk(mu_);
    model_ = std::move(fn);
}

InlineEditResult CmdKInlineEdit::run(const InlineEditRequest& req) {
    InlineEditModelFn m;
    { std::lock_guard lk(mu_); m = model_; }
    if (!m) {
        InlineEditResult r;
        r.ok = false;
        r.error = "no model attached";
        return r;
    }
    auto t0 = std::chrono::steady_clock::now();
    InlineEditResult r = m(req);
    auto t1 = std::chrono::steady_clock::now();
    r.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (r.ok && r.unified_diff.empty())
        r.unified_diff = make_unified_diff(req.selection, r.new_text, req.file_path);
    return r;
}

void CmdKInlineEdit::record_applied(const std::string& file,
                                    const std::string& before,
                                    const std::string& after) {
    std::lock_guard lk(mu_);
    history_.push_back({file, before, after});
    if (history_.size() > 64) history_.erase(history_.begin());
}

std::optional<std::pair<std::string, std::string>> CmdKInlineEdit::pop_last_applied() {
    std::lock_guard lk(mu_);
    if (history_.empty()) return std::nullopt;
    auto e = history_.back();
    history_.pop_back();
    return std::make_pair(e.file, e.before);
}

std::size_t CmdKInlineEdit::history_size() const {
    std::lock_guard lk(mu_); return history_.size();
}

} // namespace rawrxd::parity
