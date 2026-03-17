#ifndef EXECAI_SECURITY_H
#define EXECAI_SECURITY_H

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

// Security wrapper for ExecAI integration
class ExecAISecurity {
public:
    // Validate file paths for security
    static bool validateFilePath(const std::string& path);

    // Sanitize command arguments
    static std::vector<std::string> sanitizeArguments(const std::vector<std::string>& args);

    // Check if file is safe to process
    static bool isSafeFile(const std::filesystem::path& file);

    // Validate model file integrity
    static bool validateModelFile(const std::string& modelPath);
    static bool performDeepValidation(const std::string& path);
    static bool isGpuAvailable();
    static bool setupSandbox();

    // Clean up after execution
    static void cleanupSandbox();

private:
    static const std::vector<std::string> ALLOWED_EXTENSIONS;
    static const size_t MAX_FILE_SIZE;
    static const std::vector<std::string> DANGEROUS_PATHS;
    
    // GGUF Magic constants
    static constexpr uint8_t GGUF_MAGIC[] = { 'G', 'G', 'U', 'F' };
};

#endif // EXECAI_SECURITY_H
