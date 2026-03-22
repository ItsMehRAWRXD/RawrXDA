#include "engine_manager.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <mutex>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif

std::unique_ptr<EngineManager> g_engine_manager;
EngineManager::EngineManager() {
    // Initialize with default engines
    models_dir_ = (std::filesystem::current_path() / "models" / "800b").string();
}

EngineManager::~EngineManager() {
    std::lock_guard<std::mutex> lock(mutex_);
#ifdef _WIN32
    for (auto& kv : engines_) {
        if (kv.second && kv.second->module_handle) {
            HMODULE module = reinterpret_cast<HMODULE>(kv.second->module_handle);
            FreeLibrary(module);
            kv.second->module_handle = nullptr;
        }
    }
#endif
}

#ifdef _WIN32
static std::pair<std::string, int> RunProcess(const std::string& exe, const std::string& args,
                                              const std::string& cwd, DWORD timeoutMs = 60000) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadOut = nullptr, hWriteOut = nullptr;
    if (!CreatePipe(&hReadOut, &hWriteOut, &sa, 0)) {
        return {"Failed to create pipe", -1};
    }
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWriteOut;
    si.hStdError = hWriteOut;

    std::string cmdline = "\"" + exe + "\" " + args;
    std::vector<char> cmdBuf(cmdline.begin(), cmdline.end());
    cmdBuf.push_back('\0');

    PROCESS_INFORMATION pi{};
    LPCSTR cwdPtr = cwd.empty() ? nullptr : cwd.c_str();

    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, cwdPtr, &si, &pi);
    CloseHandle(hWriteOut);

    if (!ok) {
        DWORD err = GetLastError();
        CloseHandle(hReadOut);
        return {"CreateProcess failed (error=" + std::to_string(err) + ")", -1};
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadOut, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
        if (output.size() > 1024 * 1024) break;
    }
    CloseHandle(hReadOut);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD exitCode = 1;
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        output += "\n[TIMEOUT after " + std::to_string(timeoutMs / 1000) + "s]";
        exitCode = 1;
    } else {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return {output, static_cast<int>(exitCode)};
}
#endif

bool EngineManager::LoadEngine(const std::string& engine_path, const std::string& engine_id) {
    if (engine_id.empty()) {
        std::cerr << "LoadEngine: engine_id cannot be empty" << std::endl;
        return false;
    }

    auto engine = std::make_unique<EngineInfo>();
    engine->id = engine_id;
    engine->path = engine_path;
    engine->loaded = false;
    
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
    
#ifdef _WIN32
    if (!engine_path.empty() && std::filesystem::exists(engine_path)) {
        std::filesystem::path absPath = std::filesystem::absolute(engine_path);
        HMODULE module = LoadLibraryA(absPath.string().c_str());
        if (!module) {
            std::cerr << "Failed to load engine module: " << absPath
                      << " (error=" << GetLastError() << ")" << std::endl;
            return false;
        }
        engine->module_handle = reinterpret_cast<void*>(module);
    }
#endif

    // Engine metadata is always registered; dynamic module is optional for now.
    engine->loaded = true;
    std::lock_guard<std::mutex> lock(mutex_);
    engines_[engine_id] = std::move(engine);
    return true;
}

bool EngineManager::UnloadEngine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = engines_.find(engine_id);
    if (it != engines_.end()) {
#ifdef _WIN32
        if (it->second && it->second->module_handle) {
            HMODULE module = reinterpret_cast<HMODULE>(it->second->module_handle);
            FreeLibrary(module);
            it->second->module_handle = nullptr;
        }
#endif
        engines_.erase(it);
        if (current_engine_id_ == engine_id) {
            current_engine_id_.clear();
        }
        return true;
    }
    return false;
}

bool EngineManager::SwitchEngine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = engines_.find(engine_id);
    if (it != engines_.end() && it->second->loaded) {
        current_engine_id_ = engine_id;
        std::cout << "Switched to engine: " << engine_id << std::endl;
        return true;
    }
    return false;
}

std::vector<std::string> EngineManager::GetAvailableEngines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& pair : engines_) {
        result.push_back(pair.first);
    }
    return result;
}

std::string EngineManager::GetCurrentEngine() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_engine_id_;
}

const EngineInfo* EngineManager::GetEngine(const std::string& engine_id) const {
    auto it = engines_.find(engine_id);
    if (it != engines_.end()) {
        return it->second.get();
    }
    return nullptr;
}

EngineInfo* EngineManager::GetEngineMut(const std::string& engine_id) {
    auto it = engines_.find(engine_id);
    if (it != engines_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool EngineManager::Load800BModel(const std::string& model_name) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_engine_id_ != "800b-5drive") {
            std::cerr << "Error: Current engine doesn't support 800B models" << std::endl;
            return false;
        }
    }

    for (char c : model_name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.') {
            std::cerr << "Error: Invalid character in model name: '" << c << "'" << std::endl;
            return false;
        }
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
    
    // Load model parts from each drive and compute total size
    std::vector<std::string> model_parts;
    uintmax_t total_size = 0;
    for (size_t i = 0; i < 5; i++) {
        std::string part_path = drives[i] + "/" + model_name + ".part" + std::to_string(i);
        if (!std::filesystem::exists(part_path)) {
            std::cerr << "Error: Model part not found: " << part_path << std::endl;
            return false;
        }
        total_size += std::filesystem::file_size(part_path);
        model_parts.push_back(part_path);
    }

    auto space = std::filesystem::space(models_dir_);
    if (space.available < total_size) {
        std::cerr << "Error: Insufficient disk space for model assembly" << std::endl;
        return false;
    }

    std::filesystem::path output_path = std::filesystem::path(models_dir_) / (model_name + ".gguf");
    std::filesystem::path temp_path = std::filesystem::path(models_dir_) / (model_name + ".gguf.tmp");
    std::ofstream output(temp_path, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "Error: Failed to open temp model file: " << temp_path << std::endl;
        return false;
    }

    constexpr size_t BUFFER_SIZE = 4 * 1024 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);
    uintmax_t written = 0;

    for (size_t part_idx = 0; part_idx < model_parts.size(); ++part_idx) {
        const auto& part = model_parts[part_idx];
        std::ifstream input(part, std::ios::binary);
        if (!input.is_open()) {
            std::cerr << "Error: Failed to open model part file: " << part << std::endl;
            std::filesystem::remove(temp_path);
            return false;
        }

        while (input.read(buffer.data(), BUFFER_SIZE) || input.gcount() > 0) {
            output.write(buffer.data(), input.gcount());
            if (!output) {
                std::cerr << "Error: Failed while writing combined model file: " << temp_path << std::endl;
                std::filesystem::remove(temp_path);
                return false;
            }
            written += static_cast<uintmax_t>(input.gcount());
            if ((written % (1024ULL * 1024 * 1024)) < BUFFER_SIZE) {
                std::cout << "  Progress: " << (written / (1024ULL * 1024 * 1024))
                          << " / " << (total_size / (1024ULL * 1024 * 1024))
                          << " GB (part " << (part_idx + 1) << "/" << model_parts.size() << ")"
                          << std::endl;
            }
        }
    }

    output.close();
    std::error_code ec;
    std::filesystem::rename(temp_path, output_path, ec);
    if (ec) {
        std::cerr << "Error: Failed to finalize model file: " << ec.message() << std::endl;
        std::filesystem::remove(temp_path);
        return false;
    }
    
    std::cout << "Successfully loaded 800B model: " << model_name << std::endl;
    return true;
}

bool EngineManager::Setup5DriveLayout(const std::string& base_dir) {
    models_dir_ = base_dir;
    std::error_code ec;
    std::filesystem::create_directories(models_dir_, ec);
    if (ec) {
        std::cerr << "Failed to create models directory: " << ec.message() << std::endl;
        return false;
    }
    
    // Create subdirectories for each drive
    for (int i = 1; i <= 5; i++) {
        std::string drive_path = (std::filesystem::path(models_dir_) / ("drive" + std::to_string(i))).string();
        std::filesystem::create_directories(drive_path, ec);
        if (ec) {
            std::cerr << "Failed to create drive" << i << ": " << ec.message() << std::endl;
            return false;
        }
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
    std::lock_guard<std::mutex> lock(mutex_);
    auto* engine = GetEngineMut(engine_id);
    if (engine) {
        engine->supports_streaming = true;
        return true;
    }
    return false;
}

bool EngineManager::DisableStreaming(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* engine = GetEngineMut(engine_id);
    if (engine) {
        engine->supports_streaming = false;
        return true;
    }
    return false;
}

size_t EngineManager::GetOptimalContextSize(const std::string& engine_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
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
    if (compiler_id.empty() || compiler_path.empty()) return false;
    if (!std::filesystem::exists(compiler_path)) {
        std::cerr << "RegisterCompiler: path does not exist: " << compiler_path << std::endl;
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    registered_compilers_[compiler_id] = std::filesystem::absolute(compiler_path).string();
    std::cout << "Registered compiler '" << compiler_id << "' at " << compiler_path << std::endl;
    return true;
}

bool EngineManager::CompileWithEngine(const std::string& engine_id, const std::string& source_file) {
    if (engine_id != "rawrxd-compiler") {
        std::cerr << "Error: Only rawrxd-compiler engine supports compilation" << std::endl;
        return false;
    }
    if (source_file.empty() || !std::filesystem::exists(source_file)) {
        std::cerr << "Error: Source file not found: " << source_file << std::endl;
        return false;
    }

#ifdef _WIN32
    // Resolve compiler path: registered compiler > default ml64.exe
    std::string compilerPath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registered_compilers_.find("ml64");
        if (it != registered_compilers_.end()) {
            compilerPath = it->second;
        } else {
            compilerPath = "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\ml64.exe";
        }
    }

    if (!std::filesystem::exists(compilerPath)) {
        std::cerr << "CompileWithEngine: Compiler not found at " << compilerPath << std::endl;
        return false;
    }

    // Build output path next to source
    std::filesystem::path srcPath(source_file);
    std::string objPath = (srcPath.parent_path() / srcPath.stem()).string() + ".obj";

    std::string args = "/c /Zi /Fo\"" + objPath + "\" \"" + srcPath.string() + "\"";
    auto runResult = RunProcess(compilerPath, args, srcPath.parent_path().string(), 120000);
    if (runResult.second != 0) {
        std::cerr << "Compilation failed (exit=" << runResult.second << "):\n" << runResult.first << std::endl;
    }
    return runResult.second == 0;
#else
    std::cerr << "CompileWithEngine: MASM64 only available on Windows" << std::endl;
    return false;
#endif
}

std::string EngineManager::GetEngineHelp(const std::string& engine_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* engine = GetEngine(engine_id);
    if (!engine) return "Engine not found: " + engine_id;
    
    std::string help = "Engine: " + engine->name + "\n";
    help += "ID: " + engine->id + "\n";
    help += "Description: " + engine->description + "\n";
    help += "Status: " + std::string(engine->loaded ? "Loaded" : "Not loaded") + "\n";
    help += "Streaming: " + std::string(engine->supports_streaming ? "Yes" : "No") + "\n";
    help += "Max Model Size: " + std::to_string(engine->max_model_size) + "GB\n";
    help += "Supported Formats: ";
    for (size_t i = 0; i < engine->supported_formats.size(); i++) {
        if (i > 0) help += ", ";
        help += engine->supported_formats[i];
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
