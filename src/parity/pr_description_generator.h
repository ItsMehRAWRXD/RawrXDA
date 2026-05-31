// pr_description_generator.h - Generate PR title/body from diffs + context
// Feature 13/15 (Copilot parity).
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rawrxd::parity {

struct DiffFileStat {
    std::string path;
    std::uint32_t added{0};
    std::uint32_t removed{0};
    bool renamed{false};
    std::string old_path;
};

struct PRGenInput {
    std::string branch;
    std::string base_branch;
    std::vector<std::string> commit_subjects;
    std::vector<DiffFileStat> files;
    std::string unified_diff_excerpt;   // optional (clamped)
    std::string template_hint;          // repo PR template text
};

struct PRGenOutput {
    std::string title;
    std::string body;                   // markdown
    std::vector<std::string> labels;    // suggested labels
};

class PRDescriptionGenerator {
public:
    PRGenOutput generate(const PRGenInput& in) const;

private:
    static std::string derive_title(const PRGenInput& in);
    static std::vector<std::string> derive_labels(const PRGenInput& in);
    static std::string summarize_files(const std::vector<DiffFileStat>& files);
};

} // namespace rawrxd::parity
