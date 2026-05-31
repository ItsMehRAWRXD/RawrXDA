// D:\rawrxd\generator\file_emitter.cpp
// File Output and Atomic Write Implementation

#include "file_emitter.hpp"
#include "generator.hpp" // For GeneratedFile and GeneratorError
#include <fstream>
#include <iostream>

namespace RawrXD::CodeGen {

FileEmitter::FileEmitter() 
    : output_directory_("./generated")
{
    std::filesystem::create_directories(output_directory_);
}

FileEmitter::~FileEmitter() = default;

void FileEmitter::SetOutputDirectory(const std::filesystem::path& path) {
    output_directory_ = path;
    std::filesystem::create_directories(output_directory_);
}

std::expected<void, GeneratorError> 
FileEmitter::Emit(const std::vector<GeneratedFile>& files) {
    for (const auto& file : files) {
        if (auto result = EmitFile(file); !result) {
            return result;
        }
    }
    return {};
}

std::expected<void, GeneratorError> 
FileEmitter::EmitFile(const GeneratedFile& file) {
    auto full_path = output_directory_ / file.relative_path;
    
    // Create parent directories if needed
    std::filesystem::create_directories(full_path.parent_path());
    
    // Create backup if file exists and backup is enabled
    if (create_backups_ && std::filesystem::exists(full_path)) {
        if (!CreateBackup(full_path)) {
            return std::unexpected(GeneratorError::FileWriteFailed);
        }
    }
    
    // Perform atomic write
    if (!WriteAtomic(full_path, file.content)) {
        return std::unexpected(GeneratorError::FileWriteFailed);
    }
    
    return {};
}

bool FileEmitter::WriteAtomic(const std::filesystem::path& path, const std::string& content) {
    if (!atomic_write_) {
        // Simple write (not atomic)
        try {
            std::ofstream ofs(path, std::ios::binary);
            if (!ofs) return false;
            ofs.write(content.c_str(), static_cast<std::streamsize>(content.size()));
            ofs.close();
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Atomic write: write to temp file, then rename
    try {
        auto temp_path = path.string() + ".tmp";
        
        // Write to temporary file
        {
            std::ofstream ofs(temp_path, std::ios::binary);
            if (!ofs) return false;
            ofs.write(content.c_str(), static_cast<std::streamsize>(content.size()));
            ofs.close();
        }
        
        // Atomic rename
        std::error_code ec;
        std::filesystem::remove(path, ec);  // Remove old file
        std::filesystem::rename(temp_path, path, ec);
        return !ec;
    } catch (...) {
        return false;
    }
}

bool FileEmitter::CreateBackup(const std::filesystem::path& path) {
    try {
        auto backup_path = path.string() + ".backup";
        std::filesystem::copy_file(path, backup_path, 
                                   std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

std::expected<std::vector<std::filesystem::path>, GeneratorError> 
FileEmitter::Preview(const std::vector<GeneratedFile>& files) {
    std::vector<std::filesystem::path> paths;
    
    for (const auto& file : files) {
        paths.push_back(output_directory_ / file.relative_path);
    }
    
    return paths;
}

} // namespace RawrXD::CodeGen
