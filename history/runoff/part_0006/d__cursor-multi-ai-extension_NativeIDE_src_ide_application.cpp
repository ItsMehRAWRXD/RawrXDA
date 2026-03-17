#include "ide_application.h"
#include "file_manager.h"
#include "main_window.h"
#include "project_manager.h"
#include "git_integration.h"
#include "editor_core.h"
#include "compiler_integration.h"
#include "plugin_manager.h"
#include "project_manager.h"
#include "git_integration.h"

IDEApplication::IDEApplication(HINSTANCE hInstance) 
    : m_hInstance(hInstance) {
    
    // Get application directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(hInstance, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    m_appDirectory = exePath;
    
    // Set configuration path
    m_configPath = m_appDirectory + L"\\config";
}

IDEApplication::~IDEApplication() {
    Shutdown();
}

bool IDEApplication::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    try {
        // Initialize directories
        if (!InitializeDirectories()) {
            return false;
        }
        
        // Load configuration
        if (!LoadConfiguration()) {
            SetupDefaultConfiguration();
        }
        
        // Initialize core components
        if (!InitializeComponents()) {
            return false;
        }
        
        // Create main window
        m_mainWindow = std::make_unique<MainWindow>(this);
        if (!m_mainWindow->Create()) {
            return false;
        }
        
        m_initialized = true;
        return true;
    }
    catch (const std::exception& e) {
        std::string error = "Failed to initialize IDE: ";
        error += e.what();
        MessageBoxA(nullptr, error.c_str(), "Initialization Error", MB_OK | MB_ICONERROR);
        return false;
    }
}

int IDEApplication::Run(int nCmdShow) {
    if (!m_initialized || !m_mainWindow) {
        return 1;
    }
    
    // Show the main window
    m_mainWindow->Show(nCmdShow);
    
    // Main message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // Handle accelerator keys
        if (!TranslateAccelerator(m_mainWindow->GetHandle(), nullptr, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    return static_cast<int>(msg.wParam);
}

void IDEApplication::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // Save configuration
    SaveConfiguration();
    
    // Shutdown components in reverse order
    m_mainWindow.reset();
    m_gitIntegration.reset();
    m_fileManager.reset();
    m_projectManager.reset();
    m_plugins.reset();
    m_compiler.reset();
    m_editor.reset();
    
    m_initialized = false;
}

bool IDEApplication::InitializeDirectories() {
    // Ensure config directory exists
    if (!PathFileExistsW(m_configPath.c_str())) {
        if (!CreateDirectoryW(m_configPath.c_str(), nullptr)) {
            return false;
        }
    }
    
    // Ensure other directories exist
    std::vector<std::wstring> dirs = {
        m_appDirectory + L"\\templates",
        m_appDirectory + L"\\plugins",
        m_appDirectory + L"\\toolchain",
        m_appDirectory + L"\\workspace"
    };
    
    for (const auto& dir : dirs) {
        if (!PathFileExistsW(dir.c_str())) {
            CreateDirectoryW(dir.c_str(), nullptr);
        }
    }
    
    return true;
}

bool IDEApplication::InitializeComponents() {
    try {
        // Initialize editor core
        m_editor = std::make_unique<EditorCore>();
        if (!m_editor->Initialize()) {
            return false;
        }
        
        // Initialize compiler integration
        m_compiler = std::make_unique<CompilerIntegration>(m_appDirectory + L"\\toolchain");
        if (!m_compiler->Initialize()) {
            return false;
        }
        
        // Initialize plugin manager
        m_plugins = std::make_unique<PluginManager>(this);
        m_plugins->LoadAllPlugins(m_appDirectory + L"\\plugins");
        
        // Initialize project manager
        m_projectManager = std::make_unique<ProjectManager>(this);
        
        // Initialize file manager
        m_fileManager = std::make_unique<FileManager>();
        
        // Initialize git integration
        m_gitIntegration = std::make_unique<GitIntegration>(m_appDirectory + L"\\toolchain\\git");
        
        return true;
    }
    catch (const std::exception& e) {
        std::string error = "Component initialization failed: ";
        error += e.what();
        MessageBoxA(nullptr, error.c_str(), "Component Error", MB_OK | MB_ICONERROR);
        return false;
    }
}

bool IDEApplication::LoadConfiguration() {
    std::wstring configFile = m_configPath + L"\\ide_config.ini";
    
    if (!PathFileExistsW(configFile.c_str())) {
        return false;
    }
    
    // TODO: Implement INI file reading
    // For now, return true to use defaults
    return true;
}

bool IDEApplication::SaveConfiguration() {
    std::wstring configFile = m_configPath + L"\\ide_config.ini";
    
    // TODO: Implement INI file writing
    // Save window positions, recent files, settings, etc.
    return true;
}

void IDEApplication::SetupDefaultConfiguration() {
    // Set default configuration values
    // TODO: Implement default settings
}

bool IDEApplication::OpenProject(const std::wstring& projectPath) {
    if (!m_projectManager) {
        return false;
    }
    
    return m_projectManager->OpenProject(projectPath);
}

bool IDEApplication::OpenFolder(const std::wstring& folderPath) {
    if (!m_projectManager) {
        return false;
    }
    
    return m_projectManager->OpenFolder(folderPath);
}

bool IDEApplication::CreateNewProject(const std::wstring& name, const std::wstring& path, const std::string& templateName) {
    if (!m_projectManager) {
        return false;
    }
    
    return m_projectManager->CreateNewProject(name, path, templateName);
}

bool IDEApplication::CloneRepository(const std::string& url, const std::wstring& localPath) {
    if (!m_gitIntegration) {
        return false;
    }
    
    bool success = m_gitIntegration->CloneRepository(url, localPath);
    if (success) {
        return OpenFolder(localPath);
    }
    
    return false;
}

bool IDEApplication::OpenFile(const std::wstring& filePath) {
    if (!m_editor || !m_fileManager) {
        return false;
    }
    
    return m_editor->OpenFile(filePath);
}

bool IDEApplication::SaveFile(const std::wstring& filePath) {
    if (!m_editor) {
        return false;
    }
    
    return m_editor->SaveCurrentFile(filePath);
}

bool IDEApplication::SaveFileAs(const std::wstring& filePath) {
    if (!m_editor) {
        return false;
    }
    
    return m_editor->SaveCurrentFileAs(filePath);
}

CompileResult IDEApplication::CompileCurrentFile() {
    if (!m_compiler || !m_editor) {
        return {};
    }
    
    std::wstring currentFile = m_editor->GetCurrentFilePath();
    if (currentFile.empty()) {
        return {};
    }
    
    return m_compiler->CompileFile(currentFile);
}

CompileResult IDEApplication::BuildProject() {
    if (!m_compiler || !m_projectManager) {
        return {};
    }
    
    auto currentProject = m_projectManager->GetCurrentProject();
    if (!currentProject) {
        return {};
    }
    
    return m_compiler->BuildProject(currentProject->path);
}

bool IDEApplication::RunExecutable(const std::wstring& exePath) {
    if (exePath.empty() || !PathFileExistsW(exePath.c_str())) {
        return false;
    }
    
    // Launch executable
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpFile = exePath.c_str();
    sei.nShow = SW_SHOW;
    
    return ShellExecuteExW(&sei) && sei.hProcess != nullptr;
}

bool IDEApplication::StartDebugging() {
    // TODO: Implement debugging support
    // This would integrate with GDB or other debuggers
    return false;
}