#include "PredictiveEditEngine.h"
#include <algorithm>
#include <sstream>

namespace RawrXD {
namespace IDE {

PredictiveEditEngine::PredictiveEditEngine() {
}

PredictionResult PredictiveEditEngine::predictNextEdit(
    const std::string& buffer,
    int cursorLine,
    int cursorCol,
    const std::string& language) {
    
    PredictionResult result;
    result.confidence = 0.0f;
    result.lineOffset = 0;

    std::string context = captureContextWindow(buffer, cursorLine, cursorCol);
    
    // Pattern Matcher: Common edits after 'if (err)'
    // This would be replaced with our local LLM (Ministral 3B) query
    if (context.find("if (err)") != std::string::npos || context.find("if (error)") != std::string::npos) {
        result.suggestedEdit = "    return err;";
        result.rationale = "Common guard clause pattern detected.";
        result.confidence = 0.95f;
        result.lineOffset = 1;
    } else if (context.find("void ") != std::string::npos && context.find("{") != std::string::npos && context.find("}") == std::string::npos) {
        // Generate language-specific function body based on detected language
        if (language == "cpp" || language == "c++" || language == "c") {
            result.suggestedEdit = "    // TODO: Implement function logic\n    return 0;";
        } else if (language == "python") {
            result.suggestedEdit = "    # TODO: Implement function logic\n    pass";
        } else if (language == "javascript" || language == "typescript" || language == "js" || language == "ts") {
            result.suggestedEdit = "    // TODO: Implement function logic\n    return null;";
        } else if (language == "rust") {
            result.suggestedEdit = "    // TODO: Implement function logic\n    todo!()";
        } else if (language == "go" || language == "golang") {
            result.suggestedEdit = "    // TODO: Implement function logic\n    return nil, nil";
        } else if (language == "java") {
            result.suggestedEdit = "    // TODO: Implement function logic\n    return null;";
        } else {
            result.suggestedEdit = "    // TODO: Implement " + language + " function logic";
        }
        result.rationale = "New function skeleton detected.";
        result.confidence = 0.70f;
        result.lineOffset = 1;
    }

    return result;
}

std::string PredictiveEditEngine::captureContextWindow(const std::string& buffer, int line, int col) {
    // Captures last 500 characters or current function body for analysis
    size_t startPos = (buffer.length() > 500) ? buffer.length() - 500 : 0;
    return buffer.substr(startPos);
}

} // namespace IDE
} // namespace RawrXD
