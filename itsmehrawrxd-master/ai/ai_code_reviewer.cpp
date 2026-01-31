// AI-powered code review system
#include <vector>
#include <string>
#include <memory>

namespace IDE_AI {

class AICodeReviewer {
public:
    AICodeReviewer(std::shared_ptr<CompletionModel> model) : model_(model) {}
    
    // Review code for quality issues
    std::vector<ReviewComment> reviewCode(const std::string& code) {
        std::vector<ReviewComment> comments;
        
        std::string review_prompt = "Review this code for quality, style, and best practices:\n" + code;
        std::string review = model_->generateCompletion(review_prompt);
        
        // Parse review results (simplified)
        if (review.find("naming convention") != std::string::npos) {
            comments.push_back({
                "Naming Convention",
                "Consider following consistent naming conventions",
                "STYLE",
                0,
                "Use camelCase for variables, PascalCase for classes"
            });
        }
        
        if (review.find("performance") != std::string::npos) {
            comments.push_back({
                "Performance",
                "Potential performance improvement",
                "OPTIMIZATION",
                0,
                "Consider optimizing this section for better performance"
            });
        }
        
        return comments;
    }
    
    // Check code style
    std::vector<StyleIssue> checkStyle(const std::string& code) {
        std::vector<StyleIssue> issues;
        
        // Simple style checks
        if (code.find("    ") != std::string::npos && code.find("\t") != std::string::npos) {
            issues.push_back({
                "Mixed Indentation",
                "Code uses both spaces and tabs for indentation",
                "Use consistent indentation (spaces or tabs)"
            });
        }
        
        return issues;
    }
    
private:
    std::shared_ptr<CompletionModel> model_;
};

struct ReviewComment {
    std::string category;
    std::string message;
    std::string type;
    int line_number;
    std::string suggestion;
};

struct StyleIssue {
    std::string type;
    std::string description;
    std::string fix;
};

} // namespace IDE_AI
