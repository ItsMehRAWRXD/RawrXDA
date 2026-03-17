#include "tls-beaconism-pro.hpp"
#include <iostream>
#include <filesystem>

TLSBeaconismIDE::TLSBeaconismIDE() {
    toolchain_manager_ = std::make_unique<ToolchainManager>("portable-toolchains");
}

TLSBeaconismIDE::~TLSBeaconismIDE() {
    Shutdown();
}

bool TLSBeaconismIDE::Initialize() {
    std::cout << "TLS Beaconism IDE - Professional Edition\n";
    std::cout << "========================================\n\n";
    
    // Initialize toolchain manager
    if (!toolchain_manager_->DetectAllToolchains()) {
        std::cout << "Some toolchains are missing. Run setup-toolchains.bat to install them.\n\n";
    }
    
    toolchain_manager_->PrintStatus();
    
    // Initialize supported languages
    InitializeSupportedLanguages();
    
    running_ = true;
    return true;
}

// Supported languages are automatically detected via toolchain manager
void TLSBeaconismIDE::InitializeSupportedLanguages() {
    supported_languages_.clear();
    
    // C/C++ via clang++ portable if available
    if (toolchain_manager_) {
        // We will store known compiler paths by name for quick lookup in CompileSingleFile
        toolchain_paths_.clear();
        
        // Prefer clang++ in portable toolchains
        std::string clangxx = std::string("portable-toolchains\\clang-portable\\bin\\clang++.exe");
        if (std::filesystem::exists(clangxx)) {
            toolchain_paths_["cxx"] = clangxx;
            supported_languages_.push_back({
                "C++", ".cpp", clangxx, {"class","struct","namespace","template","auto","constexpr"}, "", true, true
            });
            supported_languages_.push_back({
                "C", ".c", clangxx, {"int","char","float","double","return","struct"}, "", false, false
            });
        }
        
        // Java
        if (toolchain_manager_->IsAvailable("java")) {
            supported_languages_.push_back({
                "Java", ".java", toolchain_manager_->GetToolchainPath("java"), {"class","public","private","static"}, "", true, true
            });
        }
        
        // Kotlin
        if (toolchain_manager_->IsAvailable("kotlinc")) {
            supported_languages_.push_back({
                "Kotlin", ".kt", toolchain_manager_->GetToolchainPath("kotlinc"), {"fun","val","var","class"}, "", true, true
            });
        }
        
        // Zig
        if (toolchain_manager_->IsAvailable("zig")) {
            supported_languages_.push_back({
                "Zig", ".zig", toolchain_manager_->GetToolchainPath("zig"), {"fn","const","var","struct"}, "", false, false
            });
        }
    }
}

TLSBeaconismIDE::CompileResult TLSBeaconismIDE::CompileSingleFile(const std::string& file_path, const std::string& output_path) {
    auto start_time = std::chrono::high_resolution_clock::now();
    CompileResult result;
    
    LanguageInfo lang = DetectLanguage(file_path);
    if (lang.name.empty()) {
        result.success = false;
        result.errors = "Unsupported file type";
        return result;
    }
    
    std::string output = output_path.empty() ? 
        std::filesystem::path(file_path).stem().string() + ".exe" : output_path;
    
    std::string command;
    
    if (lang.name == "Java") {
        command = "javac " + file_path;
    }
    else if (lang.name == "Kotlin") {
        command = toolchain_manager_->GetToolchainPath("kotlinc") + 
                 " " + file_path + " -include-runtime -d " + output;
    }
    else if (lang.name == "Zig") {
        command = toolchain_manager_->GetToolchainPath("zig") + 
                 " build-exe " + file_path;
    }
    else if (lang.name == "C++") {
        std::string cxx = toolchain_paths_.count("cxx") ? toolchain_paths_["cxx"] : "clang++";
        command = cxx + " -std=c++20 -O2 \"" + file_path + "\" -o \"" + output + "\"";
    }
    else if (lang.name == "C") {
        std::string cxx = toolchain_paths_.count("cxx") ? toolchain_paths_["cxx"] : "clang++";
        command = cxx + " -std=c17 -O2 \"" + file_path + "\" -o \"" + output + "\"";
    }
    
    if (!command.empty()) {
        int exit_code = system(command.c_str());
        result.success = (exit_code == 0);
        result.executable_path = output;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    compile_count_++;
    return result;
}

LanguageInfo TLSBeaconismIDE::DetectLanguage(const std::string& file_path) {
    std::string ext = std::filesystem::path(file_path).extension().string();
    
    // Built-in language support for TLS header-free compilation
    if (ext == ".js") {
        return {"JavaScript", ".js", "node", {"function", "const", "let", "var"}, "", true, true};
    }
    else if (ext == ".ts") {
        return {"TypeScript", ".ts", "tsc", {"interface", "type", "class"}, "", true, true};
    }
    else if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") {
        return {"C++", ext, "clang++", {"class", "struct", "namespace"}, "", true, true};
    }
    else if (ext == ".c") {
        return {"C", ".c", "clang", {"struct", "typedef", "enum"}, "", true, true};
    }
    else if (ext == ".cs") {
        return {"C#", ".cs", "csc", {"class", "interface", "namespace"}, "", true, true};
    }
    else if (ext == ".java") {
        return {"Java", ".java", "javac", {"class", "interface", "package"}, "", true, true};
    }
    else if (ext == ".kt") {
        return {"Kotlin", ".kt", "kotlinc", {"class", "fun", "val"}, "", true, true};
    }
    else if (ext == ".rs") {
        return {"Rust", ".rs", "rustc", {"fn", "struct", "enum"}, "", true, true};
    }
    else if (ext == ".go") {
        return {"Go", ".go", "go", {"func", "struct", "interface"}, "", true, true};
    }
    else if (ext == ".zig") {
        return {"Zig", ".zig", "zig", {"fn", "struct", "pub"}, "", true, false};
    }
    else if (ext == ".py") {
        return {"Python", ".py", "python", {"def", "class", "import"}, "", true, true};
    }
    
    return LanguageInfo{}; // Empty if not found
}

std::vector<LanguageInfo> TLSBeaconismIDE::GetSupportedLanguages() const {
    return supported_languages_;
}

bool TLSBeaconismIDE::DetectAllToolchains() {
    return toolchain_manager_->DetectAllToolchains();
}

std::vector<std::string> TLSBeaconismIDE::ListDirectory(const std::string& path) {
    std::vector<std::string> files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                name += "/";
            }
            files.push_back(name);
        }
    } catch (const std::exception& e) {
        // Handle error silently
    }
    
    return files;
}

std::string TLSBeaconismIDE::ReadFileOptimized(const std::string& path) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check cache first
    auto cache_it = file_cache_.find(path);
    if (cache_it != file_cache_.end()) {
        cache_hits_++;
        return cache_it->second;
    }
    
    cache_misses_++;
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // Cache small files
    if (content.size() < 1024 * 1024) { // 1MB limit
        file_cache_[path] = content;
        file_timestamps_[path] = std::chrono::system_clock::now();
    }
    
    return content;
}

bool TLSBeaconismIDE::WriteFileOptimized(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    
    // Update cache
    std::lock_guard<std::mutex> lock(cache_mutex_);
    file_cache_[path] = content;
    file_timestamps_[path] = std::chrono::system_clock::now();
    
    return true;
}

double TLSBeaconismIDE::GetCacheHitRatio() const {
    uint64_t total = cache_hits_ + cache_misses_;
    return total > 0 ? (double)cache_hits_ / total : 0.0;
}

std::string TLSBeaconismIDE::GetPerformanceReport() const {
    std::ostringstream report;
    report << "Performance Report:\n";
    report << "==================\n";
    report << "Cache Hit Ratio: " << (GetCacheHitRatio() * 100.0) << "%\n";
    report << "Compilations: " << compile_count_ << "\n";
    report << "Debug Sessions: " << debug_sessions_ << "\n";
    report << "Cached Files: " << file_cache_.size() << "\n";
    return report.str();
}

void TLSBeaconismIDE::Shutdown() {
    running_ = false;
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    file_cache_.clear();
    file_timestamps_.clear();
}

// Extension Manager Implementation with Marketplace
bool ExtensionManager::Initialize() {
    available_extensions_ = {
        {"cpp-enhanced", "C++ Enhanced", "2.1.0", "Advanced C++ IntelliSense", "TLS Team", {"C++"}, false, false, "", 12543, 4.8},
        {"rust-analyzer", "Rust Analyzer Pro", "1.5.2", "Complete Rust support", "Rust Foundation", {"Rust"}, false, false, "", 8921, 4.9},
        {"python-ai", "Python AI Assistant", "3.0.1", "AI-powered Python dev", "PyDev Corp", {"Python"}, false, false, "", 15632, 4.7},
        {"web-stack", "Full Stack Web Dev", "2.8.1", "Complete web toolkit", "WebDev Inc", {"JavaScript", "TypeScript"}, false, false, "", 23451, 4.9},
        {"mobile-dev", "Mobile Development Kit", "1.9.0", "Cross-platform mobile", "Mobile Studios", {"Dart", "Swift"}, false, false, "", 7632, 4.5}
    };
    
    std::cout << "[MARKETPLACE] Initialized with " << available_extensions_.size() << " extensions\n";
    return true;
}

std::vector<Extension> ExtensionManager::BrowseMarketplace() {
    return available_extensions_;
}

bool ExtensionManager::InstallExtension(const std::string& extension_id) {
    for (auto& ext : available_extensions_) {
        if (ext.id == extension_id) {
            ext.is_installed = true;
            ext.is_enabled = true;
            loaded_extensions_.push_back(ext.name);
            std::cout << "[MARKETPLACE] Installed: " << ext.name << " v" << ext.version << "\n";
            return true;
        }
    }
    return false;
}

bool ExtensionManager::LoadExtension(const std::string& path) {
    loaded_extensions_.push_back(path);
    std::cout << "[EXTENSION] Loaded: " << path << "\n";
    return true;
}

// Missing function implementations for TLS IDE
uint64_t TLSBeaconismIDE::GetCompileCount() const {
    return compile_count_;
}

uint64_t TLSBeaconismIDE::GetDebugSessionCount() const {
    return debug_sessions_;
}

// DebuggerCore destructor
DebuggerCore::~DebuggerCore() {
    // Cleanup debug resources
}