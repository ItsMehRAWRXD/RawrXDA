// commit_message_generator.cpp - Implementation
#include "commit_message_generator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <unordered_map>

namespace rawrxd::parity {

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower((unsigned char)c));
    return s;
}

std::string trim(std::string s) {
    auto ns = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), ns));
    s.erase(std::find_if(s.rbegin(), s.rend(), ns).base(), s.end());
    return s;
}

} // namespace

std::string CommitMessageGenerator::infer_type(const CommitGenInput& in) {
    if (!in.user_hint.empty()) {
        auto h = lower(in.user_hint);
        if (h.find("fix") != std::string::npos || h.find("bug") != std::string::npos)      return "fix";
        if (h.find("add") != std::string::npos || h.find("introduce") != std::string::npos) return "feat";
        if (h.find("refactor") != std::string::npos)                                        return "refactor";
        if (h.find("doc") != std::string::npos)                                             return "docs";
        if (h.find("test") != std::string::npos)                                            return "test";
        if (h.find("perf") != std::string::npos || h.find("optimi") != std::string::npos)   return "perf";
    }
    bool tests_only = !in.changed_paths.empty();
    bool docs_only  = !in.changed_paths.empty();
    bool build_only = !in.changed_paths.empty();
    for (const auto& p : in.changed_paths) {
        auto lp = lower(p);
        if (lp.find("test") == std::string::npos && lp.find("spec") == std::string::npos) tests_only = false;
        if (lp.find(".md") == std::string::npos && lp.find("docs/") == std::string::npos) docs_only = false;
        if (lp.find("cmake") == std::string::npos && lp.find("makefile") == std::string::npos &&
            lp.find(".yml") == std::string::npos && lp.find(".yaml") == std::string::npos) build_only = false;
    }
    if (tests_only) return "test";
    if (docs_only)  return "docs";
    if (build_only) return "build";

    // Diff heuristics
    std::size_t adds = 0, dels = 0;
    for (std::size_t i = 0; i + 1 < in.unified_diff.size(); ++i) {
        if (in.unified_diff[i] == '\n') {
            char c = in.unified_diff[i + 1];
            if (c == '+' && (i + 2 >= in.unified_diff.size() || in.unified_diff[i + 2] != '+')) ++adds;
            if (c == '-' && (i + 2 >= in.unified_diff.size() || in.unified_diff[i + 2] != '-')) ++dels;
        }
    }
    if (adds > 0 && dels == 0) return "feat";
    if (dels > adds * 2)       return "refactor";
    return "chore";
}

std::string CommitMessageGenerator::infer_scope(const CommitGenInput& in) {
    if (in.changed_paths.empty()) return "";
    // Longest common top-folder path (up to 2 segments).
    std::vector<std::vector<std::string>> segs;
    segs.reserve(in.changed_paths.size());
    for (const auto& p : in.changed_paths) {
        std::filesystem::path fp(p);
        std::vector<std::string> parts;
        for (const auto& s : fp) {
            std::string v = s.string();
            if (!v.empty() && v != "/" && v != "\\") parts.push_back(v);
        }
        if (!parts.empty()) parts.pop_back(); // drop filename
        segs.push_back(std::move(parts));
    }
    if (segs.empty()) return "";
    std::vector<std::string> common;
    for (std::size_t i = 0; i < segs[0].size() && i < 2; ++i) {
        const auto& candidate = segs[0][i];
        bool ok = true;
        for (const auto& s : segs)
            if (i >= s.size() || s[i] != candidate) { ok = false; break; }
        if (!ok) break;
        common.push_back(candidate);
    }
    if (common.empty()) return "";
    std::string scope;
    for (std::size_t i = 0; i < common.size(); ++i) {
        if (i) scope.push_back('/');
        scope += common[i];
    }
    // Trim scope length for readability.
    if (scope.size() > 32) scope.resize(32);
    return scope;
}

std::string CommitMessageGenerator::infer_subject(const CommitGenInput& in, std::string_view type) {
    if (!in.user_hint.empty()) {
        auto t = trim(in.user_hint);
        if (t.size() > 72) t.resize(72);
        return t;
    }
    std::ostringstream os;
    if (type == "feat")      os << "add ";
    else if (type == "fix")  os << "fix ";
    else if (type == "docs") os << "update documentation";
    else if (type == "test") os << "update tests";
    else if (type == "refactor") os << "refactor ";
    else if (type == "perf") os << "improve performance";
    else os << "update ";
    if (type != "docs" && type != "test" && type != "perf") {
        if (!in.changed_paths.empty()) {
            std::filesystem::path fp(in.changed_paths.front());
            os << fp.stem().string();
            if (in.changed_paths.size() > 1)
                os << " and " << (in.changed_paths.size() - 1) << " other(s)";
        } else {
            os << "code";
        }
    }
    std::string s = os.str();
    if (s.size() > 72) s.resize(72);
    return s;
}

CommitGenOutput CommitMessageGenerator::generate(const CommitGenInput& in) const {
    CommitGenOutput out;
    out.type    = infer_type(in);
    out.scope   = infer_scope(in);
    out.subject = infer_subject(in, out.type);

    std::ostringstream header;
    header << out.type;
    if (!out.scope.empty()) header << "(" << out.scope << ")";
    if (in.breaking)        header << "!";
    header << ": " << out.subject;

    std::ostringstream body;
    if (!in.changed_paths.empty()) {
        body << "Changes:\n";
        std::size_t shown = 0;
        for (const auto& p : in.changed_paths) {
            if (shown++ >= 10) { body << "- ...\n"; break; }
            body << "- " << p << "\n";
        }
    }
    if (in.breaking) body << "\nBREAKING CHANGE: see body for migration notes.\n";

    out.body = body.str();
    out.full = header.str() + (out.body.empty() ? "\n" : "\n\n" + out.body);
    return out;
}

} // namespace rawrxd::parity
