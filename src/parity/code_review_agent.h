// code_review_agent.h - Heuristic code review with severity-ranked findings
// Feature 15/15 (Copilot parity).
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace rawrxd::parity {

enum class ReviewSeverity { Info, Minor, Major, Critical };

struct ReviewFinding {
    std::string rule_id;
    ReviewSeverity severity{ReviewSeverity::Info};
    std::string file;
    std::uint32_t line{0};
    std::string message;
    std::string suggestion;        // optional actionable fix
};

struct ReviewFileInput {
    std::string path;
    std::string language;
    std::string content;
};

struct ReviewReport {
    std::vector<ReviewFinding> findings;
    std::string summary_markdown;
};

using ExternalReviewerFn = std::function<std::vector<ReviewFinding>(const ReviewFileInput&)>;

class CodeReviewAgent {
public:
    // Plug in an LLM/external reviewer whose findings merge into the heuristic ones.
    void set_external_reviewer(ExternalReviewerFn fn);

    ReviewReport review(const std::vector<ReviewFileInput>& files) const;

    // Heuristics applied to a single file (exposed for testing).
    static std::vector<ReviewFinding> heuristics(const ReviewFileInput& f);

private:
    static std::string render_summary(const std::vector<ReviewFinding>& findings);

    mutable std::mutex mu_;
    ExternalReviewerFn external_;
};

} // namespace rawrxd::parity
