// commit_message_generator.h - Conventional-Commit message generator
// Feature 14/15 (Copilot parity).
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rawrxd::parity {

struct CommitGenInput {
    std::vector<std::string> changed_paths;
    std::string unified_diff;       // may be empty
    std::string user_hint;          // optional natural-language hint
    bool breaking{false};
};

struct CommitGenOutput {
    std::string type;               // feat / fix / chore / refactor / test / docs / perf / build / ci / style
    std::string scope;              // inferred from paths
    std::string subject;            // <=72 chars
    std::string body;               // may be empty
    std::string full;               // rendered: "type(scope): subject\n\nbody\n"
};

class CommitMessageGenerator {
public:
    CommitGenOutput generate(const CommitGenInput& in) const;

private:
    static std::string infer_type(const CommitGenInput& in);
    static std::string infer_scope(const CommitGenInput& in);
    static std::string infer_subject(const CommitGenInput& in, std::string_view type);
};

} // namespace rawrxd::parity
