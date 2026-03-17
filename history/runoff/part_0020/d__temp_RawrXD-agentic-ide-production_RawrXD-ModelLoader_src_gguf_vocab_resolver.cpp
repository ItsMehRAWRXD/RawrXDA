#include "gguf_vocab_resolver.h"
#include <algorithm>
#include <iostream>
#include <sstream>

// Helper function to convert string to lowercase
static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Helper function to parse uint32 from string
static uint32_t parseUint32(const std::string& str) {
    try {
        return static_cast<uint32_t>(std::stoul(str));
    } catch (...) {
        return 0;
    }
}

// STEP 3: UTF-8 aware token validation
static bool isValidUtf8Character(const unsigned char* bytes, size_t remaining) {
    if (remaining == 0) return false;
    
    unsigned char b = bytes[0];
    int expectedBytes = 1;
    
    // Determine how many bytes this UTF-8 character should have
    if ((b & 0x80) == 0) {
        expectedBytes = 1; // ASCII
    } else if ((b & 0xE0) == 0xC0) {
        expectedBytes = 2;
    } else if ((b & 0xF0) == 0xE0) {
        expectedBytes = 3;
    } else if ((b & 0xF8) == 0xF0) {
        expectedBytes = 4;
    } else {
        return false; // Invalid UTF-8 start byte
    }
    
    if (remaining < static_cast<size_t>(expectedBytes)) {
        return false; // Not enough bytes
    }
    
    // Validate continuation bytes
    for (int i = 1; i < expectedBytes; ++i) {
        if ((bytes[i] & 0xC0) != 0x80) {
            return false; // Invalid continuation byte
        }
    }
    
    return true;
}

GGUFVocabResolver::GGUFVocabResolver() {
    setupVocabMappings();
}

void GGUFVocabResolver::setupVocabMappings() {
    // Expected vocab sizes for major model families
    expectedVocabSizes["llama"] = 32000;
    expectedVocabSizes["tinyllama"] = 32000;
    expectedVocabSizes["mistral"] = 32000;
    expectedVocabSizes["mixtral"] = 32000;
    expectedVocabSizes["gpt2"] = 50257;
    expectedVocabSizes["gptj"] = 50400;
    expectedVocabSizes["gptneox"] = 50280;
    expectedVocabSizes["bert"] = 30522;
    expectedVocabSizes["roberta"] = 50265;
    expectedVocabSizes["t5"] = 32128;
    expectedVocabSizes["gemma"] = 256000;
    expectedVocabSizes["qwen"] = 151936;
    expectedVocabSizes["phi"] = 51200;
    
    // Vocab key mappings for different model architectures
    std::vector<std::string> llamaKeys = {"llama.vocab_size", "tokenizer.ggml.vocab_size", "vocab_size"};
    std::vector<std::string> gptKeys = {"gpt.vocab_size", "tokenizer.ggml.vocab_size", "vocab_size"};
    std::vector<std::string> bertKeys = {"bert.vocab_size", "tokenizer.vocab_size", "vocab_size"};
    
    vocabKeyMappings["llama"] = llamaKeys;
    vocabKeyMappings["tinyllama"] = llamaKeys;
    vocabKeyMappings["mistral"] = llamaKeys;
    vocabKeyMappings["mixtral"] = llamaKeys;
    vocabKeyMappings["gpt"] = gptKeys;
    vocabKeyMappings["bert"] = bertKeys;
}

VocabSizeDetection GGUFVocabResolver::detectVocabSize(
    const std::map<std::string, std::string>& metadata,
    const std::string& modelPath) {
    
    // STEP 3: Add diagnostic logging for vocab detection
    std::cout << "[VocabResolver] STEP 3: Starting vocab size detection for model: " << modelPath << std::endl;
    std::cout << "[VocabResolver] Metadata entries: " << metadata.size() << std::endl;
    
    // Strategy 1: TinyLlama special case (fixes 7M bug)
    VocabSizeDetection tinyLlamaResult = detectForTinyLlama(metadata);
    if (tinyLlamaResult.isConfident) {
        std::cout << "[VocabResolver] TinyLlama detected: " << tinyLlamaResult.detectedSize << std::endl;
        return tinyLlamaResult;
    }
    
    // Strategy 2: Direct metadata lookup
    VocabSizeDetection metadataResult = detectFromMetadata(metadata);
    if (metadataResult.isConfident) {
        std::cout << "[VocabResolver] Vocab size detected from metadata: " << metadataResult.detectedSize << std::endl;
        return metadataResult;
    }
    
    // Strategy 3: Model family heuristics
    std::string modelFamily = determineModelFamily(metadata, modelPath);
    if (!modelFamily.empty() && expectedVocabSizes.count(modelFamily)) {
        uint32_t expectedSize = expectedVocabSizes[modelFamily];
        std::vector<std::string> evidence = {"model_family:" + modelFamily};
        std::cout << "[VocabResolver] Using family heuristic (" << modelFamily << "): " << expectedSize << std::endl;
        return createDetection("family_heuristic", expectedSize, true, evidence, modelFamily);
    }
    
    // Strategy 4: Fallback to common sizes
    std::cout << "[VocabResolver] WARNING: Could not confidently detect vocab size, using fallback (32000)" << std::endl;
    return createDetection("fallback", 32000, false, {}, "unknown");
}

VocabSizeDetection GGUFVocabResolver::detectForTinyLlama(
    const std::map<std::string, std::string>& metadata) {
    
    // Check if this is TinyLlama by looking for identifying metadata
    bool isTinyLlama = false;
    for (const auto& [key, value] : metadata) {
        std::string lowerKey = toLower(key);
        std::string lowerValue = toLower(value);
        if (lowerKey.find("tiny") != std::string::npos && 
            lowerKey.find("llama") != std::string::npos) {
            isTinyLlama = true;
            break;
        }
        if (lowerValue.find("tinyllama") != std::string::npos) {
            isTinyLlama = true;
            break;
        }
    }
    
    if (isTinyLlama) {
        std::vector<std::string> evidence = {"tinyllama_identifier"};
        return createDetection("tinyllama_specific", 32000, true, evidence, "tinyllama");
    }
    
    return createDetection("", 0, false, {}, "");
}

VocabSizeDetection GGUFVocabResolver::detectFromMetadata(
    const std::map<std::string, std::string>& metadata) {
    
    // Search for vocab size in common metadata keys
    std::vector<std::string> possibleKeys = {
        "llama.vocab_size",
        "tokenizer.ggml.vocab_size",
        "vocab_size",
        "gpt.vocab_size",
        "bert.vocab_size",
        "tokenizer.vocab_size",
        "model.vocab_size",
        "embedding.vocab_size"
    };
    
    for (const auto& key : possibleKeys) {
        auto it = metadata.find(key);
        if (it != metadata.end()) {
            uint32_t vocabSize = parseUint32(it->second);
            if (vocabSize > 0 && isVocabSizeReasonable(vocabSize)) {
                std::vector<std::string> evidence = {key};
                return createDetection("metadata_direct", vocabSize, true, evidence, "");
            }
        }
    }
    
    return createDetection("", 0, false, {}, "");
}

VocabSizeDetection GGUFVocabResolver::detectFromTensorDimensions(
    const std::map<std::string, std::string>& metadata) {
    
    // Look for embedding tensor dimensions (vocab size is typically first dimension)
    auto embIt = metadata.find("token_embd.weight.shape");
    if (embIt != metadata.end()) {
        // Parse shape string like "[32000,2048]"
        std::string shapeStr = embIt->second;
        size_t startPos = shapeStr.find('[');
        size_t commaPos = shapeStr.find(',');
        if (startPos != std::string::npos && commaPos != std::string::npos) {
            std::string firstDim = shapeStr.substr(startPos + 1, commaPos - startPos - 1);
            uint32_t vocabSize = parseUint32(firstDim);
            if (vocabSize > 0 && isVocabSizeReasonable(vocabSize)) {
                std::vector<std::string> evidence = {"token_embd.weight.shape"};
                return createDetection("tensor_dimension", vocabSize, true, evidence, "");
            }
        }
    }
    
    return createDetection("", 0, false, {}, "");
}

uint32_t GGUFVocabResolver::handleSpecialCases(
    const std::string& modelFamily,
    const std::map<std::string, std::string>& metadata) {
    
    if (modelFamily == "tinyllama") {
        return 32000;
    }
    if (modelFamily == "gemma") {
        return 256000;
    }
    if (modelFamily == "qwen") {
        return 151936;
    }
    
    return 0;
}

bool GGUFVocabResolver::isVocabSizeReasonable(uint32_t vocabSize, const std::string& modelFamily) const {
    if (vocabSize < minVocabSize || vocabSize > maxVocabSize) {
        return false;
    }
    
    if (!modelFamily.empty() && expectedVocabSizes.count(modelFamily)) {
        uint32_t expected = expectedVocabSizes.at(modelFamily);
        // Allow 20% deviation from expected size
        uint32_t deviation = static_cast<uint32_t>(expected * 0.2);
        if (vocabSize < expected - deviation || vocabSize > expected + deviation) {
            return false;
        }
    }
    
    return true;
}

uint32_t GGUFVocabResolver::applySanityBounds(uint32_t detectedSize) {
    if (detectedSize < minVocabSize) return minVocabSize;
    if (detectedSize > maxVocabSize) return maxVocabSize;
    return detectedSize;
}

std::string GGUFVocabResolver::determineModelFamily(
    const std::map<std::string, std::string>& metadata,
    const std::string& modelPath) const {
    
    // Check metadata for architecture hints
    for (const auto& [key, value] : metadata) {
        std::string lowerKey = toLower(key);
        std::string lowerValue = toLower(value);
        
        if (lowerValue.find("tinyllama") != std::string::npos) return "tinyllama";
        if (lowerValue.find("llama") != std::string::npos) return "llama";
        if (lowerValue.find("mistral") != std::string::npos) return "mistral";
        if (lowerValue.find("mixtral") != std::string::npos) return "mixtral";
        if (lowerValue.find("gemma") != std::string::npos) return "gemma";
        if (lowerValue.find("qwen") != std::string::npos) return "qwen";
        if (lowerValue.find("phi") != std::string::npos) return "phi";
        if (lowerValue.find("gpt") != std::string::npos) return "gpt";
        if (lowerValue.find("bert") != std::string::npos) return "bert";
    }
    
    // Check model path for hints
    std::string lowerPath = toLower(modelPath);
    if (lowerPath.find("tinyllama") != std::string::npos) return "tinyllama";
    if (lowerPath.find("llama") != std::string::npos) return "llama";
    if (lowerPath.find("mistral") != std::string::npos) return "mistral";
    if (lowerPath.find("mixtral") != std::string::npos) return "mixtral";
    if (lowerPath.find("gemma") != std::string::npos) return "gemma";
    
    return "";
}

std::vector<std::string> GGUFVocabResolver::getAllPossibleVocabKeys() const {
    std::vector<std::string> allKeys;
    for (const auto& [family, keys] : vocabKeyMappings) {
        allKeys.insert(allKeys.end(), keys.begin(), keys.end());
    }
    // Remove duplicates
    std::sort(allKeys.begin(), allKeys.end());
    allKeys.erase(std::unique(allKeys.begin(), allKeys.end()), allKeys.end());
    return allKeys;
}

VocabSizeDetection GGUFVocabResolver::createDetection(
    const std::string& method,
    uint32_t size,
    bool confident,
    const std::vector<std::string>& evidence,
    const std::string& family) {
    
    VocabSizeDetection result;
    result.detectionMethod = method;
    result.detectedSize = applySanityBounds(size);
    result.isConfident = confident && isVocabSizeReasonable(size, family);
    result.evidenceKeys = evidence;
    result.modelFamily = family;
    return result;
}
