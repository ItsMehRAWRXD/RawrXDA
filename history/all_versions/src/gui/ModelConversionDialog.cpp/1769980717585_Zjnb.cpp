#include "ModelConversionDialog.h"
#include "../win32app/IDELogger.h"
#include <sstream>
#include <algorithm>
#include <thread>
#include <array>
#include <format>
#include <iostream>

// Helper to convert wstring to string manually if needed, but we use filesystem path string() which is generic
static std::string pathToString(const fs::path& p) {
    return p.string();
}

// --------------------------------------------------------------------------------------
// ProcessHandle Implementation
// --------------------------------------------------------------------------------------

ProcessHandle::~ProcessHandle() {
    close();
}

ProcessHandle::ProcessHandle(ProcessHandle&& other) noexcept : m_handle(other.m_handle) {
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

// --------------------------------------------------------------------------------------
// ModelConversionDialog Implementation
// --------------------------------------------------------------------------------------

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
    // Auto-detect if converter path is missing
    if (m_config.converterPath.empty()) {
        auto [path, error] = findConverter(fs::current_path());
        if (error == ConversionError::None) {
            m_config.converterPath = path;
        }
    }
}

bool ModelConversionDialog::needsConversion(const fs::path& modelPath) {
    auto ext = modelPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    // Rough check: if it's not gguf, likely needs conversion for our engine
    return ext != ".gguf";
}

std::pair<fs::path, ConversionError> ModelConversionDialog::findConverter(const fs::path& searchRoot) {
    // Priority: 1. rawrxd-converter.exe, 2. llama-quantize.exe
    std::vector<std::string> candidates = { "rawrxd-converter.exe", "llama-quantize.exe", "tools/llama-quantize.exe" };
    
    // Check absolute or relative to CWD
    for (const auto& bin : candidates) {
        fs::path p(bin);
        if (fs::exists(p)) return { fs::absolute(p), ConversionError::None };
        
        fs::path p2 = searchRoot / p;
        if (fs::exists(p2)) return { fs::absolute(p2), ConversionError::None };
        
        // Also check a "Tools" directory sibling
        fs::path p3 = searchRoot / "Tools" / bin;
        if (fs::exists(p3)) return { fs::absolute(p3), ConversionError::None };
    }

    return { {}, ConversionError::ConverterNotFound };
}

ModelConversionDialog::Result ModelConversionDialog::exec() {
    log("Starting conversion dialog logic", false);

    // 1. Check if model path is valid
    auto [validModel, modelErr] = validateModelPath();
    if (!validModel) {
        log("Invalid model path: " + m_modelPath.string(), true);
        showErrorDialog("The selected model file is invalid or does not exist.");
        return Result::Failed;
    }

    // 2. Check if converter logic is configured
    auto [validConfig, configErr] = validatePaths();
    if (!validConfig) {
        if (configErr == ConversionError::ConverterNotFound) {
             showErrorDialog(buildConverterNotFoundMessage());
        } else {
             showErrorDialog("Security Violation: Path traversal detected in configuration.");
        }
        return Result::Failed;
    }

    // 3. User Confirmation
    if (m_config.showUI) {
        if (!askUserPermission(buildUserMessage())) {
            log("User cancelled conversion", false);
            return Result::Cancelled;
        }
    }

    // 4. Prepare Command
    auto [cmdLine, err] = buildCommandLine();
    if (err != ConversionError::None) {
        showErrorDialog("Failed to generate conversion command.");
        return Result::Failed;
    }

    // 5. Execute
    auto start = std::chrono::high_resolution_clock::now();
    log("Executing: " + cmdLine, false);
    
    auto [proc, execErr] = executeConverter(cmdLine);
    if (execErr != ConversionError::None || !proc.isValid()) {
        log("Failed to start process", true);
        showErrorDialog("Failed to launch the conversion tool.");
        return Result::Failed;
    }
    
    m_processHandle = std::move(proc);

    // 6. Monitor Loop
    showInfoDialog("Conversion started. Please wait...");
    monitorProcess(m_processHandle, m_config.timeout);

    auto end = std::chrono::high_resolution_clock::now();
    m_conversionResult.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return handleConversionResult();
}

void ModelConversionDialog::execAsync(std::function<void(Result, const ConversionResult&)> callback) {
    // In a real GUI app, we'd spawn a thread or use a dispatcher. 
    // Here we'll just detach a std::thread for simulation of async behavior.
    std::thread([this, callback]() {
        Result r = this->exec();
        if (callback) callback(r, m_conversionResult);
    }).detach();
}

// --------------------------------------------------------------------------------------
// Validation & Logic
// --------------------------------------------------------------------------------------

std::pair<bool, ConversionError> ModelConversionDialog::validatePaths() const {
    if (m_config.converterPath.empty()) return { false, ConversionError::ConverterNotFound };
    if (!fs::exists(m_config.converterPath)) return { false, ConversionError::ConverterNotFound };

    // Anti-path traversal check
    std::string pathStr = m_config.converterPath.string();
    if (pathStr.find("..") != std::string::npos) {
        return { false, ConversionError::PathTraversalDetected }; // suspicious
    }

    return { true, ConversionError::None };
}

std::pair<bool, ConversionError> ModelConversionDialog::validateModelPath() const {
    if (m_modelPath.empty()) return { false, ConversionError::InvalidModelPath };
    if (!fs::exists(m_modelPath)) return { false, ConversionError::InvalidModelPath };
    return { true, ConversionError::None };
}

std::string ModelConversionDialog::buildUserMessage() const {
    std::stringstream ss;
    ss << "The selected model format is not optimal for this engine.\n\n"
       << "Current: " << m_modelPath.extension().string() << "\n"
       << "Recommended: " << m_recommendedType << " (GGUF)\n\n"
       << "Would you like to convert it automatically using the local tool?";
    return ss.str();
}

std::string ModelConversionDialog::buildConverterNotFoundMessage() const {
    return "Conversion tool (rawrxd-converter or llama-quantize) not found in the application directory or Tools folder.";
}

std::pair<std::string, ConversionError> ModelConversionDialog::buildCommandLine() const {
    // Assumption: using llama-quantize syntax: <exe> <src> <dst> <type>
    // Or potentially python script. We'll assume the binary tool for now.
    
    fs::path destPath = m_modelPath;
    destPath.replace_extension(".gguf");
    
    // Check if dest exists, maybe append _converted
    if (fs::exists(destPath)) {
        destPath = m_modelPath.parent_path() / (m_modelPath.stem().string() + "_optimized.gguf");
    }

    // Store destination in result to look for it later
    const_cast<ModelConversionDialog*>(this)->m_conversionResult.convertedModelPath = destPath;

    std::stringstream cmd;
    cmd << "\"" << m_config.converterPath.string() << "\" "
        << "\"" << m_modelPath.string() << "\" "
        << "\"" << destPath.string() << "\" "
        << "Q4_K_M"; // Default quantization

    return { cmd.str(), ConversionError::None };
}

// --------------------------------------------------------------------------------------
// Execution helpers
// --------------------------------------------------------------------------------------

std::pair<ProcessHandle, ConversionError> ModelConversionDialog::executeConverter(const std::string& commandLine) const {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create pipes for stdout capture (simplified, typically needs security attributes)
    // For now we run without pipe capture to ensure stability, or minimal pipe
    // We'll just define basic creation for now.
    
    // Make a mutable copy of the command line
    std::string cmd = commandLine;

    // Create the child process. 
    BOOL result = CreateProcessA(
        NULL,           // No module name (use command line)
        cmd.data(),     // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        CREATE_NO_WINDOW, // No console window
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi             // Pointer to PROCESS_INFORMATION structure
    );

    if (!result) {
        log("CreateProcess failed with error: " + std::to_string(GetLastError()), true);
        return { ProcessHandle(), ConversionError::CommandExecutionFailed };
    }

    CloseHandle(pi.hThread);
    return { ProcessHandle(pi.hProcess), ConversionError::None };
}

void ModelConversionDialog::monitorProcess(ProcessHandle& handle, std::chrono::seconds timeout) {
    if (!handle.isValid()) return;

    // Wait until child process exits or timeout
    DWORD waitResult = WaitForSingleObject(handle.get(), (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());

    if (waitResult == WAIT_TIMEOUT) {
        log("Process timed out, terminating...", true);
        TerminateProcess(handle.get(), 1);
    }
}

ModelConversionDialog::Result ModelConversionDialog::handleConversionResult() {
    DWORD exitCode = 0;
    if (m_processHandle.isValid()) {
        if (GetExitCodeProcess(m_processHandle.get(), &exitCode)) {
             m_conversionResult.exitCode = (int)exitCode;
             if (exitCode == 0 && fs::exists(m_conversionResult.convertedModelPath)) {
                 log("Conversion successful: " + m_conversionResult.convertedModelPath.string(), false);
                 return Result::Converted;
             }
        }
    }
    
    log("Conversion process failed or produced no output.", true);
    return Result::Failed;
}

// --------------------------------------------------------------------------------------
// UI / Logging
// --------------------------------------------------------------------------------------

void ModelConversionDialog::log(const std::string& message, bool isError) const {
    auto level = isError ? IDELogger::Level::ERR : IDELogger::Level::INFO;
    IDELogger::getInstance().log(level, "ModelConverter", message);
}

void ModelConversionDialog::showInfoDialog(const std::string& message) const {
    if (!m_config.showUI || m_parent == NULL) return;
    // In a real non-blocking scenario we wouldn't use MessageBox here potentially
    // But for this blocking flow it's fine.
    // However, if called during processing, it blocks processing. 
    // We provided a "Please wait" earlier, actually we can't show a msgbox for "Please wait" as it blocks.
    // So we'll skip the "Please wait" dialog box in this implementation 
    // and only show results or errors.
}

void ModelConversionDialog::showErrorDialog(const std::string& message) const {
    if (!m_config.showUI) return;
    MessageBoxA(m_parent, message.c_str(), "Conversion Error", MB_ICONERROR | MB_OK);
}

bool ModelConversionDialog::askUserPermission(const std::string& message) const {
    if (!m_config.showUI) return true; // Default to yes if UI is hidden? Or config dependent.
    int result = MessageBoxA(m_parent, message.c_str(), "Optimize Model", MB_ICONQUESTION | MB_YESNO);
    return result == IDYES;
}



