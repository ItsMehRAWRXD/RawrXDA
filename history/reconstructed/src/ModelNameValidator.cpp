// ModelNameValidator.cpp - Permissive Model Name Validation
// Allows any reasonable model name format including F32-FROM-Q4

#include <string>
#include <regex>

namespace RawrXD {

bool isValidModelName(const std::string& modelName) {
    if (modelName.empty()) return false;
    if (modelName.length() > 256) return false;
    
    // Allow: letters, numbers, hyphens, underscores, dots, colons (for tags)
    // Examples:
    //   - BigDaddyG-F32-FROM-Q4
    //   - llama-2-7b-chat
    //   - mistral:latest
    //   - bigdaddyg-personalized-agentic:v1
    //   - Phi-3-mini-4k-instruct-q4.gguf
    
    std::regex validPattern(R"(^[a-zA-Z0-9_\-\.:\+]+$)");
    return std::regex_match(modelName, validPattern);
}

std::string sanitizeModelName(const std::string& modelName) {
    std::string sanitized;
    for (char c : modelName) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ':' || c == '+') {
            sanitized += c;
        } else if (c == ' ') {
            sanitized += '-';  // Replace spaces with hyphens
        }
        // Skip invalid characters
    }
    return sanitized;
}

std::string extractModelBaseName(const std::string& fullPath) {
    // Extract model name from path and remove .gguf extension
    size_t lastSlash = fullPath.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) 
        ? fullPath.substr(lastSlash + 1) 
        : fullPath;
    
    // Remove .gguf extension if present
    size_t ggufPos = filename.rfind(".gguf");
    if (ggufPos != std::string::npos) {
        filename = filename.substr(0, ggufPos);
    }
    
    return filename;
}

} // namespace RawrXD
