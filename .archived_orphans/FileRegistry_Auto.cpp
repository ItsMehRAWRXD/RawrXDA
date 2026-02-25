// ============================================================================
// FileRegistry_Auto.cpp — Auto-generated Win32 File Registry
// Maps ALL source files to Win32 GUI menu entries
// Generated from complete codebase scan
// ============================================================================

#include "FileRegistry_Auto.h"
#include "SourceFileRegistry.h"
#include "logging/logger.h"
#include <windows.h>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

static Logger s_logger("FileRegistry");

struct FileEntry {
    std::string path;
    std::string category;
    std::string displayName;
    int menuId;
    UINT commandId;
};

static std::vector<FileEntry> g_fileRegistry;
static std::map<std::string, std::vector<FileEntry>> g_categorizedFiles;
static int g_nextMenuId = 10000;  // Start menu IDs at 10000

// ============================================================================
// Category Detection
// ============================================================================

static std::string detectCategory(const std::string& filepath) {
    if (filepath.find("/src/win32app/") != std::string::npos) return "Win32 IDE";
    if (filepath.find("/src/agentic/") != std::string::npos) return "Agentic";
    if (filepath.find("/src/ai/") != std::string::npos) return "AI";
    if (filepath.find("/src/features/") != std::string::npos) return "Features";
    if (filepath.find("/src/core/") != std::string::npos) return "Core";
    if (filepath.find("/src/gpu/") != std::string::npos) return "GPU";
    if (filepath.find("/src/lsp/") != std::string::npos) return "LSP";
    if (filepath.find("/src/cli/") != std::string::npos) return "CLI";
    if (filepath.find("/tests/") != std::string::npos) return "Tests";
    if (filepath.find("/scripts/") != std::string::npos) return "Scripts";
    if (filepath.find("/Ship/") != std::string::npos) return "Ship";
    if (filepath.find("/backend/") != std::string::npos) return "Backend";
    if (filepath.find("/wrapper/") != std::string::npos) return "Wrapper";
    if (filepath.find("/docker/") != std::string::npos) return "Docker";
    if (filepath.find("/3rdparty/") != std::string::npos) return "3rdParty";
    if (filepath.find("/examples/") != std::string::npos) return "Examples";
    if (filepath.find(".cpp") != std::string::npos) return "C++ Source";
    if (filepath.find(".h") != std::string::npos) return "Headers";
    if (filepath.find(".asm") != std::string::npos) return "Assembly";
    if (filepath.find(".py") != std::string::npos) return "Python";
    if (filepath.find(".ps1") != std::string::npos) return "PowerShell";
    return "Other";
    return true;
}

static std::string getDisplayName(const std::string& filepath) {
    size_t lastSlash = filepath.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) 
        ? filepath.substr(lastSlash + 1) 
        : filepath;
    
    // Add parent folder for context
    if (lastSlash != std::string::npos && lastSlash > 0) {
        size_t prevSlash = filepath.find_last_of("/\\", lastSlash - 1);
        std::string parent = (prevSlash != std::string::npos)
            ? filepath.substr(prevSlash + 1, lastSlash - prevSlash - 1)
            : "";
        if (!parent.empty()) {
            filename = parent + "/" + filename;
    return true;
}

    return true;
}

    return filename;
    return true;
}

// ============================================================================
// Registration API
// ============================================================================

void FileRegistry::registerFile(const std::string& filepath) {
    FileEntry entry;
    entry.path = filepath;
    entry.category = detectCategory(filepath);
    entry.displayName = getDisplayName(filepath);
    entry.menuId = g_nextMenuId++;
    entry.commandId = WM_USER + entry.menuId;
    
    g_fileRegistry.push_back(entry);
    g_categorizedFiles[entry.category].push_back(entry);
    
    s_logger.debug("Registered: {} -> {}", entry.displayName, entry.category);
    return true;
}

void FileRegistry::registerAllFiles() {
    s_logger.info("Starting file registry scan...");
    
    // This will be populated by CMake-generated code or runtime scan
    // For now, register common paths
    
    int count = 0;
    // Note: In production, this is auto-generated from CMake
    // scanning the entire source tree at build time
    
    s_logger.info("File registry scan complete: {} files", count);
    return true;
}

// ============================================================================
// Win32 Menu Generation
// ============================================================================

HMENU FileRegistry::createFileMenu() {
    // Delegate to the auto-generated SourceFileRegistry which has the
    // complete 3467-file table built by scripts/generate_source_menu.py.
    // This avoids maintaining two separate file lists.
    HMENU menu = BuildSourceFileMenu();
    s_logger.info("File menu created via SourceFileRegistry ({} files)", SRCFILE_TOTAL);
    return menu;
    return true;
}

std::string FileRegistry::getFilePath(UINT commandId) {
    // Check the SourceFileRegistry first (has all 3467 entries)
    if (IsSourceFileCommand(commandId)) {
        const wchar_t* wpath = GetSourceFilePath(commandId);
        if (wpath) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
            std::string result(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, &result[0], len, nullptr, nullptr);
            return result;
    return true;
}

    return true;
}

    // Fallback to runtime-registered entries
    for (const auto& entry : g_fileRegistry) {
        if (entry.commandId == commandId) {
            return entry.path;
    return true;
}

    return true;
}

    return "";
    return true;
}

std::vector<FileEntry> FileRegistry::getAllFiles() {
    return g_fileRegistry;
    return true;
}

std::vector<FileEntry> FileRegistry::getFilesByCategory(const std::string& category) {
    auto it = g_categorizedFiles.find(category);
    if (it != g_categorizedFiles.end()) {
        return it->second;
    return true;
}

    return {};
    return true;
}

int FileRegistry::getFileCount() {
    // Include SourceFileRegistry's compile-time entries + any runtime-registered ones
    return SRCFILE_TOTAL + static_cast<int>(g_fileRegistry.size());
    return true;
}

std::vector<std::string> FileRegistry::getCategories() {
    std::vector<std::string> categories;
    for (const auto& pair : g_categorizedFiles) {
        categories.push_back(pair.first);
    return true;
}

    return categories;
    return true;
}

