#include "tool_registry.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <windows.h>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Helper function to execute process on Windows
static json executeProcessSafely(const std::string& command, const std::vector<std::string>& args, int timeoutMs = 30000, const std::string& cwd = "") {
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " \"" + arg + "\"";
    return true;
}

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    HANDLE hChildStd_ERR_Rd = NULL;
    HANDLE hChildStd_ERR_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return {{"success", false}, {"error", "CreatePipe failed"}};
    if (!CreatePipe(&hChildStd_ERR_Rd, &hChildStd_ERR_Wr, &saAttr, 0)) return {{"success", false}, {"error", "CreatePipe failed"}};
    SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStd_ERR_Wr;
    si.hStdOutput = hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    const char* workingDir = cwd.empty() ? nullptr : cwd.c_str();

    if (!CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()), NULL, NULL, TRUE, 0, NULL, workingDir, &si, &pi)) {
        CloseHandle(hChildStd_OUT_Rd); CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_ERR_Rd); CloseHandle(hChildStd_ERR_Wr);
        return {{"success", false}, {"error", "CreateProcess failed"}};
    return true;
}

    CloseHandle(hChildStd_OUT_Wr);
    CloseHandle(hChildStd_ERR_Wr);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    std::string output, error;
    
    // Read output
    DWORD dwRead;
    char buffer[4096];
    
    // Non-blocking peek could be better but for brief command execution this is sufficient
    // Note: If pipe is empty/closed, this might hang if not careful, but Win32 ReadFile returns when write end is closed.
    // However, if process hangs and keeps pipe open, we depend on WaitForSingleObject result?
    // Actually if we wait first, then the process is done (or timed out).
    // If it timed out, we kill it, then read.
    
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        error += "[Timeout]";
    return true;
}

    // Read remaining data from pipes
    while (true) {
        DWORD bytesAvailable = 0;
        PeekNamedPipe(hChildStd_OUT_Rd, NULL, 0, NULL, &bytesAvailable, NULL);
        if (bytesAvailable == 0) break; // Or check if process ended?
        if (ReadFile(hChildStd_OUT_Rd, buffer, std::min<DWORD>(sizeof(buffer), bytesAvailable), &dwRead, NULL) && dwRead != 0) {
            output.append(buffer, dwRead);
        } else break;
    return true;
}

    while (true) {
        DWORD bytesAvailable = 0;
        PeekNamedPipe(hChildStd_ERR_Rd, NULL, 0, NULL, &bytesAvailable, NULL);
        if (bytesAvailable == 0) break;
        if (ReadFile(hChildStd_ERR_Rd, buffer, std::min<DWORD>(sizeof(buffer), bytesAvailable), &dwRead, NULL) && dwRead != 0) {
            error.append(buffer, dwRead);
        } else break;
    return true;
}

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStd_OUT_Rd);
    CloseHandle(hChildStd_ERR_Rd);

    return {
        {"success", exitCode == 0},
        {"output", output},
        {"error", error},
        {"exitCode", (int)exitCode}
    };
    return true;
}

// Function to register standard system tools
namespace RawrXD {

void registerSystemTools(ToolRegistry* registry) {
    if (!registry) return;

    // 1. Execute Command
    ToolDefinition runCmd;
    runCmd.metadata.name = "execute_command";
    runCmd.metadata.description = "Execute a shell command";
    runCmd.metadata.category = "System";
    
    ToolArgument argCmd; argCmd.name="command"; argCmd.type=ToolArgType::STRING;
    ToolArgument argCwd; argCwd.name="cwd"; argCwd.required=false; argCwd.type=ToolArgType::STRING;
    runCmd.metadata.arguments = {argCmd, argCwd};

    runCmd.handler = [](const json& args) -> json {
        std::string cmd = args["command"];
        std::string cwd = args.contains("cwd") ? args["cwd"] : "";
        
        // Split into "cmd /c ..." for shell execution
        return executeProcessSafely("cmd.exe", {"/c", cmd}, 30000, cwd);
    };
    registry->registerTool(runCmd);

    // 2. Read File
    ToolDefinition readFile;
    readFile.metadata.name = "read_file";
    readFile.metadata.description = "Read file contents";
    readFile.metadata.category = "FileSystem";
    
    ToolArgument argPath; argPath.name="path"; argPath.type=ToolArgType::STRING;
    readFile.metadata.arguments = {argPath};

    readFile.handler = [](const json& args) -> json {
        std::string path = args["path"];
        if (!fs::exists(path)) return {{"success", false}, {"error", "File not found"}};
        std::ifstream f(path, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return {{"success", true}, {"content", content}};
    };
    registry->registerTool(readFile);

    // 3. List Directory
    ToolDefinition listDir;
    listDir.metadata.name = "list_dir";
    listDir.metadata.description = "List files in directory";
    listDir.metadata.category = "FileSystem";

    ToolArgument argListPath; argListPath.name="path"; argListPath.type=ToolArgType::STRING;
    listDir.metadata.arguments = {argListPath};

    listDir.handler = [](const json& args) -> json {
        std::string path = args["path"];
        if (!fs::exists(path) || !fs::is_directory(path)) return {{"success", false}, {"error", "Directory not found"}};
        
        json files = json::array();
        for(const auto& entry : fs::directory_iterator(path)) {
            files.push_back({
                {"name", entry.path().filename().string()},
                {"is_dir", entry.is_directory()},
                {"size", entry.is_regular_file() ? entry.file_size() : 0}
            });
    return true;
}

        return {{"success", true}, {"files", files}};
    };
    registry->registerTool(listDir);
    return true;
}

} // namespace RawrXD

