// pr_description_generator.cpp - Implementation
#include "pr_description_generator.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <unordered_map>

namespace rawrxd::parity {

namespace {

std::string trim(std::string s) {
    auto ns = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), ns));
    s.erase(std::find_if(s.rbegin(), s.rend(), ns).base(), s.end());
    return s;
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower((unsigned char)c));
    return s;
}

bool starts_with_any(std::string_view s, std::initializer_list<const char*> prefixes) {
    for (auto p : prefixes) {
        std::size_t n = std::strlen(p);
        if (s.size() >= n && s.compare(0, n, p) == 0) return true;
    }
    return false;
}

} // namespace

std::string PRDescriptionGenerator::derive_title(const PRGenInput& in) {
    // Prefer the first non-trivial commit subject; fall back to branch name.
    for (const auto& c : in.commit_subjects) {
        auto t = trim(c);
        if (t.empty()) continue;
        if (lower(t).find("merge branch") == 0) continue;
        if (lower(t).find("wip") == 0) continue;
        if (t.size() > 72) t = t.substr(0, 72);
        return t;
    }
    if (!in.branch.empty()) {
        std::string b = in.branch;
        std::replace(b.begin(), b.end(), '-', ' ');
        std::replace(b.begin(), b.end(), '_', ' ');
        if (!b.empty()) b[0] = static_cast<char>(std::toupper((unsigned char)b[0]));
        return b;
    }
    return "Update";
}

std::vector<std::string> PRDescriptionGenerator::derive_labels(const PRGenInput& in) {
    std::vector<std::string> labels;
    bool docs_only = !in.files.empty();
    bool tests_only = !in.files.empty();
    bool has_build = false;
    bool has_security = false;
    for (const auto& f : in.files) {
        auto p = lower(f.path);
        if (p.find(".md") == std::string::npos && p.find("docs/") == std::string::npos) docs_only = false;
        if (p.find("test") == std::string::npos && p.find("spec") == std::string::npos) tests_only = false;
        if (p.find("cmake") != std::string::npos || p == "makefile" || p.find("build") != std::string::npos) has_build = true;
        if (p.find("security") != std::string::npos || p.find("auth") != std::string::npos) has_security = true;
    }
    if (docs_only) labels.push_back("documentation");
    if (tests_only) labels.push_back("tests");
    if (has_build) labels.push_back("build");
    if (has_security) labels.push_back("security");
    // Size label.
    std::uint64_t total = 0;
    for (const auto& f : in.files) total += f.added + f.removed;
    if (total > 1000) labels.push_back("size/XL");
    else if (total > 300) labels.push_back("size/L");
    else if (total > 100) labels.push_back("size/M");
    else labels.push_back("size/S");
    return labels;
}

std::string PRDescriptionGenerator::summarize_files(const std::vector<DiffFileStat>& files) {
    if (files.empty()) return "_No file-level stats available._";
    std::ostringstream os;
    os << "| File | +Added | -Removed |\n|---|---:|---:|\n";
    std::size_t shown = 0;
    for (const auto& f : files) {
        if (shown++ >= 25) { os << "| … | … | … |\n"; break; }
        os << "| `" << f.path << (f.renamed ? " (renamed)" : "") << "` | "
           << f.added << " | " << f.removed << " |\n";
    }
    return os.str();
}

PRGenOutput PRDescriptionGenerator::generate(const PRGenInput& in) const {
    PRGenOutput out;
    out.title = derive_title(in);
    out.labels = derive_labels(in);

    std::ostringstream os;
    os << "## Summary\n";
    if (!in.commit_subjects.empty()) {
        os << "This PR aggregates " << in.commit_subjects.size()
           << " commit(s) on `" << (in.branch.empty() ? "(branch)" : in.branch)
           << "` targeting `" << (in.base_branch.empty() ? "main" : in.base_branch) << "`.\n\n";
    } else {
        os << "Changes on `" << (in.branch.empty() ? "(branch)" : in.branch) << "`.\n\n";
    }

    os << "## Changes\n";
    if (!in.commit_subjects.empty()) {
        for (const auto& c : in.commit_subjects) {
            auto t = trim(c);
            if (!t.empty()) os << "- " << t << "\n";
        }
        os << "\n";
    }

    os << "## File Stats\n" << summarize_files(in.files) << "\n";

    if (!in.unified_diff_excerpt.empty()) {
        os << "## Diff Excerpt\n```diff\n";
        std::string excerpt = in.unified_diff_excerpt;
        if (excerpt.size() > 4000) excerpt.resize(4000);
        os << excerpt;
        if (excerpt.size() == 4000) os << "\n... (truncated)";
        os << "\n```\n\n";
    }

    os << "## Checklist\n"
       << "- [ ] Tests added or updated\n"
       << "- [ ] Documentation updated\n"
       << "- [ ] CI green\n";

    if (!in.template_hint.empty()) os << "\n---\n" << in.template_hint << "\n";

    out.body = os.str();
    return out;
}

} // namespace rawrxd::parity
