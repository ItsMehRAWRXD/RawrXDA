#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace CPUInference {

struct VocabSizeDetection {
    std::string detectionMethod;
    uint32_t detectedSize;
    bool isConfident;
    std::vector<std::string> evidenceKeys;
    std::string modelFamily;
};

class GGUFVocabResolver {
private:
    std::map<std::string, uint32_t> expectedVocabSizes;
    std::map<std::string, std::vector<std::string>> vocabKeyMappings;
    uint32_t maxVocabSize = 1000000;
    uint32_t minVocabSize = 1000;

public:
    GGUFVocabResolver();
    
    // Universal vocab size detection
    VocabSizeDetection detectVocabSize(const std::map<std::string, std::string>& metadata,
                                     const std::string& modelPath = "");
    
    // Validation
    bool isVocabSizeReasonable(uint32_t vocabSize, const std::string& modelFamily = "") const;
    uint32_t applySanityBounds(uint32_t detectedSize);
    
    // Utilities
    std::string determineModelFamily(const std::map<std::string, std::string>& metadata, 
                                const std::string& modelPath = "") const;
    std::vector<std::string> getAllPossibleVocabKeys() const;

private:
    void setupVocabMappings();
    VocabSizeDetection detectFromMetadata(const std::map<std::string, std::string>& metadata);
    VocabSizeDetection detectFromTensorDimensions(const std::map<std::string, std::string>& metadata);
    VocabSizeDetection detectForTinyLlama(const std::map<std::string, std::string>& metadata);
    uint32_t handleSpecialCases(const std::string& modelFamily, const std::map<std::string, std::string>& metadata);
    VocabSizeDetection createDetection(const std::string& method, uint32_t size, bool confident,
                                     const std::vector<std::string>& evidence, const std::string& family);
};

} // namespace CPUInference
