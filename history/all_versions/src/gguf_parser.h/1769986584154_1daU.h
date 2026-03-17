#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "utils/Expected.h"

// We need InferenceError definition. 
// Ideally it should be in a common header. 
// For now, let's assume it's available or we forward declare or we use a header.
// To avoid circular dependency with CPUInferenceEngine, we should probably move key types to common header.
// But as a quick fix, we can include CPUInferenceEngine.h or just redefine/use a specific error type.
// Actually, GGUFParser might just return std::expected<ParsedGGUFModel, std::string> or similar.
// But CPUInferenceEngine uses InferenceError.

// Let's rely on CPUInferenceEngine.h defining InferenceError, but we can't include it here if it includes us.
// CIRCULAR DEPENDENCY ALERT.
// CPUInferenceEngine.h includes "gguf_parser.h" (forward declared in header? no, include).
// Warning: CPUInferenceEngine.h includes nothing about GGUFParser, it forwards declares `class GGUFParser`.
// So we are safe to include CPUInferenceEngine.h IF we move GGUFParser pointer member to unique_ptr and forward declare in CPUInferenceEngine.h (already done).

// However, CPUInferenceEngine.h DEFINES InferenceError.
// So we can include "cpu_inference_engine.h" here? 
// No, cpu_inference_engine.h forwards declares GGUFParser.
// If cpu_inference_engine.h includes this file, we have a cycle.
// cpu_inference_engine.cpp includes "gguf_parser.h".
// cpu_inference_engine.h DOES NOT include "gguf_parser.h", it forwards declares.
// So we can include "cpu_inference_engine.h" here?
// Wait, cpu_inference_engine.h likely includes other things.

// SAFEST BET: Move InferenceError to a common type file or just use int/string error in Parser and let Engine map it.
// I'll stick to a simple struct in GGUFParser and let the engine handle the error type.

namespace RawrXD {

struct ParsedGGUFModel {
    size_t totalSize;
    std::vector<std::string> vocab;
    std::vector<uint8_t> weights;
    // Add more metadata as needed
    std::vector<float> token_scores;
    std::vector<uint32_t> token_types;
};

class GGUFParser {
public:
    GGUFParser() = default;
    
    // Returns ParsedGGUFModel or error message string
    Expected<ParsedGGUFModel, std::string> parse(const std::string& path);
};
}
