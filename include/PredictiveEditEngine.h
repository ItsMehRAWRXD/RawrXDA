#pragma once
#include <string>
#include <vector>
#include <memory>
#include "CommonTypes.h"

namespace RawrXD {
namespace IDE {

struct PredictionResult {
    std::string suggestedEdit;
    float confidence;
    int lineOffset;
    std::string rationale;
};

class PredictiveEditEngine {
public:
    PredictiveEditEngine();
    
    // Analyze current buffer and cursor to predict the next change
    PredictionResult predictNextEdit(
        const std::string& buffer,
        int cursorLine,
        int cursorCol,
        const std::string& language);

private:
    // Uses local MiniLM/LLM to pattern match against recent edits
    std::string captureContextWindow(const std::string& buffer, int line, int col);
};

} // namespace IDE
} // namespace RawrXD
