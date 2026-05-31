// tab_autocomplete_engine.cpp - Implementation
#include "tab_autocomplete_engine.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace rawrxd::parity {

namespace {

std::uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::size_t count_lines(std::string_view s) {
    if (s.empty()) return 0;
    return static_cast<std::size_t>(
               std::count(s.begin(), s.end(), '\n')) + 1;
}

std::string indent_of_last_line(std::string_view prefix) {
    auto nl = prefix.find_last_of('\n');
    std::string_view line = (nl == std::string_view::npos) ? prefix : prefix.substr(nl + 1);
    std::string ind;
    for (char c : line) {
        if (c == ' ' || c == '\t') ind.push_back(c);
        else break;
    }
    return ind;
}

} // namespace

void TabAutocompleteEngine::set_debounce(std::chrono::milliseconds d) {
    std::lock_guard lk(mu_);
    debounce_ = d;
}

void TabAutocompleteEngine::set_model(TabModelFn fn) {
    std::lock_guard lk(mu_);
    model_ = std::move(fn);
}

std::string TabAutocompleteEngine::cache_key(const TabCompletionRequest& r) const {
    // Last ~128 chars of prefix + first 32 of suffix + language.
    constexpr std::size_t kPre = 128, kSuf = 32;
    std::string key;
    key.reserve(kPre + kSuf + r.language.size() + 8);
    key.append(r.language).push_back('|');
    const auto& p = r.prefix;
    if (p.size() > kPre) key.append(p, p.size() - kPre, kPre);
    else key.append(p);
    key.push_back('\x1e');
    key.append(r.suffix.substr(0, std::min<std::size_t>(kSuf, r.suffix.size())));
    return key;
}

std::optional<std::string> TabAutocompleteEngine::heuristic(const TabCompletionRequest& r) const {
    // Deterministic language-aware fallback when no model is wired.
    if (r.prefix.empty()) return std::nullopt;
    std::string_view p = r.prefix;
    char last = p.back();

    // Close unmatched brace/paren on new line.
    if (last == '{') {
        std::string ind = indent_of_last_line(p);
        std::string body = "\n" + ind + "    \n" + ind + "}";
        return body;
    }
    if (last == '(') {
        // Suggest closing paren only if suffix doesn't already have one on the same line.
        auto nl = r.suffix.find('\n');
        std::string_view head = (nl == std::string_view::npos) ? r.suffix : r.suffix.substr(0, nl);
        if (head.find(')') == std::string_view::npos) return std::string(")");
    }
    // End-of-statement for semicolon languages.
    if ((r.language == "cpp" || r.language == "c" || r.language == "cs" ||
         r.language == "java" || r.language == "rs" || r.language == "js" || r.language == "ts") &&
        last != ';' && last != '\n' && last != '{' && last != '}') {
        // Only when the line already looks like a statement (has `=` or function call).
        auto nl = p.find_last_of('\n');
        std::string_view line = (nl == std::string_view::npos) ? p : p.substr(nl + 1);
        if (line.find('=') != std::string_view::npos || line.find('(') != std::string_view::npos)
            return std::string(";");
    }
    return std::nullopt;
}

std::optional<TabCompletion> TabAutocompleteEngine::request(
    const TabCompletionRequest& req, std::uint64_t* out_request_id) {
    std::unique_lock lk(mu_);
    ++total_;
    std::uint64_t id = next_id_++;
    if (out_request_id) *out_request_id = id;

    // Debounce — if a fire happened very recently, return cached/heuristic only.
    auto now = std::chrono::steady_clock::now();
    bool allow_model = (now - last_fire_) >= debounce_;
    last_fire_ = now;

    std::string key = cache_key(req);
    for (auto& kv : cache_) {
        if (kv.first == key) {
            ++hits_;
            active_ = kv.second;
            return active_;
        }
    }

    TabModelFn m = model_;
    lk.unlock();

    std::optional<std::string> text;
    std::string src = "heuristic";
    if (allow_model && m) {
        text = m(req);
        if (text) src = "llm";
    }
    if (!text) text = heuristic(req);
    if (!text) {
        std::lock_guard lk2(mu_);
        active_.reset();
        return std::nullopt;
    }

    // Enforce length caps.
    std::string out = std::move(*text);
    if (out.size() > req.max_chars) out.resize(req.max_chars);
    if (req.max_lines > 0) {
        std::size_t lines = 0, cut = out.size();
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (out[i] == '\n') {
                if (++lines >= req.max_lines) { cut = i; break; }
            }
        }
        if (cut < out.size()) out.resize(cut);
    }

    TabCompletion tc;
    tc.text = std::move(out);
    tc.lines = static_cast<std::uint32_t>(count_lines(tc.text));
    tc.request_epoch_ms = now_ms();
    tc.source = src;

    std::lock_guard lk2(mu_);
    if (cache_.size() >= kCacheCap) cache_.erase(cache_.begin());
    cache_.emplace_back(std::move(key), tc);
    active_ = tc;
    return tc;
}

std::optional<TabCompletion> TabAutocompleteEngine::accept_active() {
    std::lock_guard lk(mu_);
    if (!active_) return std::nullopt;
    active_->accepted = true;
    ++accepted_;
    auto snap = *active_;
    active_.reset();
    return snap;
}

void TabAutocompleteEngine::dismiss_active() {
    std::lock_guard lk(mu_);
    active_.reset();
}

std::uint64_t TabAutocompleteEngine::total_requests() const {
    std::lock_guard lk(mu_); return total_;
}
std::uint64_t TabAutocompleteEngine::cache_hits() const {
    std::lock_guard lk(mu_); return hits_;
}
std::uint64_t TabAutocompleteEngine::acceptances() const {
    std::lock_guard lk(mu_); return accepted_;
}

} // namespace rawrxd::parity
