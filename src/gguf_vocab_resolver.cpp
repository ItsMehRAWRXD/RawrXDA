#include "gguf_vocab_resolver.h"
#include <iostream>

GGUFVocabResolver::GGUFVocabResolver() {
    setupVocabMappings();
}

void GGUFVocabResolver::setupVocabMappings() {
    expectedVocabSizes["llama"] = 32000;
    expectedVocabSizes["mistral"] = 32000;
    expectedVocabSizes["qwen"] = 151936;
}

VocabSizeDetection GGUFVocabResolver::detectVocabSize(const std::map<std::string, std::string>& metadata, const std::string& modelPath) {
    VocabSizeDetection result;
    result.detectedSize = 32000;
    result.isConfident = false;
    result.modelFamily = "unknown";
    result.detectionMethod = "default";
    return result;
}

bool GGUFVocabResolver::isVocabSizeReasonable(uint32_t vocabSize, const std::string& modelFamily) const {
    return vocabSize >= minVocabSize && vocabSize <= maxVocabSize;
}

uint32_t GGUFVocabResolver::applySanityBounds(uint32_t detectedSize) {
    if (detectedSize < minVocabSize) return minVocabSize;
    if (detectedSize > maxVocabSize) return maxVocabSize;
    return detectedSize;
}

std::string GGUFVocabResolver::determineModelFamily(const std::map<std::string, std::string>& metadata, const std::string& modelPath) const {
    return "unknown";
}

std::vector<std::string> GGUFVocabResolver::getAllPossibleVocabKeys() const {
    return {"vocab_size", "token_embd.weight", "output.weight"};
}

VocabSizeDetection GGUFVocabResolver::detectFromMetadata(const std::map<std::string, std::string>& metadata) {
    VocabSizeDetection result;
    result.detectedSize = 32000;
    result.isConfident = false;
    result.modelFamily = "unknown";
    return result;
}

VocabSizeDetection GGUFVocabResolver::detectFromTensorDimensions(const std::map<std::string, std::string>& metadata) {
    VocabSizeDetection result;
    result.detectedSize = 32000;
    result.isConfident = false;
    return result;
}

VocabSizeDetection GGUFVocabResolver::detectForTinyLlama(const std::map<std::string, std::string>& metadata) {
    VocabSizeDetection result;
    result.detectedSize = 32000;
    result.isConfident = false;
    return result;
}

uint32_t GGUFVocabResolver::handleSpecialCases(const std::string& modelFamily, const std::map<std::string, std::string>& metadata) {
    return 32000;
}

VocabSizeDetection GGUFVocabResolver::createDetection(const std::string& method, uint32_t size, bool confident, const std::vector<std::string>& evidence, const std::string& family) {
    VocabSizeDetection result;
    result.detectionMethod = method;
    result.detectedSize = size;
    result.isConfident = confident;
    result.evidenceKeys = evidence;
    result.modelFamily = family;
    return result;
}
