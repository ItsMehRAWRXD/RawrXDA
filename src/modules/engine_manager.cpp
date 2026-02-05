#include "engine_manager.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstdlib>

std::unique_ptr<EngineManager> g_engine_manager;

EngineManager::EngineManager() {
    // Initialize with default engines
    models_dir_ = std::filesystem::current_path() / "models" / "800b";
}

EngineManager::~EngineManager() = default;

bool EngineManager::LoadEngine(const std::string& engine_path, const std::string& engine_id) {
    auto engine = std::make_unique<EngineInfo>();
    engine->id = engine_id;
    engine->path = engine_path;
    engine->loaded = true;
    
    // Set engine-specific properties
    if (engine_id == "800b-5drive") {
        engine->name = "800B Model Engine (5-Drive)";
        engine->description = "Supports 800B models using 5-drive distributed loading";
        engine->supports_streaming = true;
        engine->max_model_size = 800; // 800GB
        engine->supported_formats = {"gguf", "safetensors"};
    }
    else if (engine_id == "codex-ultimate") {
        engine->name = "Codex Ultimate";
        engine->description = "Reverse engineering suite with disassembler, dumpbin, compiler";
        engine->supports_streaming = false;
        engine->max_model_size = 100;
        engine->supported_formats = {"exe", "dll", "obj"};
    }
    else if (engine_id == "rawrxd-compiler") {
        engine->name = "RawrXD MASM64 Compiler";
        engine->description = "High-performance MASM64 compiler with AVX-512 optimization";
        engine->supports_streaming = false;
        engine->max_model_size = 50;
        engine->supported_formats = {"asm", "cpp", "c"};
    }
    else {
        engine->name = engine_id;
        engine->description = "Custom engine";
        engine->supports_streaming = true;
        engine->max_model_size = 100;
        engine->supported_formats = {"gguf"};
    }
    
    engines_[engine_id] = std::move(engine);
    return true;
}

bool EngineManager::UnloadEngine(const std::string& engine_id) {
    auto it = engines_.find(engine_id);
    if (it != engines_.end()) {
        engines_.erase(it);
        if (current_engine_id_ == engine_id) {
            current_engine_id_.clear();
        }
        return true;
    }
    return false;
}

bool EngineManager::SwitchEngine(const std::string& engine_id) {
    auto it = engines_.find(engine_id);
    if (it != engines_.end() && it->second->loaded) {
        current_engine_id_ = engine_id;
        std::cout << "Switched to engine: " << engine_id << std::endl;
        return true;
    }
    return false;
}

std::vector<std::string> EngineManager::GetAvailableEngines() const {
    std::vector<std::string> result;
    for (const auto& pair : engines_) {
        result.push_back(pair.first);
    }
    return result;
}

std::string EngineManager::GetCurrentEngine() const {
    return current_engine_id_;
}

EngineInfo* EngineManager::GetEngine(const std::string& engine_id) {
    auto it = engines_.find(engine_id);
    if (it != engines_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool EngineManager::Load800BModel(const std::string& model_name) {
    if (current_engine_id_ != "800b-5drive") {
        std::cerr << "Error: Current engine doesn't support 800B models" << std::endl;
        return false;
    }
    
    // Verify 5-drive setup
    if (!VerifyDriveSetup()) {
        std::cerr << "Error: 5-drive setup not properly configured" << std::endl;
        return false;
    }
    
    auto drives = GetDrivePaths();
    if (drives.size() != 5) {
        std::cerr << "Error: Expected 5 drives, found " << drives.size() << std::endl;
        return false;
    }
    
    // Load model parts from each drive
    std::vector<std::string> model_parts;
    for (size_t i = 0; i < 5; i++) {
        std::string part_path = drives[i] + "/" + model_name + ".part" + std::to_string(i);
        if (!std::filesystem::exists(part_path)) {
            std::cerr << "Error: Model part not found: " << part_path << std::endl;
            return false;
        }
        model_parts.push_back(part_path);
    }
    
    // Combine parts (simplified - would use mmap in real implementation)
    std::filesystem::path output_path = models_dir_ / (model_name + ".gguf");
    std::ofstream output(output_path, std::ios::binary);
    
    for (const auto& part : model_parts) {
        std::ifstream input(part, std::ios::binary);
        output << input.rdbuf();
    }
    
    std::cout << "Successfully loaded 800B model: " << model_name << std::endl;
    return true;
}

bool EngineManager::Setup5DriveLayout(const std::string& base_dir) {
    models_dir_ = std::filesystem::path(base_dir);
    std::filesystem::create_directories(models_dir_);
    
    // Create subdirectories for each drive
    for (int i = 1; i <= 5; i++) {
        std::string drive_path = (models_dir_ / ("drive" + std::to_string(i))).string();
        std::filesystem::create_directories(drive_path);
    }
    
    std::cout << "5-drive layout created at: " << base_dir << std::endl;
    return true;
}

bool EngineManager::VerifyDriveSetup() {
    auto drives = GetDrivePaths();
    if (drives.size() != 5) {
        return false;
    }
    
    for (const auto& drive : drives) {
        if (!std::filesystem::exists(drive)) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> EngineManager::GetDrivePaths() {
    std::vector<std::string> paths;
    for (int i = 1; i <= 5; i++) {
        paths.push_back((std::filesystem::path(models_dir_) / ("drive" + std::to_string(i))).string());
    }
    return paths;
}

bool EngineManager::EnableStreaming(const std::string& engine_id) {
    auto* engine = GetEngine(engine_id);
    if (engine) {
        engine->supports_streaming = true;
        return true;
    }
    return false;
}

bool EngineManager::DisableStreaming(const std::string& engine_id) {
    auto* engine = GetEngine(engine_id);
    if (engine) {
        engine->supports_streaming = false;
        return true;
    }
    return false;
}

size_t EngineManager::GetOptimalContextSize(const std::string& engine_id) {
    auto* engine = GetEngine(engine_id);
    if (!engine) return 4096; // Default 4K
    
    // Return optimal context based on engine capabilities
    if (engine_id == "800b-5drive") {
        return 32768; // 32K for large models
    }
    else if (engine_id == "codex-ultimate") {
        return 8192; // 8K for reverse engineering
    }
    else if (engine_id == "rawrxd-compiler") {
        return 4096; // 4K for compilation tasks
    }
    
    return 4096;
}

bool EngineManager::RegisterCompiler(const std::string& compiler_id, const std::string& compiler_path) {
    // Register compiler for agentic access
    auto* engine = GetEngine("rawrxd-compiler");
    if (engine) {
        // Simple way to store it in manifest like structure if needed, or just log it
        // engine->manifest["compilers"][compiler_id] = compiler_path;
        return true; // Simplified
    }
    return false;
}

bool EngineManager::CompileWithEngine(const std::string& engine_id, const std::string& source_file) {
    if (engine_id != "rawrxd-compiler") {
        std::cerr << "Error: Only rawrxd-compiler engine supports compilation" << std::endl;
        return false;
    }
    
    // Execute compilation (simplified)
    std::string command = "ml64 /c /Fo output.obj " + source_file;
    int result = std::system(command.c_str());
    return result == 0;
}

std::string EngineManager::GetEngineHelp(const std::string& engine_id) const {
    auto* engine = const_cast<EngineManager*>(this)->GetEngine(engine_id);
    if (!engine) return "Engine not found: " + engine_id;
    
    std::string help = "Engine: " + engine->name + "\n";
    help += "ID: " + engine->id + "\n";
    help += "Description: " + engine->description + "\n";
    help += "Status: " + std::string(engine->loaded ? "Loaded" : "Not loaded") + "\n";
    help += "Streaming: " + std::string(engine->supports_streaming ? "Yes" : "No") + "\n";
    help += "Max Model Size: " + std::to_string(engine->max_model_size) + "GB\n";
    help += "Supported Formats: ";
    for (const auto& format : engine->supported_formats) {
        help += format + ", ";
    }
    help += "\n";
    
    // Engine-specific help
    if (engine_id == "800b-5drive") {
        help += "\n800B Model Engine Features:\n";
        help += "- Distributed loading across 5 drives\n";
        help += "- Streaming support for memory efficiency\n";
        help += "- Optimized for large models (100GB+)\n";
        help += "- Requires 5-drive setup in models/800b/\n";
        help += "\nCommands:\n";
        help += "  !engine load800b <model_name> - Load 800B model\n";
        help += "  !engine setup5drive <dir>     - Setup 5-drive layout\n";
        help += "  !engine verify                - Verify drive setup\n";
    }
    else if (engine_id == "codex-ultimate") {
        help += "\nCodex Ultimate Features:\n";
        help += "- Reverse engineering suite\n";
        help += "- Disassembler for binary analysis\n";
        help += "- Dumpbin for PE/COFF inspection\n";
        help += "- MASM64 compiler integration\n";
        help += "- Agentic code analysis\n";
        help += "\nCommands:\n";
        help += "  !engine disasm <file>         - Disassemble binary\n";
        help += "  !engine dumpbin <file>        - Dump PE/COFF info\n";
        help += "  !engine compile <file>        - Compile with MASM64\n";
        help += "  !engine analyze <file>        - Agentic analysis\n";
    }
    else if (engine_id == "rawrxd-compiler") {
        help += "\nRawrXD Compiler Features:\n";
        help += "- MASM64 compiler with AVX-512\n";
        help += "- Optimized for performance\n";
        help += "- Agentic compilation assistance\n";
        help += "- Real-time error correction\n";
        help += "\nCommands:\n";
        help += "  !engine compile <file>        - Compile source\n";
        help += "  !engine optimize <file>       - Optimize code\n";
        help += "  !engine correct <file>        - Auto-correct errors\n";
    }
    
    return help;
}

std::string EngineManager::GetAllEnginesHelp() const {
    std::string help = "=== Available Engines ===\n\n";
    
    auto engines = GetAvailableEngines();
    for (const auto& engine_id : engines) {
        help += GetEngineHelp(engine_id) + "\n";
    }
    
    help += "=== Usage ===\n";
    help += "!engine list                    - List all engines\n";
    help += "!engine switch <id>             - Switch to engine\n";
    help += "!engine load <path> <id>        - Load new engine\n";
    help += "!engine unload <id>             - Unload engine\n";
    help += "!engine help <id>               - Show engine help\n";
    help += "!engine load800b <model>        - Load 800B model (800b-5drive only)\n";
    help += "!engine setup5drive <dir>       - Setup 5-drive layout\n";
    help += "!engine verify                  - Verify drive setup\n";
    
    return help;
}
