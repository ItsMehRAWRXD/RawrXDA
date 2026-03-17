#include "ModelConversionDialog.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <array>
#include <iostream>

// ==========================================
// ProcessHandle Implementation
// ==========================================

ProcessHandle::~ProcessHandle() { 
    close(); 
}

ProcessHandle::ProcessHandle(ProcessHandle&& other) noexcept 
    : m_handle(other.m_handle) { 
    other.m_handle = nullptr; 
}

ProcessHandle& ProcessHandle::operator=(ProcessHandle&& other) noexcept {
    if (this != &other) {
        close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

void ProcessHandle::close() {
    if (isValid()) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
}

// ==========================================
// ModelConversionDialog Implementation
// ==========================================

ModelConversionDialog::ModelConversionDialog(const std::vector<std::string>& unsupportedTypes,
                                             std::string_view recommendedType,
                                             const fs::path& modelPath,
                                             HWND parent,
                                             const ConversionConfig& config)
    : m_unsupportedTypes(unsupportedTypes)
    , m_recommendedType(recommendedType)
    , m_modelPath(modelPath)
    , m_parent(parent)
    , m_config(config)
{
    // Auto-discovery of converter if not provided
    if (m_config.converterPath.empty()) {
        auto [foundPath, error] = findConverter(fs::current_path());
        if (error == ConversionError::None) {
            m_config.converterPath = foundPath;
        }
    }
}

void ModelConversionDialog::log(const std::string& message, bool isError) const {
    if (isError) spdlog::error("[ModelConverter] {}", message);
    else spdlog::info("[ModelConverter] {}", message);
}

ModelConversionDialog::Result ModelConversionDialog::exec() {
    // 1. Validate
    auto [isValid, validErr] = validateModelPath();
    if (!isValid) {
        showErrorDialog("Invalid model path provided.");
        return Result::Failed;
    }

    if (m_config.converterPath.empty()) {
        showErrorDialog("No suitable converter (llama-quantize, python script) found in path.");
        return Result::Failed;
    }

    // 2. Ask Permission
    std::string msg = buildUserMessage();
    if (m_config.showUI && !askUserPermission(msg)) {
        return Result::Cancelled;
    }

    // 3. Prepare Command
    auto [cmdLine, cmdErr] = buildCommandLine();
    if (cmdErr != ConversionError::None) {
        showErrorDialog("Failed to build conversion command.");
        return Result::Failed;
    }

    log("Starting conversion: " + cmdLine);

    // 4. Execute
    auto [procHandle, execErr] = executeConverter(cmdLine);
    if (execErr != ConversionError::None) {
        showErrorDialog("Failed to start converter process. Check logs.");
        return Result::Failed;
    }
    
    m_processHandle = std::move(procHandle);

    // 5. Monitor (Blocking)
    monitorProcess(m_processHandle, m_config.timeout);

    return handleConversionResult();
}

std::string ModelConversionDialog::buildUserMessage() const {
    std::stringstream ss;
    ss << "The model at:\n" << m_modelPath.string() << "\n\n"
       << "Is in a format that requires conversion to " << m_recommendedType << ".\n"
       << "Do you want to attempt conversion now?\n(This may take several minutes)";
    return ss.str();
}

bool ModelConversionDialog::askUserPermission(const std::string& message) const {
    int ret = MessageBoxA(m_parent, message.c_str(), "Model Conversion Required", MB_YESNO | MB_ICONQUESTION);
    return (ret == IDYES);
}

std::pair<std::string, ConversionError> ModelConversionDialog::buildCommandLine() const {
    // Simple logic: If python script, use python. If exe, use directly.
    std::string cmd;
    std::string converter = m_config.converterPath.string();
    
    // Construct output path (e.g., model.pth -> model.gguf)
    fs::path outPath = m_modelPath;
    outPath.replace_extension(m_recommendedType == "GGUF" ? ".gguf" : ".bin");
    
    // Safety Quote paths
    auto quote = [](const std::string& s) { return "\"" + s + "\""; };

    if (converter.find(".py") != std::string::npos) {
        cmd = "python " + quote(converter) + " " + quote(m_modelPath.string()) + " " + quote(outPath.string());
    } else {
        // Assume CLI tool like llama-quantize: app input output data_type
        // Hardcoding sane default "q4_0" for now as per "recommended" implicit context
        cmd = quote(converter) + " " + quote(m_modelPath.string()) + " " + quote(outPath.string()) + " q4_0";
    }

    return {cmd, ConversionError::None};
}

std::pair<ProcessHandle, ConversionError> ModelConversionDialog::executeConverter(const std::string& commandLine) const {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES; // We should redirect stdout/err in a real app
    // For simplicity in this non-async implementation, we let it inherit console or hide it.
    // If we want to capture output, we need pipes.
    
    ZeroMemory(&pi, sizeof(pi));

    // Create pipes would go here for full capture. 
    // For now, we launch.
    
    // Using CreateProcessA
    // Note: commandLine must be mutable for CreateProcess
    std::vector<char> cmdVec(commandLine.begin(), commandLine.end());
    cmdVec.push_back(0);

    if (!CreateProcessA(NULL, cmdVec.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        log("CreateProcess failed. Error: " + std::to_string(GetLastError()), true);
        return {ProcessHandle(), ConversionError::CommandExecutionFailed};
    }

    CloseHandle(pi.hThread);
    return {ProcessHandle(pi.hProcess), ConversionError::None};
}

void ModelConversionDialog::monitorProcess(ProcessHandle& handle, std::chrono::seconds timeout) {
    if (!handle.isValid()) return;
    
    // Wait
    DWORD result = WaitForSingleObject(handle.get(), (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    
    if (result == WAIT_TIMEOUT) {
        log("Conversion timed out.", true);
        TerminateProcess(handle.get(), 999);
        m_conversionResult.success = false;
        m_conversionResult.exitCode = 999;
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(handle.get(), &exitCode);
        m_conversionResult.success = (exitCode == 0);
        m_conversionResult.exitCode = (int)exitCode;
        if (m_conversionResult.success) {
             // Set output path in result
             fs::path outPath = m_modelPath;
             outPath.replace_extension(m_recommendedType == "GGUF" ? ".gguf" : ".bin");
             m_conversionResult.convertedModelPath = outPath;
             log("Conversion successful: " + outPath.string());
        } else {
             log("Conversion process exited with code " + std::to_string(exitCode), true);
        }
    }
}

ModelConversionDialog::Result ModelConversionDialog::handleConversionResult() {
    if (m_conversionResult.success) return Result::Converted;
    return Result::Failed;
}

void ModelConversionDialog::showInfoDialog(const std::string& message) const {
    MessageBoxA(m_parent, message.c_str(), "Conversion Info", MB_OK | MB_ICONINFORMATION);
}

void ModelConversionDialog::showErrorDialog(const std::string& message) const {
    MessageBoxA(m_parent, message.c_str(), "Conversion Error", MB_OK | MB_ICONERROR);
}

std::pair<bool, ConversionError> ModelConversionDialog::validateModelPath() const {
    if (!fs::exists(m_modelPath)) return {false, ConversionError::InvalidModelPath};
    return {true, ConversionError::None};
}

std::pair<fs::path, ConversionError> ModelConversionDialog::findConverter(const fs::path& searchRoot) {
    // Basic search for common tools
    std::vector<std::string> candidates = {"llama-quantize.exe", "convert.py", "quantize.exe"};
    
    // Check current dir
    for(const auto& c : candidates) {
        if (fs::exists(searchRoot / c)) return {searchRoot / c, ConversionError::None};
    }
    
    // Check known bin folders
    if (fs::exists(searchRoot / "bin")) {
        for(const auto& c : candidates) {
            if (fs::exists(searchRoot / "bin" / c)) return {searchRoot / "bin" / c, ConversionError::None};
        }
    }

    return {{}, ConversionError::ConverterNotFound};
}
