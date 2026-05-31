// cursor_rules_loader.cpp - Implementation for CursorRulesLoader
#include "cursor_rules_loader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace rawrxd::parity {

namespace {

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string to_lower(std::string_view v) {
    std::string out(v);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string short_id(std::string_view body) {
    // FNV-1a 32-bit → hex; compact, deterministic, collision-tolerant for local use.
    std::uint32_t h = 2166136261u;
    for (unsigned char c : body) {
        h ^= c;
        h *= 16777619u;
    }
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08x", h);
    return std::string(buf);
}

bool contains_must(std::string_view body) {
    std::string lo = to_lower(body);
    return lo.find("must ") != std::string::npos ||
           lo.find("required") != std::string::npos ||
           lo.find("never ") != std::string::npos ||
           lo.find("always ") != std::string::npos;
}

} // namespace

std::size_t CursorRulesLoader::load(const std::filesystem::path& workspace_root) {
    std::lock_guard lk(mu_);
    root_ = workspace_root;
    rules_.clear();
    loaded_ = false;

    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {
        workspace_root / ".cursorrules",
        workspace_root / ".cursor" / "rules",
        workspace_root / ".cursor" / "rules.md",
    };
    for (const auto& p : candidates) {
        std::error_code ec;
        if (!fs::exists(p, ec) || ec) continue;
        std::ifstream f(p, std::ios::binary);
        if (!f) continue;
        std::ostringstream ss;
        ss << f.rdbuf();
        source_ = p;
        auto n = parse_buffer(ss.str());
        loaded_ = true;
        return n;
    }
    return 0;
}

std::size_t CursorRulesLoader::reload() {
    std::filesystem::path root;
    {
        std::lock_guard lk(mu_);
        root = root_;
    }
    if (root.empty()) return 0;
    return load(root);
}

std::size_t CursorRulesLoader::parse_buffer(std::string_view text) {
    // Parses sections delimited by `## name` headings or blank lines.
    rules_.clear();
    std::string current_section;
    std::string accum;
    int         priority = 0;

    auto flush = [&]() {
        std::string body = trim(std::move(accum));
        accum.clear();
        if (body.empty()) return;
        CursorRule r;
        r.body = body;
        r.section = current_section;
        r.priority = priority--;
        r.must = contains_must(body);
        r.id = short_id(body);
        rules_.push_back(std::move(r));
    };

    std::size_t start = 0;
    while (start <= text.size()) {
        auto end = text.find('\n', start);
        std::string_view line = text.substr(
            start, end == std::string_view::npos ? text.size() - start : end - start);
        start = (end == std::string_view::npos) ? text.size() + 1 : end + 1;

        std::string t = trim(std::string(line));
        if (t.empty()) { flush(); continue; }
        if (t.rfind("##", 0) == 0) {
            flush();
            current_section = trim(t.substr(2));
            continue;
        }
        if (t.rfind("#", 0) == 0) {
            // Treat H1 as top-level section reset.
            flush();
            current_section = trim(t.substr(1));
            continue;
        }
        if (!accum.empty()) accum.push_back('\n');
        accum += t;
    }
    flush();

    std::stable_sort(rules_.begin(), rules_.end(),
                     [](const CursorRule& a, const CursorRule& b) {
                         if (a.must != b.must) return a.must && !b.must;
                         return a.priority > b.priority;
                     });
    return rules_.size();
}

std::vector<CursorRule> CursorRulesLoader::rules() const {
    std::lock_guard lk(mu_);
    return rules_;
}

std::string CursorRulesLoader::render_system_prompt(std::size_t max_chars) const {
    std::lock_guard lk(mu_);
    std::string out;
    out.reserve(std::min<std::size_t>(max_chars, 8192));
    for (const auto& r : rules_) {
        std::string prefix = r.must ? "[MUST] " : "- ";
        std::string line;
        if (!r.section.empty()) line = "(" + r.section + ") ";
        line += prefix + r.body + "\n";
        if (out.size() + line.size() > max_chars) break;
        out += line;
    }
    return out;
}

bool CursorRulesLoader::has_hard_constraint(std::string_view needle) const {
    std::string n = to_lower(needle);
    std::lock_guard lk(mu_);
    for (const auto& r : rules_) {
        if (!r.must) continue;
        if (to_lower(r.body).find(n) != std::string::npos) return true;
    }
    return false;
}

bool CursorRulesLoader::loaded() const {
    std::lock_guard lk(mu_);
    return loaded_;
}

std::filesystem::path CursorRulesLoader::source_path() const {
    std::lock_guard lk(mu_);
    return source_;
}

} // namespace rawrxd::parity
