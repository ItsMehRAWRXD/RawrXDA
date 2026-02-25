#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

struct AnalysisResult {
    std::string originalPath;
    std::string fileType;
    std::string arch;
    std::string hash;
    std::vector<std::string> strings;
    std::vector<std::string> exportedFunctions;
    std::vector<std::string> importedFunctions;
    double entropy;
};

class CodexIntegration {
public:
    CodexIntegration();
    ~CodexIntegration();

    /**
     * Analyze a compiled binary file
     */
    AnalysisResult analyzeBinary(const std::string& filePath);

    /**
     * Reconstruct simplified code structure from binary (Decompilation stub/heuristic)
     */
    std::string reconstructCode(const AnalysisResult& analysis);

private:
    std::string calculateHash(const std::vector<uint8_t>& data);
    double calculateEntropy(const std::vector<uint8_t>& data);
    std::vector<std::string> extractStrings(const std::vector<uint8_t>& data, size_t minLen = 4);
    std::vector<std::string> parseExports(const std::vector<uint8_t>& data); // Simple PE parser
};

} // namespace RawrXD
