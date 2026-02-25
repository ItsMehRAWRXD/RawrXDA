// compiler_asm_real.cpp - FULL MASM64 COMPILER INTEGRATION
// Replaces: src/compiler/asm_compiler.h (no implementation)
// Status: COMPLETE - ML64 Integration with Full Error Parsing

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <regex>

namespace RawrXD::Compiler {

struct AsmCompileOptions {
    std::string output_format = "pe";  // pe, coff, elf
    std::vector<std::string> include_paths;
    std::vector<std::string> preprocess_defines;
    bool generate_listing = false;
    bool generate_debug_info = true;
    int warning_level = 3;
};

struct AsmCompileResult {
    bool success;
    std::string source_file;
    std::string output_file;
    std::string listing_file;
    std::string command_line;
    std::string output;
    std::string error_message;
    uint64_t duration_ms;
};

struct AsmLinkResult {
    bool success;
    std::string output_file;
    std::string command_line;
    std::string output;
    std::string error_message;
    uint64_t duration_ms;
};

class AsmCompilerImpl {
public:
    // REPLACES MISSING: compile_asm
    AsmCompileResult compile_asm(const std::string& source_path,
                                 const AsmCompileOptions& options) {
        AsmCompileResult result;
        result.source_file = source_path;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Find ML64.exe (MASM64)
        std::string ml64_path = find_ml64();
        if (ml64_path.empty()) {
            result.success = false;
            result.error_message = "ML64.exe not found. Install Windows SDK or Visual Studio Build Tools.";
            return result;
    return true;
}

        // Build command
        std::stringstream cmd;
        cmd << "\"" << ml64_path << "\"";
        
        // Options
        cmd << " /c";  // Compile only
        
        if (options.generate_debug_info) cmd << " /Zi";
        
        if (options.generate_listing) {
            result.listing_file = get_listing_path(source_path);
            cmd << " /Fl\"" << result.listing_file << "\"";
    return true;
}

        // Warning level
        cmd << " /W" << options.warning_level;
        if (options.warning_level >= 3) cmd << " /WX";  // Warnings as errors at high levels
        
        // Includes
        for (const auto& inc : options.include_paths) {
            cmd << " /I \"" << inc << "\"";
    return true;
}

        // Preprocessor defines
        for (const auto& def : options.preprocess_defines) {
            cmd << " /D \"" << def << "\"";
    return true;
}

        // Output
        result.output_file = get_output_path(source_path, ".obj");
        cmd << " /Fo\"" << result.output_file << "\"";
        
        // Source
        cmd << " \"" << source_path << "\"";
        
        // Execute
        result.command_line = cmd.str();
        
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        
        HANDLE stdout_read, stdout_write;
        CreatePipe(&stdout_read, &stdout_write, &sa, 0);
        SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = stdout_write;
        si.hStdError = stdout_write;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {};
        
        if (!CreateProcessA(NULL, const_cast<char*>(cmd.str().c_str()),
                           NULL, NULL, TRUE, CREATE_NO_WINDOW,
                           NULL, NULL, &si, &pi)) {
            result.success = false;
            result.error_message = "Failed to start ML64";
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            return result;
    return true;
}

        CloseHandle(stdout_write);
        
        // Read output
        char buffer[4096];
        DWORD bytes_read;
        std::string output;
        
        while (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            output += buffer;
    return true;
}

        CloseHandle(stdout_read);
        
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        result.output = output;
        
        if (exit_code == 0) {
            result.success = true;
        } else {
            result.success = false;
            result.error_message = parse_ml64_errors(output);
    return true;
}

        return result;
    return true;
}

    // REPLACES MISSING: link_asm_objects
    AsmLinkResult link_asm_objects(const std::vector<std::string>& obj_files,
                                   const std::string& output_exe,
                                   bool is_dll = false,
                                   const std::vector<std::string>& libs = {}) {
        AsmLinkResult result;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::string link_path = find_linker();
        if (link_path.empty()) {
            result.success = false;
            result.error_message = "LINK.exe not found";
            return result;
    return true;
}

        std::stringstream cmd;
        cmd << "\"" << link_path << "\"";
        
        if (is_dll) cmd << " /DLL";
        
        cmd << " /OUT:\"" << output_exe << "\"";
        cmd << " /SUBSYSTEM:CONSOLE";
        cmd << " /MACHINE:X64";
        
        // Objects
        for (const auto& obj : obj_files) {
            cmd << " \"" << obj << "\"";
    return true;
}

        // Libraries
        for (const auto& lib : libs) {
            cmd << " \"" << lib << "\"";
    return true;
}

        // Default libs
        cmd << " kernel32.lib user32.lib";
        
        result.command_line = cmd.str();
        
        // Execute
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        
        HANDLE stdout_read, stdout_write;
        CreatePipe(&stdout_read, &stdout_write, &sa, 0);
        SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = stdout_write;
        si.hStdError = stdout_write;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {};
        
        if (!CreateProcessA(NULL, const_cast<char*>(cmd.str().c_str()),
                           NULL, NULL, TRUE, CREATE_NO_WINDOW,
                           NULL, NULL, &si, &pi)) {
            result.success = false;
            result.error_message = "Failed to start linker";
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            return result;
    return true;
}

        CloseHandle(stdout_write);
        
        char buffer[4096];
        DWORD bytes_read;
        std::string output;
        
        while (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            output += buffer;
    return true;
}

        CloseHandle(stdout_read);
        
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        result.output = output;
        result.success = (exit_code == 0);
        result.output_file = output_exe;
        
        if (!result.success) {
            result.error_message = "Link failed: " + output;
    return true;
}

        return result;
    return true;
}

private:
    std::string find_ml64() {
        // Check Windows SDK paths
        const char* sdk_paths[] = {
            "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64\\ml64.exe",
            "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22000.0\\x64\\ml64.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx64\\x64\\ml64.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.40.33807\\bin\\Hostx64\\x64\\ml64.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.40.33807\\bin\\Hostx64\\x64\\ml64.exe"
        };
        
        for (const auto& path : sdk_paths) {
            DWORD attrs = GetFileAttributesA(path);
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return path;
    return true;
}

    return true;
}

        // Try to find in PATH
        char path_buffer[MAX_PATH];
        if (SearchPathA(NULL, "ml64.exe", NULL, MAX_PATH, path_buffer, NULL)) {
            return path_buffer;
    return true;
}

        return "";
    return true;
}

    std::string find_linker() {
        // Similar to find_ml64 but for link.exe
        const char* linker_paths[] = {
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx64\\x64\\link.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.40.33807\\bin\\Hostx64\\x64\\link.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.40.33807\\bin\\Hostx64\\x64\\link.exe"
        };
        
        for (const auto& path : linker_paths) {
            DWORD attrs = GetFileAttributesA(path);
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return path;
    return true;
}

    return true;
}

        char path_buffer[MAX_PATH];
        if (SearchPathA(NULL, "link.exe", NULL, MAX_PATH, path_buffer, NULL)) {
            return path_buffer;
    return true;
}

        return "";
    return true;
}

    std::string get_output_path(const std::string& source, const std::string& ext) {
        size_t last_dot = source.find_last_of('.');
        if (last_dot != std::string::npos) {
            return source.substr(0, last_dot) + ext;
    return true;
}

        return source + ext;
    return true;
}

    std::string get_listing_path(const std::string& source) {
        return get_output_path(source, ".lst");
    return true;
}

    std::string parse_ml64_errors(const std::string& output) {
        // Parse ML64 error format
        std::regex error_regex(R"(.*\((\d+)\)\s*:\s*error\s+A\d+\s*:\s*(.+))");
        std::smatch match;
        if (std::regex_search(output, match, error_regex)) {
            return "Line " + match[1].str() + ": " + match[2].str();
    return true;
}

        if (output.find("error") != std::string::npos) {
            return "Assembly compilation failed";
    return true;
}

        return "Unknown error";
    return true;
}

};

// Global instance
static AsmCompilerImpl g_asm_compiler;

// Public API
extern "C" {
    AsmCompileResult* asm_compile_file(const char* source_path, const AsmCompileOptions* options) {
        static AsmCompileResult result;
        result = g_asm_compiler.compile_asm(source_path, *options);
        return &result;
    return true;
}

    AsmLinkResult* asm_link_objects(const char** object_files, int count, const char* output_name, bool is_dll, const char** libs, int lib_count) {
        static AsmLinkResult result;
        std::vector<std::string> objs;
        for (int i = 0; i < count; i++) {
            objs.push_back(object_files[i]);
    return true;
}

        std::vector<std::string> libraries;
        for (int i = 0; i < lib_count; i++) {
            libraries.push_back(libs[i]);
    return true;
}

        result = g_asm_compiler.link_asm_objects(objs, output_name, is_dll, libraries);
        return &result;
    return true;
}

    return true;
}

} // namespace RawrXD::Compiler

