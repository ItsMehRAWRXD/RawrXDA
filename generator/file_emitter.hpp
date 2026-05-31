// D:\rawrxd\generator\file_emitter.hpp
// File Output and Atomic Write Management

#pragma once
#include <filesystem>
#include <expected>
#include <string>
#include <vector>

namespace RawrXD::CodeGen {

// Forward declarations
struct GeneratedFile;
enum class GeneratorError;

class FileEmitter {
public:
    FileEmitter();
    ~FileEmitter();
    
    // Emit generated files to disk
    std::expected<void, GeneratorError> Emit(const std::vector<GeneratedFile>& files);
    
    // Emit a single file
    std::expected<void, GeneratorError> EmitFile(const GeneratedFile& file);
    
    // Set output directory
    void SetOutputDirectory(const std::filesystem::path& path);
    
    // Preview what would be written without writing
    std::expected<std::vector<std::filesystem::path>, GeneratorError> 
        Preview(const std::vector<GeneratedFile>& files);
    
    // Configuration
    bool atomic_write_ = true;
    bool create_backups_ = true;
    
private:
    std::filesystem::path output_directory_;
    
    // Atomic write: write to temp, then rename
    bool WriteAtomic(const std::filesystem::path& path, const std::string& content);
    
    // Create backup before overwriting
    bool CreateBackup(const std::filesystem::path& path);
};

} // namespace RawrXD::CodeGen
