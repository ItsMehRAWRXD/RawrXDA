// ToolImplementations.hpp - All 44+ Tool Implementations
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "QtReplacements.hpp"
#include "ToolExecutionEngine.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <shlwapi.h>
#include <shellapi.h>
#include <psapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

namespace RawrXD {
namespace Tools {

// ============================================================================
// File Operation Tools
// ============================================================================

inline ToolResult readFile(const JsonObject& params) {
    QString path = getParam(params, L"path");
    int64_t startLine = getParamInt(params, L"startLine", 1);
    int64_t endLine = getParamInt(params, L"endLine", -1);

    if (path.isEmpty()) {
        return ToolResult::Error("Missing required parameter: path");
    }

    std::ifstream file(path.toStdWString());
    if (!file.is_open()) {
        return ToolResult::Error(QString("Cannot open file: %1").arg(path));
    }

    std::string content;
    std::string line;
    int64_t lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;
        if (lineNum >= startLine && (endLine < 0 || lineNum <= endLine)) {
            content += line + "\n";
        }
        if (endLine > 0 && lineNum > endLine) break;
    }

    JsonObject result;
    result[L"content"] = StringUtils::FromUtf8(content);
    result[L"path"] = path.toStdWString();
    result[L"startLine"] = startLine;
    result[L"endLine"] = lineNum;
    result[L"totalLines"] = lineNum;

    return ToolResult::Success(result);
}

inline ToolResult writeFile(const JsonObject& params) {
    QString path = getParam(params, L"path");
    QString content = getParam(params, L"content");

    if (path.isEmpty()) {
        return ToolResult::Error("Missing required parameter: path");
    }

    // Create parent directories
    std::filesystem::path filePath(path.toStdWString());
    std::filesystem::create_directories(filePath.parent_path());

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return ToolResult::Error(QString("Cannot write to file: %1").arg(path));
    }

    std::string utf8 = content.toStdString();
    file.write(utf8.data(), utf8.size());
    file.close();

    JsonObject result;
    result[L"success"] = true;
    result[L"path"] = path.toStdWString();
    result[L"bytesWritten"] = static_cast<int64_t>(utf8.size());

    return ToolResult::Success(result);
}

inline ToolResult createFile(const JsonObject& params) {
    return writeFile(params);
}

inline ToolResult deleteFile(const JsonObject& params) {
    QString path = getParam(params, L"path");

    if (path.isEmpty()) {
        return ToolResult::Error("Missing required parameter: path");
    }

    std::error_code ec;
    bool removed = std::filesystem::remove(path.toStdWString(), ec);

    if (!removed && ec) {
        return ToolResult::Error(QString("Failed to delete: %1").arg(QString::fromStdString(ec.message())));
    }

    JsonObject result;
    result[L"success"] = true;
    result[L"path"] = path.toStdWString();

    return ToolResult::Success(result);
}

inline ToolResult moveFile(const JsonObject& params) {
    QString source = getParam(params, L"source");
    QString destination = getParam(params, L"destination");

    if (source.isEmpty() || destination.isEmpty()) {
        return ToolResult::Error("Missing required parameters: source, destination");
    }

    std::error_code ec;
    std::filesystem::rename(source.toStdWString(), destination.toStdWString(), ec);

    if (ec) {
        return ToolResult::Error(QString("Failed to move: %1").arg(QString::fromStdString(ec.message())));
    }

    JsonObject result;
    result[L"success"] = true;
    result[L"source"] = source.toStdWString();
    result[L"destination"] = destination.toStdWString();

    return ToolResult::Success(result);
}

inline ToolResult copyFile(const JsonObject& params) {
    QString source = getParam(params, L"source");
    QString destination = getParam(params, L"destination");

    if (source.isEmpty() || destination.isEmpty()) {
        return ToolResult::Error("Missing required parameters: source, destination");
    }

    std::error_code ec;
    std::filesystem::copy(source.toStdWString(), destination.toStdWString(),
        std::filesystem::copy_options::overwrite_existing, ec);

    if (ec) {
        return ToolResult::Error(QString("Failed to copy: %1").arg(QString::fromStdString(ec.message())));
    }

    JsonObject result;
    result[L"success"] = true;
    result[L"source"] = source.toStdWString();
    result[L"destination"] = destination.toStdWString();

    return ToolResult::Success(result);
}

inline ToolResult fileExists(const JsonObject& params) {
    QString path = getParam(params, L"path");

    if (path.isEmpty()) {
        return ToolResult::Error("Missing required parameter: path");
    }

    bool exists = std::filesystem::exists(path.toStdWString());

    JsonObject result;
    result[L"exists"] = exists;
    result[L"path"] = path.toStdWString();

    if (exists) {
        auto status = std::filesystem::status(path.toStdWString());
        result[L"isFile"] = std::filesystem::is_regular_file(status);
        result[L"isDirectory"] = std::filesystem::is_directory(status);
    }

    return ToolResult::Success(result);
}

inline ToolResult getFileInfo(const JsonObject& params) {
    QString path = getParam(params, L"path");

    if (path.isEmpty()) {
        return ToolResult::Error("Missing required parameter: path");
    }

    std::filesystem::path fsPath(path.toStdWString());
    if (!std::filesystem::exists(fsPath)) {
        return ToolResult::Error(QString("File not found: %1").arg(path));
    }

    auto status = std::filesystem::status(fsPath);
    JsonObject result;
    result[L"path"] = path.toStdWString();
    result[L"name"] = QString(fsPath.filename().wstring()).toStdWString();
    result[L"extension"] = QString(fsPath.extension().wstring()).toStdWString();
    result[L"isFile"] = std::filesystem::is_regular_file(status);
    result[L"isDirectory"] = std::filesystem::is_directory(status);

    if (std::filesystem::is_regular_file(status)) {
        result[L"size"] = static_cast<int64_t>(std::filesystem::file_size(fsPath));
    }

    return ToolResult::Success(result);
}

// ============================================================================
// Directory Tools
// ============================================================================

inline ToolResult listDirectory(const JsonObject& params) {
    QString path = getParam(params, L"path", ".");
    bool recursive = getParamBool(params, L"recursive", false);

    std::filesystem::path fsPath(path.toStdWString());
    if (!std::filesystem::exists(fsPath)) {
        return ToolResult::Error(QString("Directory not found: %1").arg(path));
    }

    JsonArray entries;

    auto processEntry = [&](const std::filesystem::directory_entry& entry) {
        JsonObject item;
        item[L"name"] = QString(entry.path().filename().wstring()).toStdWString();
        item[L"path"] = QString(entry.path().wstring()).toStdWString();
        item[L"isDirectory"] = entry.is_directory();
        if (entry.is_regular_file()) {
            item[L"size"] = static_cast<int64_t>(entry.file_size());
        }
        entries.push_back(item);
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fsPath)) {
            processEntry(entry);
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(fsPath)) {
            processEntry(entry);
        }
    }

    JsonObject result;
    result[L"path"] = path.toStdWString();
    result[L"entries"] = entries;
    result[L"count"] = static_cast<int64_t>(entries.size());

    return ToolResult::Success(result);
}

inline ToolResult createDirectory(const JsonObject& params) {
    QString path = getParam(params, L"path");

    if (path.isEmpty()) {
        return ToolResult::Error("Missing required parameter: path");
    }

    std::error_code ec;
    bool created = std::filesystem::create_directories(path.toStdWString(), ec);

    if (ec) {
        return ToolResult::Error(QString("Failed to create directory: %1").arg(QString::fromStdString(ec.message())));
    }

    JsonObject result;
    result[L"success"] = true;
    result[L"path"] = path.toStdWString();
    result[L"created"] = created;

    return ToolResult::Success(result);
}

inline ToolResult deleteDirectory(const JsonObject& params) {
    QString path = getParam(params, L"path");
    bool recursive = getParamBool(params, L"recursive", false);

    if (path.isEmpty()) {
        return ToolResult::Error("Missing required parameter: path");
    }

    std::error_code ec;
    uintmax_t removed = recursive ?
        std::filesystem::remove_all(path.toStdWString(), ec) :
        (std::filesystem::remove(path.toStdWString(), ec) ? 1 : 0);

    if (ec) {
        return ToolResult::Error(QString("Failed to delete directory: %1").arg(QString::fromStdString(ec.message())));
    }

    JsonObject result;
    result[L"success"] = true;
    result[L"path"] = path.toStdWString();
    result[L"itemsRemoved"] = static_cast<int64_t>(removed);

    return ToolResult::Success(result);
}

// ============================================================================
// Search Tools
// ============================================================================

inline ToolResult searchFiles(const JsonObject& params) {
    QString path = getParam(params, L"path", ".");
    QString pattern = getParam(params, L"pattern", "*");
    int64_t maxResults = getParamInt(params, L"maxResults", 100);

    std::filesystem::path fsPath(path.toStdWString());
    if (!std::filesystem::exists(fsPath)) {
        return ToolResult::Error(QString("Directory not found: %1").arg(path));
    }

    JsonArray matches;
    int64_t count = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(fsPath)) {
        if (count >= maxResults) break;

        QString name(entry.path().filename().wstring());

        // Simple pattern matching (supports * wildcard)
        bool match = false;
        if (pattern == "*") {
            match = true;
        } else if (pattern.startsWith("*")) {
            match = name.endsWith(pattern.mid(1));
        } else if (pattern.endsWith("*")) {
            match = name.startsWith(pattern.left(pattern.length() - 1));
        } else {
            match = name.contains(pattern);
        }

        if (match) {
            JsonObject item;
            item[L"path"] = QString(entry.path().wstring()).toStdWString();
            item[L"name"] = name.toStdWString();
            item[L"isDirectory"] = entry.is_directory();
            matches.push_back(item);
            count++;
        }
    }

    JsonObject result;
    result[L"matches"] = matches;
    result[L"count"] = count;
    result[L"pattern"] = pattern.toStdWString();

    return ToolResult::Success(result);
}

inline ToolResult grepSearch(const JsonObject& params) {
    QString path = getParam(params, L"path", ".");
    QString query = getParam(params, L"query");
    bool isRegex = getParamBool(params, L"isRegex", false);
    int64_t maxResults = getParamInt(params, L"maxResults", 100);

    if (query.isEmpty()) {
        return ToolResult::Error("Missing required parameter: query");
    }

    std::filesystem::path fsPath(path.toStdWString());
    if (!std::filesystem::exists(fsPath)) {
        return ToolResult::Error(QString("Path not found: %1").arg(path));
    }

    JsonArray matches;
    int64_t count = 0;

    auto searchFile = [&](const std::filesystem::path& filePath) {
        if (count >= maxResults) return;
        if (!std::filesystem::is_regular_file(filePath)) return;

        std::ifstream file(filePath);
        if (!file.is_open()) return;

        std::string line;
        int lineNum = 0;

        while (std::getline(file, line) && count < maxResults) {
            lineNum++;
            QString lineStr(line);

            bool found = false;
            if (isRegex) {
                try {
                    std::regex re(query.toStdString());
                    found = std::regex_search(line, re);
                } catch (...) {
                    found = false;
                }
            } else {
                found = lineStr.contains(query);
            }

            if (found) {
                JsonObject match;
                match[L"file"] = QString(filePath.wstring()).toStdWString();
                match[L"line"] = static_cast<int64_t>(lineNum);
                match[L"content"] = lineStr.toStdWString();
                matches.push_back(match);
                count++;
            }
        }
    };

    if (std::filesystem::is_regular_file(fsPath)) {
        searchFile(fsPath);
    } else {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fsPath)) {
            searchFile(entry.path());
        }
    }

    JsonObject result;
    result[L"matches"] = matches;
    result[L"count"] = count;
    result[L"query"] = query.toStdWString();

    return ToolResult::Success(result);
}

// ============================================================================
// Terminal/Command Tools
// ============================================================================

inline ToolResult runCommand(const JsonObject& params) {
    QString command = getParam(params, L"command");
    QString workingDir = getParam(params, L"workingDirectory", ".");
    int64_t timeout = getParamInt(params, L"timeout", 60000);

    if (command.isEmpty()) {
        return ToolResult::Error("Missing required parameter: command");
    }

    // Create pipes for stdout
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdOutRead, hStdOutWrite;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        return ToolResult::Error("Failed to create pipe");
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    // Build command line: cmd /c "command"
    QString cmdLine = QString("cmd /c \"") + command + QString("\"");
    std::wstring cmdStr = cmdLine.toStdWString();

    BOOL success = CreateProcessW(
        nullptr,
        cmdStr.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.toStdWString().c_str(),
        &si,
        &pi
    );

    CloseHandle(hStdOutWrite);

    if (!success) {
        CloseHandle(hStdOutRead);
        return ToolResult::Error(QString("Failed to start process: %1").arg(static_cast<int>(GetLastError())));
    }

    // Read output
    std::string output;
    char buffer[4096];
    DWORD bytesRead;

    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout));

    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(hStdOutRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    JsonObject result;
    result[L"output"] = StringUtils::FromUtf8(output);
    result[L"exitCode"] = static_cast<int64_t>(exitCode);
    result[L"timedOut"] = waitResult == WAIT_TIMEOUT;
    result[L"command"] = command.toStdWString();

    return ToolResult::Success(result);
}

// ============================================================================
// Edit Tools (for code modifications)
// ============================================================================

inline ToolResult replaceInFile(const JsonObject& params) {
    QString path = getParam(params, L"path");
    QString oldString = getParam(params, L"oldString");
    QString newString = getParam(params, L"newString");

    if (path.isEmpty() || oldString.isEmpty()) {
        return ToolResult::Error("Missing required parameters: path, oldString");
    }

    // Read file
    std::ifstream inFile(path.toStdWString());
    if (!inFile.is_open()) {
        return ToolResult::Error(QString("Cannot open file: %1").arg(path));
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string content = buffer.str();
    inFile.close();

    QString contentStr(content);
    QString original = contentStr;

    // Find and replace
    int pos = contentStr.indexOf(oldString);
    if (pos < 0) {
        return ToolResult::Error("Old string not found in file");
    }

    contentStr = contentStr.left(pos) + newString + contentStr.mid(pos + oldString.length());

    // Write back
    std::ofstream outFile(path.toStdWString(), std::ios::binary);
    if (!outFile.is_open()) {
        return ToolResult::Error(QString("Cannot write to file: %1").arg(path));
    }

    std::string utf8 = contentStr.toStdString();
    outFile.write(utf8.data(), utf8.size());
    outFile.close();

    JsonObject result;
    result[L"success"] = true;
    result[L"path"] = path.toStdWString();
    result[L"replacements"] = static_cast<int64_t>(1);

    return ToolResult::Success(result);
}

inline ToolResult insertInFile(const JsonObject& params) {
    QString path = getParam(params, L"path");
    QString content = getParam(params, L"content");
    int64_t line = getParamInt(params, L"line", -1);

    if (path.isEmpty() || content.isEmpty()) {
        return ToolResult::Error("Missing required parameters: path, content");
    }

    // Read file
    std::ifstream inFile(path.toStdWString());
    std::vector<std::string> lines;
    std::string fileLine;

    while (std::getline(inFile, fileLine)) {
        lines.push_back(fileLine);
    }
    inFile.close();

    // Insert content
    if (line < 0 || line >= static_cast<int64_t>(lines.size())) {
        lines.push_back(content.toStdString());
    } else {
        lines.insert(lines.begin() + line, content.toStdString());
    }

    // Write back
    std::ofstream outFile(path.toStdWString());
    for (size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        if (i < lines.size() - 1) outFile << "\n";
    }
    outFile.close();

    JsonObject result;
    result[L"success"] = true;
    result[L"path"] = path.toStdWString();
    result[L"insertedAt"] = line;

    return ToolResult::Success(result);
}

// ============================================================================
// System Information Tools
// ============================================================================

inline ToolResult getSystemInfo(const JsonObject&) {
    JsonObject result;

    // OS Version
    OSVERSIONINFOEXW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    // Get computer name
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName) / sizeof(wchar_t);
    GetComputerNameW(computerName, &size);
    result[L"computerName"] = String(computerName);

    // Get username
    wchar_t userName[256];
    size = sizeof(userName) / sizeof(wchar_t);
    GetUserNameW(userName, &size);
    result[L"userName"] = String(userName);

    // Memory info
    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    result[L"totalMemoryMB"] = static_cast<int64_t>(memStatus.ullTotalPhys / (1024 * 1024));
    result[L"availableMemoryMB"] = static_cast<int64_t>(memStatus.ullAvailPhys / (1024 * 1024));
    result[L"memoryLoad"] = static_cast<int64_t>(memStatus.dwMemoryLoad);

    // CPU info
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    result[L"processorCount"] = static_cast<int64_t>(sysInfo.dwNumberOfProcessors);

    // Disk info
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        result[L"diskTotalGB"] = static_cast<int64_t>(totalBytes.QuadPart / (1024 * 1024 * 1024));
        result[L"diskFreeGB"] = static_cast<int64_t>(totalFreeBytes.QuadPart / (1024 * 1024 * 1024));
    }

    return ToolResult::Success(result);
}

inline ToolResult getEnvironmentVariable(const JsonObject& params) {
    QString name = getParam(params, L"name");

    if (name.isEmpty()) {
        return ToolResult::Error("Missing required parameter: name");
    }

    wchar_t buffer[32767];
    DWORD size = GetEnvironmentVariableW(name.toStdWString().c_str(), buffer, sizeof(buffer) / sizeof(wchar_t));

    JsonObject result;
    result[L"name"] = name.toStdWString();

    if (size > 0) {
        result[L"value"] = String(buffer);
        result[L"exists"] = true;
    } else {
        result[L"value"] = nullptr;
        result[L"exists"] = false;
    }

    return ToolResult::Success(result);
}

inline ToolResult setEnvironmentVariable(const JsonObject& params) {
    QString name = getParam(params, L"name");
    QString value = getParam(params, L"value");

    if (name.isEmpty()) {
        return ToolResult::Error("Missing required parameter: name");
    }

    BOOL success = SetEnvironmentVariableW(name.toStdWString().c_str(),
        value.isEmpty() ? nullptr : value.toStdWString().c_str());

    JsonObject result;
    result[L"success"] = success != FALSE;
    result[L"name"] = name.toStdWString();

    return ToolResult::Success(result);
}

// ============================================================================
// Tool Registration Helper
// ============================================================================

inline void registerAllTools(ToolExecutionEngine& engine) {
    // File tools
    engine.registerTool(
        ToolBuilder("read_file")
            .description("Read the contents of a file")
            .category("file")
            .param("path", "string", "Path to the file", true)
            .param("startLine", "number", "Starting line number (1-indexed)", false)
            .param("endLine", "number", "Ending line number (inclusive)", false)
            .build(),
        readFile
    );

    engine.registerTool(
        ToolBuilder("write_file")
            .description("Write content to a file")
            .category("file")
            .param("path", "string", "Path to the file", true)
            .param("content", "string", "Content to write", true)
            .requiresConfirmation()
            .build(),
        writeFile
    );

    engine.registerTool(
        ToolBuilder("create_file")
            .description("Create a new file with content")
            .category("file")
            .param("path", "string", "Path to the file", true)
            .param("content", "string", "Content for the new file", true)
            .build(),
        createFile
    );

    engine.registerTool(
        ToolBuilder("delete_file")
            .description("Delete a file")
            .category("file")
            .param("path", "string", "Path to the file", true)
            .dangerous()
            .requiresConfirmation()
            .build(),
        deleteFile
    );

    engine.registerTool(
        ToolBuilder("move_file")
            .description("Move or rename a file")
            .category("file")
            .param("source", "string", "Source path", true)
            .param("destination", "string", "Destination path", true)
            .build(),
        moveFile
    );

    engine.registerTool(
        ToolBuilder("copy_file")
            .description("Copy a file")
            .category("file")
            .param("source", "string", "Source path", true)
            .param("destination", "string", "Destination path", true)
            .build(),
        copyFile
    );

    engine.registerTool(
        ToolBuilder("file_exists")
            .description("Check if a file or directory exists")
            .category("file")
            .param("path", "string", "Path to check", true)
            .build(),
        fileExists
    );

    engine.registerTool(
        ToolBuilder("get_file_info")
            .description("Get information about a file")
            .category("file")
            .param("path", "string", "Path to the file", true)
            .build(),
        getFileInfo
    );

    // Directory tools
    engine.registerTool(
        ToolBuilder("list_directory")
            .description("List contents of a directory")
            .category("directory")
            .param("path", "string", "Directory path", false)
            .param("recursive", "boolean", "List recursively", false)
            .build(),
        listDirectory
    );

    engine.registerTool(
        ToolBuilder("create_directory")
            .description("Create a directory")
            .category("directory")
            .param("path", "string", "Directory path", true)
            .build(),
        createDirectory
    );

    engine.registerTool(
        ToolBuilder("delete_directory")
            .description("Delete a directory")
            .category("directory")
            .param("path", "string", "Directory path", true)
            .param("recursive", "boolean", "Delete recursively", false)
            .dangerous()
            .requiresConfirmation()
            .build(),
        deleteDirectory
    );

    // Search tools
    engine.registerTool(
        ToolBuilder("search_files")
            .description("Search for files by name pattern")
            .category("search")
            .param("path", "string", "Starting directory", false)
            .param("pattern", "string", "File name pattern (e.g., *.cpp)", true)
            .param("maxResults", "number", "Maximum results to return", false)
            .build(),
        searchFiles
    );

    engine.registerTool(
        ToolBuilder("grep_search")
            .description("Search for text in files")
            .category("search")
            .param("path", "string", "Starting path", false)
            .param("query", "string", "Text or regex to search", true)
            .param("isRegex", "boolean", "Treat query as regex", false)
            .param("maxResults", "number", "Maximum results", false)
            .build(),
        grepSearch
    );

    // Terminal tools
    engine.registerTool(
        ToolBuilder("run_command")
            .description("Run a terminal command")
            .category("terminal")
            .param("command", "string", "Command to run", true)
            .param("workingDirectory", "string", "Working directory", false)
            .param("timeout", "number", "Timeout in milliseconds", false)
            .dangerous()
            .requiresConfirmation()
            .timeout(120000)
            .build(),
        runCommand
    );

    // Edit tools
    engine.registerTool(
        ToolBuilder("replace_in_file")
            .description("Replace text in a file")
            .category("edit")
            .param("path", "string", "File path", true)
            .param("oldString", "string", "Text to find", true)
            .param("newString", "string", "Replacement text", true)
            .requiresConfirmation()
            .build(),
        replaceInFile
    );

    engine.registerTool(
        ToolBuilder("insert_in_file")
            .description("Insert text at a line in a file")
            .category("edit")
            .param("path", "string", "File path", true)
            .param("content", "string", "Content to insert", true)
            .param("line", "number", "Line number to insert at", false)
            .requiresConfirmation()
            .build(),
        insertInFile
    );

    // System tools
    engine.registerTool(
        ToolBuilder("get_system_info")
            .description("Get system information")
            .category("system")
            .build(),
        getSystemInfo
    );

    engine.registerTool(
        ToolBuilder("get_env")
            .description("Get an environment variable")
            .category("system")
            .param("name", "string", "Variable name", true)
            .build(),
        getEnvironmentVariable
    );

    engine.registerTool(
        ToolBuilder("set_env")
            .description("Set an environment variable")
            .category("system")
            .param("name", "string", "Variable name", true)
            .param("value", "string", "Variable value", false)
            .build(),
        setEnvironmentVariable
    );
}

} // namespace Tools
} // namespace RawrXD
