#include "auto_bootstrap.h"
#include <iostream>
#include <windows.h>
#include <wininet.h>
#include <shlwapi.h>
#include <cstdlib>
#include <array>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

// ==================== CONSTRUCTOR / DESTRUCTOR ====================

AutoBootstrap::AutoBootstrap()
    : m_skipDownloads(false)
    , m_verbose(false)
{
    // Set default paths
    char* userProfile = nullptr;
    size_t len = 0;
    if (_dupenv_s(&userProfile, &len, "USERPROFILE") == 0 && userProfile) {
        m_downloadPath = std::string(userProfile) + "\\Downloads\\RawrXD";
        m_installPath = std::string(userProfile) + "\\AppData\\Local\\RawrXD";
        free(userProfile);
    } else {
        m_downloadPath = "C:\\temp\\RawrXD";
        m_installPath = "C:\\RawrXD";
    }

    initializeDependencies();
}

AutoBootstrap::~AutoBootstrap() {
}

// ==================== INITIALIZATION ====================

void AutoBootstrap::initializeDependencies() {
    // Git
    DependencyInfo git;
    git.name = "git";
    git.version = "";
    git.minVersion = "2.20.0";
    git.checkPath = "C:\\Program Files\\Git\\bin\\git.exe";
    git.checkCommand = "git --version";
    git.downloadUrl = "https://github.com/git-for-windows/git/releases/download/v2.43.0.windows.1/Git-2.43.0-64-bit.exe";
    git.installPath = "C:\\Program Files\\Git";
    git.required = true;
    git.installed = false;
    m_dependencies["git"] = git;

    // CMake
    DependencyInfo cmake;
    cmake.name = "cmake";
    cmake.version = "";
    cmake.minVersion = "3.20.0";
    cmake.checkPath = "C:\\Program Files\\CMake\\bin\\cmake.exe";
    cmake.checkCommand = "cmake --version";
    cmake.downloadUrl = "https://github.com/Kitware/CMake/releases/download/v3.27.8/cmake-3.27.8-windows-x86_64.msi";
    cmake.installPath = "C:\\Program Files\\CMake";
    cmake.required = true;
    cmake.installed = false;
    m_dependencies["cmake"] = cmake;

    // MSVC Compiler
    DependencyInfo msvc;
    msvc.name = "msvc";
    msvc.version = "";
    msvc.minVersion = "193.0";
    msvc.checkPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC";
    msvc.checkCommand = "cl.exe";
    msvc.downloadUrl = "";  // VS is installed via Visual Studio Installer
    msvc.installPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community";
    msvc.required = true;
    msvc.installed = false;
    m_dependencies["msvc"] = msvc;

    // Python
    DependencyInfo python;
    python.name = "python";
    python.version = "";
    python.minVersion = "3.8.0";
    python.checkPath = "C:\\Python312\\python.exe";
    python.checkCommand = "python --version";
    python.downloadUrl = "https://www.python.org/ftp/python/3.12.1/python-3.12.1-amd64.exe";
    python.installPath = "C:\\Python312";
    python.required = false;
    python.installed = false;
    m_dependencies["python"] = python;

    // PowerShell 7
    DependencyInfo pwsh;
    pwsh.name = "pwsh";
    pwsh.version = "";
    pwsh.minVersion = "7.0.0";
    pwsh.checkPath = "C:\\Program Files\\PowerShell\\7\\pwsh.exe";
    pwsh.checkCommand = "pwsh -Version";
    pwsh.downloadUrl = "https://github.com/PowerShell/PowerShell/releases/download/v7.4.0/PowerShell-7.4.0-win-x64.msi";
    pwsh.installPath = "C:\\Program Files\\PowerShell\\7";
    pwsh.required = false;
    pwsh.installed = false;
    m_dependencies["pwsh"] = pwsh;
}

// ==================== BOOTSTRAP SEQUENCE ====================

BootstrapStatus AutoBootstrap::bootstrapEnvironment() {
    if (m_verbose) {
        std::cout << "[AutoBootstrap] Starting environment bootstrap..." << std::endl;
    }

    // Step 1: Detect dependencies
    BootstrapStatus status = detectDependencies();
    if (status != BootstrapStatus::OK && status != BootstrapStatus::MISSING_DEPENDENCY) {
        m_lastError = "Failed to detect dependencies";
        return status;
    }

    // Step 2: Check for missing dependencies
    auto missing = getMissingDependencies();
    if (!missing.empty() && !m_skipDownloads) {
        if (m_verbose) {
            std::cout << "[AutoBootstrap] Downloading " << missing.size() << " missing dependencies..." << std::endl;
        }
        status = downloadMissingDependencies();
        if (status != BootstrapStatus::OK) {
            m_lastError = "Failed to download missing dependencies";
            return status;
        }
    }

    // Step 3: Setup environment variables
    setupEnvironmentVariables();

    // Step 4: Verify installations
    status = verifyInstallations();
    if (status != BootstrapStatus::OK) {
        m_lastError = "Some dependencies could not be verified";
        if (m_verbose) {
            std::cout << "[AutoBootstrap] Verification failed: " << m_lastError << std::endl;
        }
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] Bootstrap complete" << std::endl;
    }

    return status;
}

// ==================== DEPENDENCY DETECTION ====================

BootstrapStatus AutoBootstrap::detectDependencies() {
    if (m_verbose) {
        std::cout << "[AutoBootstrap] Detecting dependencies..." << std::endl;
    }

    checkGit();
    checkCMake();
    checkPython();
    checkCompiler();
    checkPowershell();

    return BootstrapStatus::OK;
}

bool AutoBootstrap::checkGit() {
    auto& git = m_dependencies["git"];
    std::string output = executeCommand("git --version");
    
    if (!output.empty() && output.find("git version") != std::string::npos) {
        git.installed = true;
        // Extract version: "git version 2.43.0.windows.1"
        size_t pos = output.find("version ") + 8;
        size_t end = output.find('\n', pos);
        if (pos != std::string::npos && end != std::string::npos) {
            git.detectedVersion = output.substr(pos, end - pos);
        }
        if (m_verbose) {
            std::cout << "[AutoBootstrap] Git detected: " << git.detectedVersion << std::endl;
        }
        return true;
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] Git NOT detected" << std::endl;
    }
    return false;
}

bool AutoBootstrap::checkCMake() {
    auto& cmake = m_dependencies["cmake"];
    std::string output = executeCommand("cmake --version");
    
    if (!output.empty() && output.find("cmake version") != std::string::npos) {
        cmake.installed = true;
        size_t pos = output.find("cmake version ") + 14;
        size_t end = output.find('\n', pos);
        if (pos != std::string::npos && end != std::string::npos) {
            cmake.detectedVersion = output.substr(pos, end - pos);
        }
        if (m_verbose) {
            std::cout << "[AutoBootstrap] CMake detected: " << cmake.detectedVersion << std::endl;
        }
        return true;
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] CMake NOT detected" << std::endl;
    }
    return false;
}

bool AutoBootstrap::checkPython() {
    auto& python = m_dependencies["python"];
    std::string output = executeCommand("python --version");
    
    if (!output.empty() && output.find("Python") != std::string::npos) {
        python.installed = true;
        size_t pos = output.find("Python ") + 7;
        size_t end = output.find('\n', pos);
        if (pos != std::string::npos && end != std::string::npos) {
            python.detectedVersion = output.substr(pos, end - pos);
        }
        if (m_verbose) {
            std::cout << "[AutoBootstrap] Python detected: " << python.detectedVersion << std::endl;
        }
        return true;
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] Python NOT detected" << std::endl;
    }
    return false;
}

bool AutoBootstrap::checkCompiler() {
    auto& msvc = m_dependencies["msvc"];
    std::string output = executeCommand("cl.exe");
    
    if (!output.empty() && (output.find("Microsoft") != std::string::npos || output.find("version") != std::string::npos)) {
        msvc.installed = true;
        size_t pos = output.find("Version ");
        if (pos != std::string::npos) {
            pos += 8;
            size_t end = output.find('\n', pos);
            if (end != std::string::npos) {
                msvc.detectedVersion = output.substr(pos, end - pos);
            }
        }
        if (m_verbose) {
            std::cout << "[AutoBootstrap] MSVC detected" << std::endl;
        }
        return true;
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] MSVC NOT detected" << std::endl;
    }
    return false;
}

bool AutoBootstrap::checkPowershell() {
    auto& pwsh = m_dependencies["pwsh"];
    std::string output = executeCommand("pwsh -Version 2>&1");
    
    if (!output.empty() && output.find("PowerShell") != std::string::npos) {
        pwsh.installed = true;
        if (m_verbose) {
            std::cout << "[AutoBootstrap] PowerShell 7 detected" << std::endl;
        }
        return true;
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] PowerShell 7 NOT detected" << std::endl;
    }
    return false;
}

// ==================== DOWNLOAD AND INSTALL ====================

BootstrapStatus AutoBootstrap::downloadMissingDependencies() {
    auto missing = getMissingDependencies();
    
    for (const auto& depName : missing) {
        auto& dep = m_dependencies[depName];
        
        if (dep.downloadUrl.empty()) {
            if (dep.required) {
                m_lastError = "Required dependency " + depName + " is missing and cannot be auto-installed";
                return BootstrapStatus::DOWNLOAD_FAILED;
            }
            continue;
        }

        if (m_verbose) {
            std::cout << "[AutoBootstrap] Downloading " << depName << "..." << std::endl;
        }

        // Create download directory if it doesn't exist
        fs::create_directories(m_downloadPath);

        std::string filename = dep.downloadUrl.substr(dep.downloadUrl.find_last_of("/") + 1);
        std::string fullPath = m_downloadPath + "\\" + filename;

        BootstrapStatus status = downloadFile(dep.downloadUrl, fullPath);
        if (status != BootstrapStatus::OK) {
            if (dep.required) {
                return status;
            }
            continue;
        }

        if (m_verbose) {
            std::cout << "[AutoBootstrap] Installing " << depName << "..." << std::endl;
        }

        status = installDependency(dep);
        if (status != BootstrapStatus::OK) {
            if (dep.required) {
                return status;
            }
            continue;
        }
    }

    return BootstrapStatus::OK;
}

BootstrapStatus AutoBootstrap::downloadFile(const std::string& url, const std::string& destPath) {
    // Use Windows URLDownloadToFile API
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), destPath.c_str(),
                                    0, nullptr);

    if (SUCCEEDED(hr)) {
        if (m_verbose) {
            std::cout << "[AutoBootstrap] Downloaded: " << destPath << std::endl;
        }
        return BootstrapStatus::OK;
    } else {
        m_lastError = "Failed to download: " + url + " (HRESULT: 0x" + 
                      std::to_string(static_cast<unsigned>(hr)) + ")";
        if (m_verbose) {
            std::cout << "[AutoBootstrap] Download failed: " << m_lastError << std::endl;
        }
        return BootstrapStatus::DOWNLOAD_FAILED;
    }
}

BootstrapStatus AutoBootstrap::installDependency(const DependencyInfo& dep) {
    // For executables/MSI files, attempt to run the installer
    // Note: This may require admin privileges
    
    std::string downloadedFile;
    std::string filename = dep.downloadUrl.substr(dep.downloadUrl.find_last_of("/") + 1);
    downloadedFile = m_downloadPath + "\\" + filename;

    if (!fs::exists(downloadedFile)) {
        m_lastError = "Downloaded file not found: " + downloadedFile;
        return BootstrapStatus::INSTALL_FAILED;
    }

    // For .exe files, simple execution
    // For .msi files, use msiexec
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    std::string command;
    if (filename.find(".msi") != std::string::npos) {
        command = "msiexec /i \"" + downloadedFile + "\" /quiet /norestart";
    } else if (filename.find(".exe") != std::string::npos) {
        command = "\"" + downloadedFile + "\" /SILENT /NORESTART";
    } else {
        m_lastError = "Unknown installer format: " + filename;
        return BootstrapStatus::INSTALL_FAILED;
    }

    if (!CreateProcessA(nullptr, (LPSTR)command.c_str(), nullptr, nullptr,
                       FALSE, 0, nullptr, nullptr, &si, &pi)) {
        m_lastError = "Failed to execute installer: " + std::to_string(GetLastError());
        return BootstrapStatus::INSTALL_FAILED;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        m_lastError = "Installer exited with code: " + std::to_string(exitCode);
        return BootstrapStatus::INSTALL_FAILED;
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] Installed: " << dep.name << std::endl;
    }

    return BootstrapStatus::OK;
}

// ==================== VERIFICATION ====================

BootstrapStatus AutoBootstrap::verifyInstallations() {
    if (m_verbose) {
        std::cout << "[AutoBootstrap] Verifying installations..." << std::endl;
    }

    // Re-detect all dependencies
    detectDependencies();

    // Check required dependencies
    for (const auto& [name, dep] : m_dependencies) {
        if (dep.required && !dep.installed) {
            m_lastError = "Required dependency not installed: " + name;
            if (m_verbose) {
                std::cout << "[AutoBootstrap] Verification FAILED: " << m_lastError << std::endl;
            }
            return BootstrapStatus::VERIFICATION_FAILED;
        }
    }

    if (m_verbose) {
        std::cout << "[AutoBootstrap] All required dependencies verified" << std::endl;
    }

    return BootstrapStatus::OK;
}

// ==================== ENVIRONMENT SETUP ====================

BootstrapStatus AutoBootstrap::setupEnvironmentVariables() {
    if (m_verbose) {
        std::cout << "[AutoBootstrap] Setting up environment variables..." << std::endl;
    }

    setupPath();
    setupCompilerEnvironment();

    return BootstrapStatus::OK;
}

void AutoBootstrap::setupPath() {
    // Add detected tool directories to PATH
    std::vector<std::string> pathEntries;

    if (m_dependencies["git"].installed) {
        pathEntries.push_back("C:\\Program Files\\Git\\bin");
    }
    if (m_dependencies["cmake"].installed) {
        pathEntries.push_back("C:\\Program Files\\CMake\\bin");
    }
    if (m_dependencies["python"].installed) {
        pathEntries.push_back("C:\\Python312");
    }
    if (m_dependencies["pwsh"].installed) {
        pathEntries.push_back("C:\\Program Files\\PowerShell\\7");
    }

    std::string currentPath = getEnvironmentVariable("PATH");
    for (const auto& entry : pathEntries) {
        if (currentPath.find(entry) == std::string::npos) {
            currentPath = entry + ";" + currentPath;
        }
    }

    setEnvironmentVariable("PATH", currentPath);
}

void AutoBootstrap::setupCompilerEnvironment() {
    // Setup VSTOOLS and compiler environment if MSVC is installed
    if (m_dependencies["msvc"].installed) {
        // Try to locate VCVARS batch file
        std::vector<std::string> vcvarsPaths = {
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
            "C:\\Program Files\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
        };

        for (const auto& vcvarsPath : vcvarsPaths) {
            if (fs::exists(vcvarsPath)) {
                if (m_verbose) {
                    std::cout << "[AutoBootstrap] Found vcvars at: " << vcvarsPath << std::endl;
                }
                // Note: To properly set compiler environment, caller should run vcvars64.bat
                break;
            }
        }
    }
}

bool AutoBootstrap::setEnvironmentVariable(const std::string& name, const std::string& value) {
    BOOL result = SetEnvironmentVariableA(name.c_str(), value.c_str());
    return result == TRUE;
}

std::string AutoBootstrap::getEnvironmentVariable(const std::string& name) const {
    char buffer[4096];
    DWORD size = GetEnvironmentVariableA(name.c_str(), buffer, sizeof(buffer));
    if (size > 0) {
        return std::string(buffer);
    }
    return std::string();
}

// ==================== UTILITIES ====================

std::string AutoBootstrap::executeCommand(const std::string& command) {
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        return result;
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    _pclose(pipe);
    return result;
}

std::string AutoBootstrap::getFileVersion(const std::string& filePath) {
    DWORD dwHandle = 0;
    DWORD dwSize = GetFileVersionInfoSizeA(filePath.c_str(), &dwHandle);
    if (dwSize == 0) {
        return std::string();
    }

    std::vector<BYTE> buffer(dwSize);
    if (!GetFileVersionInfoA(filePath.c_str(), dwHandle, dwSize, buffer.data())) {
        return std::string();
    }

    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT uLen = 0;
    if (!VerQueryValueA(buffer.data(), "\\", (LPVOID*)&pFileInfo, &uLen)) {
        return std::string();
    }

    if (pFileInfo == nullptr) {
        return std::string();
    }

    // Format version as major.minor.build.revision
    char versionStr[64];
    sprintf_s(versionStr, sizeof(versionStr), "%d.%d.%d.%d",
        HIWORD(pFileInfo->dwFileVersionMS),
        LOWORD(pFileInfo->dwFileVersionMS),
        HIWORD(pFileInfo->dwFileVersionLS),
        LOWORD(pFileInfo->dwFileVersionLS));

    return std::string(versionStr);
}

bool AutoBootstrap::compareVersions(const std::string& detected, const std::string& minimum) const {
    // Parse semantic versions (major.minor.patch)
    auto parseVersion = [](const std::string& version) -> std::tuple<int, int, int> {
        std::stringstream ss(version);
        std::string token;
        int major = 0, minor = 0, patch = 0;

        if (std::getline(ss, token, '.')) {
            major = std::stoi(token);
        }
        if (std::getline(ss, token, '.')) {
            minor = std::stoi(token);
        }
        if (std::getline(ss, token, '.')) {
            patch = std::stoi(token);
        }

        return std::make_tuple(major, minor, patch);
    };

    auto [detMajor, detMinor, detPatch] = parseVersion(detected);
    auto [minMajor, minMinor, minPatch] = parseVersion(minimum);

    // Compare versions: detected >= minimum
    if (detMajor > minMajor) return true;
    if (detMajor < minMajor) return false;
    if (detMinor > minMinor) return true;
    if (detMinor < minMinor) return false;
    return detPatch >= minPatch;
}

// ==================== QUERY METHODS ====================

bool AutoBootstrap::isDependencyInstalled(const std::string& depName) const {
    auto it = m_dependencies.find(depName);
    if (it == m_dependencies.end()) {
        return false;
    }
    return it->second.installed;
}

std::string AutoBootstrap::getDependencyVersion(const std::string& depName) const {
    auto it = m_dependencies.find(depName);
    if (it == m_dependencies.end()) {
        return std::string();
    }
    return it->second.detectedVersion;
}

std::vector<std::string> AutoBootstrap::getMissingDependencies() const {
    std::vector<std::string> missing;
    for (const auto& [name, dep] : m_dependencies) {
        if (!dep.installed) {
            missing.push_back(name);
        }
    }
    return missing;
}

// ==================== CONFIGURATION ====================

void AutoBootstrap::setDownloadPath(const std::string& path) {
    m_downloadPath = path;
    fs::create_directories(path);
}

void AutoBootstrap::setInstallPath(const std::string& path) {
    m_installPath = path;
}
