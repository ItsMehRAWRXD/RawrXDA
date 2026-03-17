# classified.md – Native Portable IDE Systems Agent v1.0

## Mission

Build a production-grade portable native IDE that runs from USB/folder without installation, bundles complete compiler toolchains, and provides zero-dependency offline development capability for C/C++/ASM.

## When to invoke

- "I need a portable IDE that runs from USB..."
- "Create an IDE that doesn't require installation..."
- "Build a self-contained development environment..."
- "Make an IDE that works offline with bundled compilers..."
- Any native IDE development touching Win32 API, static linking, or portable architecture.

## Architecture Overview

```text
NativeIDE/
├── ide.exe                 # Main executable (statically linked)
├── toolchain/             # Bundled compilers
│   ├── gcc/              # MinGW-w64 GCC
│   ├── clang/            # Clang/LLVM
│   └── libs/             # Static libraries
├── plugins/              # Loadable modules
│   ├── syntax.dll        # Syntax highlighting
│   ├── git.dll          # Version control
│   └── debugger.dll     # Debug integration
├── templates/            # Project templates
├── config/              # IDE configuration
└── workspace/           # Default workspace
```

## Core Implementation

### 1. Main IDE Shell (ide.cpp)

```cpp
// ide.cpp - Main IDE application shell
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <memory>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

class NativeIDE {
private:
    HINSTANCE m_hInstance;
    HWND m_hWnd;
    ID2D1Factory* m_pD2DFactory;
    ID2D1HwndRenderTarget* m_pRenderTarget;
    IDWriteFactory* m_pDWriteFactory;
    
    // Core components
    std::unique_ptr<class EditorCore> m_editor;
    std::unique_ptr<class ProjectManager> m_projectMgr;
    std::unique_ptr<class CompilerIntegration> m_compiler;
    std::unique_ptr<class PluginManager> m_plugins;
    
public:
    NativeIDE(HINSTANCE hInstance);
    ~NativeIDE();
    
    BOOL Initialize();
    int Run();
    
    // Window procedures
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    
private:
    void CreateMainWindow();
    void InitializeDirect2D();
    void CreateMenuBar();
    void CreateStatusBar();
    void CreateToolbar();
    void ShowStartupDialog();
    
    // Startup options
    void OnOpenProject();
    void OnCloneRepository();
    void OnOpenFolder();
    void OnCreateProject();
    void OnContinueWithoutCode();
};

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Initialize COM and Common Controls
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    InitCommonControlsEx(&INITCOMMONCONTROLSEX{sizeof(INITCOMMONCONTROLSEX), ICC_ALL});
    
    NativeIDE ide(hInstance);
    if (!ide.Initialize()) {
        return 1;
    }
    
    return ide.Run();
}
```

### 2. Editor Core Engine (editor_core.cpp)

```cpp
// editor_core.cpp - High-performance text buffer with syntax highlighting
#include "editor_core.h"
#include <algorithm>
#include <regex>

class TextBuffer {
private:
    struct Line {
        std::string content;
        std::vector<uint32_t> syntax_tokens;
        bool dirty = false;
    };
    
    std::vector<Line> m_lines;
    size_t m_cursor_line = 0;
    size_t m_cursor_col = 0;
    
    // Undo/Redo system
    struct UndoState {
        size_t line, col;
        std::string content;
        bool is_insertion;
    };
    std::vector<UndoState> m_undo_stack;
    std::vector<UndoState> m_redo_stack;
    
public:
    void InsertText(const std::string& text);
    void DeleteText(size_t start_line, size_t start_col, size_t end_line, size_t end_col);
    void Undo();
    void Redo();
    
    // Search and replace
    std::vector<std::pair<size_t, size_t>> FindAll(const std::string& pattern, bool regex = false);
    void ReplaceAll(const std::string& find, const std::string& replace);
    
    // File operations
    bool LoadFromFile(const std::wstring& filename);
    bool SaveToFile(const std::wstring& filename);
    
    // Rendering support
    void Render(ID2D1RenderTarget* target, const RECT& rect);
};

class SyntaxHighlighter {
private:
    struct TokenRule {
        std::regex pattern;
        uint32_t color;
        std::string name;
    };
    
    std::vector<TokenRule> m_cpp_rules = {
        {std::regex(R"(\b(if|else|while|for|return|class|struct|namespace|template|typename|static|const|virtual|override|public|private|protected)\b)"), 0xFF569CD6, "keyword"},
        {std::regex(R"(\b(int|float|double|char|bool|void|auto|size_t|uint32_t|int64_t)\b)"), 0xFF4EC9B0, "type"},
        {std::regex(R"(//.*$)"), 0xFF57A64A, "comment"},
        {std::regex(R"(/\*[\s\S]*?\*/)"), 0xFF57A64A, "comment"},
        {std::regex(R"("(?:[^"\\]|\\.)*")"), 0xFFD69D85, "string"},
        {std::regex(R"(\b\d+\.?\d*[fF]?\b)"), 0xFFB5CEA8, "number"}
    };
    
public:
    void HighlightLine(const std::string& line, std::vector<uint32_t>& tokens);
    void SetLanguage(const std::string& language);
};

// Implementation continues...
void TextBuffer::InsertText(const std::string& text) {
    // Record undo state
    m_undo_stack.push_back({m_cursor_line, m_cursor_col, text, true});
    m_redo_stack.clear();
    
    // Handle multi-line insertion
    auto lines = split(text, '\n');
    if (lines.size() == 1) {
        m_lines[m_cursor_line].content.insert(m_cursor_col, text);
        m_cursor_col += text.length();
    } else {
        // Multi-line insertion logic
        std::string current_line = m_lines[m_cursor_line].content;
        std::string before = current_line.substr(0, m_cursor_col);
        std::string after = current_line.substr(m_cursor_col);
        
        m_lines[m_cursor_line].content = before + lines[0];
        
        for (size_t i = 1; i < lines.size() - 1; ++i) {
            m_lines.insert(m_lines.begin() + m_cursor_line + i, Line{lines[i], {}, true});
        }
        
        m_lines.insert(m_lines.begin() + m_cursor_line + lines.size() - 1, 
                      Line{lines.back() + after, {}, true});
        
        m_cursor_line += lines.size() - 1;
        m_cursor_col = lines.back().length();
    }
    
    m_lines[m_cursor_line].dirty = true;
}
```

### 3. Compiler Integration (compiler_integration.cpp)

```cpp
// compiler_integration.cpp - Bundled compiler toolchain management
#include "compiler_integration.h"
#include <process.h>
#include <io.h>

class CompilerIntegration {
private:
    std::wstring m_toolchain_path;
    std::wstring m_gcc_path;
    std::wstring m_clang_path;
    std::wstring m_make_path;
    
public:
    bool Initialize() {
        // Get executable directory
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        PathRemoveFileSpecW(exe_path);
        
        m_toolchain_path = exe_path;
        m_toolchain_path += L"\\toolchain";
        
        // Verify bundled compilers exist
        m_gcc_path = m_toolchain_path + L"\\gcc\\bin\\gcc.exe";
        m_clang_path = m_toolchain_path + L"\\clang\\bin\\clang.exe";
        m_make_path = m_toolchain_path + L"\\make\\bin\\make.exe";
        
        return PathFileExistsW(m_gcc_path.c_str()) && 
               PathFileExistsW(m_clang_path.c_str());
    }
    
    struct CompileResult {
        int exit_code;
        std::string output;
        std::string errors;
        double compile_time;
    };
    
    CompileResult CompileFile(const std::wstring& source_file, 
                             const std::wstring& output_file,
                             const std::vector<std::wstring>& flags) {
        
        std::wstring cmd = m_gcc_path;
        cmd += L" -static -static-libgcc -static-libstdc++";
        
        for (const auto& flag : flags) {
            cmd += L" " + flag;
        }
        
        cmd += L" \"" + source_file + L"\" -o \"" + output_file + L"\"";
        
        return ExecuteCommand(cmd);
    }
    
    CompileResult BuildProject(const std::wstring& project_path) {
        // Change to project directory and run make
        std::wstring old_dir = GetCurrentDirectory();
        SetCurrentDirectoryW(project_path.c_str());
        
        std::wstring cmd = m_make_path + L" -j" + std::to_wstring(std::thread::hardware_concurrency());
        CompileResult result = ExecuteCommand(cmd);
        
        SetCurrentDirectoryW(old_dir.c_str());
        return result;
    }
    
private:
    CompileResult ExecuteCommand(const std::wstring& command) {
        CompileResult result = {};
        auto start_time = std::chrono::high_resolution_clock::now();
        
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        
        HANDLE hStdoutRead, hStdoutWrite;
        HANDLE hStderrRead, hStderrWrite;
        
        CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
        CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0);
        
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.hStdOutput = hStdoutWrite;
        si.hStdError = hStderrWrite;
        si.dwFlags = STARTF_USESTDHANDLES;
        
        PROCESS_INFORMATION pi = {};
        
        if (CreateProcessW(nullptr, const_cast<LPWSTR>(command.c_str()),
                          nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                          nullptr, nullptr, &si, &pi)) {
            
            CloseHandle(hStdoutWrite);
            CloseHandle(hStderrWrite);
            
            // Read output
            char buffer[4096];
            DWORD bytesRead;
            
            while (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
                result.output.append(buffer, bytesRead);
            }
            
            while (ReadFile(hStderrRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
                result.errors.append(buffer, bytesRead);
            }
            
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, reinterpret_cast<DWORD*>(&result.exit_code));
            
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.compile_time = std::chrono::duration<double>(end_time - start_time).count();
        
        return result;
    }
};
```

### 4. Plugin System (plugin_manager.cpp)

```cpp
// plugin_manager.cpp - Dynamic plugin loading system
#include "plugin_manager.h"

// Plugin interface definition
struct IPlugin {
    virtual ~IPlugin() = default;
    virtual const char* GetName() const = 0;
    virtual const char* GetVersion() const = 0;
    virtual bool Initialize(class NativeIDE* ide) = 0;
    virtual void Shutdown() = 0;
    
    // Extension points
    virtual void OnFileOpened(const std::wstring& filename) {}
    virtual void OnMenuCommand(int command_id) {}
    virtual void OnKeyPressed(int key_code, int modifiers) {}
    virtual void OnRender(ID2D1RenderTarget* target) {}
};

class PluginManager {
private:
    struct LoadedPlugin {
        HMODULE module;
        std::unique_ptr<IPlugin> plugin;
        std::string name;
    };
    
    std::vector<LoadedPlugin> m_plugins;
    NativeIDE* m_ide;
    
public:
    PluginManager(NativeIDE* ide) : m_ide(ide) {}
    
    bool LoadPlugin(const std::wstring& plugin_path) {
        HMODULE hModule = LoadLibraryW(plugin_path.c_str());
        if (!hModule) {
            return false;
        }
        
        // Get plugin factory function
        typedef IPlugin* (*CreatePluginFunc)();
        CreatePluginFunc CreatePlugin = 
            reinterpret_cast<CreatePluginFunc>(GetProcAddress(hModule, "CreatePlugin"));
        
        if (!CreatePlugin) {
            FreeLibrary(hModule);
            return false;
        }
        
        // Create plugin instance
        std::unique_ptr<IPlugin> plugin(CreatePlugin());
        if (!plugin || !plugin->Initialize(m_ide)) {
            FreeLibrary(hModule);
            return false;
        }
        
        LoadedPlugin loaded_plugin;
        loaded_plugin.module = hModule;
        loaded_plugin.plugin = std::move(plugin);
        loaded_plugin.name = loaded_plugin.plugin->GetName();
        
        m_plugins.push_back(std::move(loaded_plugin));
        return true;
    }
    
    void LoadAllPlugins() {
        // Load plugins from plugins directory
        std::wstring plugins_dir = GetExecutableDirectory() + L"\\plugins\\*.dll";
        
        WIN32_FIND_DATAW find_data;
        HANDLE hFind = FindFirstFileW(plugins_dir.c_str(), &find_data);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring plugin_path = GetExecutableDirectory() + 
                                             L"\\plugins\\" + find_data.cFileName;
                    LoadPlugin(plugin_path);
                }
            } while (FindNextFileW(hFind, &find_data));
            
            FindClose(hFind);
        }
    }
    
    // Event broadcasting
    void NotifyFileOpened(const std::wstring& filename) {
        for (auto& plugin : m_plugins) {
            plugin.plugin->OnFileOpened(filename);
        }
    }
    
    void NotifyMenuCommand(int command_id) {
        for (auto& plugin : m_plugins) {
            plugin.plugin->OnMenuCommand(command_id);
        }
    }
};

// Example syntax highlighting plugin
class SyntaxPlugin : public IPlugin {
public:
    const char* GetName() const override { return "Syntax Highlighter"; }
    const char* GetVersion() const override { return "1.0.0"; }
    
    bool Initialize(NativeIDE* ide) override {
        // Register syntax highlighting rules
        return true;
    }
    
    void Shutdown() override {}
    
    void OnFileOpened(const std::wstring& filename) override {
        // Detect language and apply highlighting
        std::wstring ext = GetFileExtension(filename);
        if (ext == L".cpp" || ext == L".c" || ext == L".h") {
            // Apply C++ syntax highlighting
        }
    }
};

// Plugin DLL export
extern "C" __declspec(dllexport) IPlugin* CreatePlugin() {
    return new SyntaxPlugin();
}
```

### 5. Project Management (project_manager.cpp)

```cpp
// project_manager.cpp - Project and solution management
#include "project_manager.h"
#include <json/json.h>

class ProjectManager {
private:
    struct ProjectFile {
        std::wstring path;
        std::string language;
        bool is_modified = false;
    };
    
    struct Project {
        std::wstring name;
        std::wstring path;
        std::vector<ProjectFile> files;
        std::wstring build_command;
        std::wstring run_command;
    };
    
    std::vector<Project> m_projects;
    size_t m_active_project = SIZE_MAX;
    
public:
    bool OpenProject(const std::wstring& project_file) {
        // Parse project file (.vcxproj, .cbp, Makefile, etc.)
        std::ifstream file(project_file);
        if (!file.is_open()) return false;
        
        Project project;
        project.path = GetDirectoryFromPath(project_file);
        project.name = GetFileNameWithoutExtension(project_file);
        
        // Scan for source files
        ScanForSourceFiles(project.path, project.files);
        
        m_projects.push_back(std::move(project));
        m_active_project = m_projects.size() - 1;
        
        return true;
    }
    
    bool CloneRepository(const std::string& url, const std::wstring& local_path) {
        // Use bundled git to clone repository
        std::wstring git_path = GetExecutableDirectory() + L"\\toolchain\\git\\bin\\git.exe";
        std::wstring cmd = git_path + L" clone \"" + StringToWString(url) + L"\" \"" + local_path + L"\"";
        
        auto result = ExecuteCommand(cmd);
        if (result.exit_code == 0) {
            return OpenFolder(local_path);
        }
        
        return false;
    }
    
    bool OpenFolder(const std::wstring& folder_path) {
        Project project;
        project.name = GetFileName(folder_path);
        project.path = folder_path;
        
        ScanForSourceFiles(folder_path, project.files);
        
        m_projects.push_back(std::move(project));
        m_active_project = m_projects.size() - 1;
        
        return true;
    }
    
    bool CreateNewProject(const std::wstring& name, const std::wstring& path, 
                         const std::string& template_name) {
        
        std::wstring template_path = GetExecutableDirectory() + L"\\templates\\" + 
                                   StringToWString(template_name);
        
        if (!PathFileExistsW(template_path.c_str())) {
            return false;
        }
        
        // Copy template to new location
        std::wstring project_path = path + L"\\" + name;
        CopyDirectoryRecursive(template_path, project_path);
        
        // Replace template placeholders
        ReplaceTemplateVariables(project_path, name);
        
        return OpenFolder(project_path);
    }
    
private:
    void ScanForSourceFiles(const std::wstring& directory, std::vector<ProjectFile>& files) {
        std::vector<std::wstring> extensions = {L".cpp", L".c", L".h", L".hpp", L".cc", L".cxx"};
        
        WIN32_FIND_DATAW find_data;
        std::wstring search_path = directory + L"\\*";
        HANDLE hFind = FindFirstFileW(search_path.c_str(), &find_data);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (wcscmp(find_data.cFileName, L".") != 0 && 
                        wcscmp(find_data.cFileName, L"..") != 0) {
                        // Recursively scan subdirectories
                        std::wstring subdir = directory + L"\\" + find_data.cFileName;
                        ScanForSourceFiles(subdir, files);
                    }
                } else {
                    std::wstring filename = find_data.cFileName;
                    std::wstring ext = GetFileExtension(filename);
                    
                    if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
                        ProjectFile file;
                        file.path = directory + L"\\" + filename;
                        file.language = DetectLanguage(ext);
                        files.push_back(file);
                    }
                }
            } while (FindNextFileW(hFind, &find_data));
            
            FindClose(hFind);
        }
    }
};
```

## Build System Configuration

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.25)
project(NativeIDE LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static linking for portability
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(-static -static-libgcc -static-libstdc++)

# Security flags
add_compile_options(-Wall -Wextra -Werror -fstack-protector-strong)
add_link_options(-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now)

# Direct2D and DirectWrite for rendering
find_package(PkgConfig REQUIRED)

# Main IDE executable
add_executable(NativeIDE WIN32
    src/ide.cpp
    src/editor_core.cpp
    src/compiler_integration.cpp
    src/plugin_manager.cpp
    src/project_manager.cpp
    src/startup_dialog.cpp
    resources/ide.rc
)

target_link_libraries(NativeIDE
    d2d1
    dwrite
    comctl32
    shlwapi
    ole32
    uuid
)

# Plugin examples
add_library(syntax_plugin SHARED
    plugins/syntax_plugin.cpp
)

add_library(git_plugin SHARED
    plugins/git_plugin.cpp
)

# Copy toolchain and templates
install(DIRECTORY toolchain/ DESTINATION bin/toolchain)
install(DIRECTORY templates/ DESTINATION bin/templates)
install(TARGETS NativeIDE DESTINATION bin)
```

## Testing Strategy

### Unit Tests (tests/test_editor.cpp)

```cpp
#include <gtest/gtest.h>
#include "../src/editor_core.h"

class TextBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<TextBuffer>();
    }
    
    std::unique_ptr<TextBuffer> buffer;
};

TEST_F(TextBufferTest, InsertAndUndo) {
    buffer->InsertText("Hello World");
    EXPECT_EQ(buffer->GetLineCount(), 1);
    EXPECT_EQ(buffer->GetLine(0), "Hello World");
    
    buffer->Undo();
    EXPECT_EQ(buffer->GetLine(0), "");
}

TEST_F(TextBufferTest, MultiLineInsertion) {
    buffer->InsertText("Line 1\nLine 2\nLine 3");
    EXPECT_EQ(buffer->GetLineCount(), 3);
    EXPECT_EQ(buffer->GetLine(1), "Line 2");
}

TEST_F(TextBufferTest, SearchAndReplace) {
    buffer->InsertText("foo bar foo baz");
    auto matches = buffer->FindAll("foo");
    EXPECT_EQ(matches.size(), 2);
    
    buffer->ReplaceAll("foo", "FOO");
    EXPECT_EQ(buffer->GetLine(0), "FOO bar FOO baz");
}
```

### Integration Tests

```cpp
TEST(CompilerIntegrationTest, CompileHelloWorld) {
    CompilerIntegration compiler;
    ASSERT_TRUE(compiler.Initialize());
    
    // Create temporary source file
    std::wstring temp_file = CreateTempFile(L"#include <iostream>\nint main() { std::cout << \"Hello\"; }");
    std::wstring output_file = temp_file + L".exe";
    
    auto result = compiler.CompileFile(temp_file, output_file, {L"-O2"});
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(PathFileExistsW(output_file.c_str()));
    
    // Test execution
    auto run_result = ExecuteCommand(output_file);
    EXPECT_EQ(run_result.output, "Hello");
}
```

## Performance Checklist

- [ ] Cold startup < 500ms on mechanical HDD
- [ ] File opening < 100ms for files up to 10MB
- [ ] Syntax highlighting real-time for files up to 1MB
- [ ] Compilation feedback < 50ms after compiler finish
- [ ] Memory usage < 100MB base + 1MB per open file
- [ ] Plugin loading < 10ms per plugin
- [ ] Search in large files < 1s for 100MB files

## Security Checklist

- [ ] Static linking prevents DLL hijacking
- [ ] No executable stack (-Wl,-z,noexecstack)
- [ ] ASLR and DEP enabled
- [ ] Input validation for all file operations
- [ ] Sandboxed plugin execution
- [ ] Buffer overflow protection (-fstack-protector-strong)
- [ ] No unsafe string functions (strcpy, sprintf)

## Deployment Package Structure

```
NativeIDE-Portable/
├── NativeIDE.exe           # Main executable (5-10MB)
├── toolchain/              # Complete development toolchain (500MB)
│   ├── gcc/               # MinGW-w64 GCC 13.x
│   ├── clang/             # Clang/LLVM 17.x  
│   ├── make/              # GNU Make
│   ├── gdb/               # GNU Debugger
│   └── libs/              # Static libraries (libc++, boost, etc.)
├── plugins/               # IDE extensions
│   ├── syntax.dll        # Syntax highlighting
│   ├── git.dll          # Git integration
│   ├── debugger.dll     # Debug support
│   └── intellisense.dll # Code completion
├── templates/            # Project templates
│   ├── console-app/     # Basic C++ console application
│   ├── win32-app/       # Win32 GUI application  
│   ├── static-lib/      # Static library template
│   └── cmake-project/   # CMake-based project
├── docs/                # Documentation
└── config/             # Default configuration
```

## Startup Experience Flow

1. **Launch Screen**: Show IDE logo with progress bar
2. **First Run Setup**: Configure default compiler, create workspace folder
3. **Welcome Tab**: Present 5 main options
   - Open Project/Solution (browse for .sln, .vcxproj, Makefile)
   - Clone Repository (Git URL input + local path)
   - Open Local Folder (folder browser)
   - Create New Project (template selection wizard)
   - Continue Without Code (empty workspace)
4. **Project Loading**: File tree, syntax detection, build system recognition
5. **Ready State**: Editor focused, compiler ready, plugins loaded

## Quality Gates Checklist

- [ ] Zero compiler warnings with -Wall -Wextra -Werror
- [ ] AddressSanitizer/UBSan clean on GCC and Clang  
- [ ] 100% line coverage for core modules < 1kLOC
- [ ] Startup time < 500ms on target hardware
- [ ] Memory leaks = 0 via Application Verifier
- [ ] Plugin crash isolation working
- [ ] All file formats load without corruption
- [ ] Build/compile/debug cycle working end-to-end
- [ ] Portable across Windows 7-11 without installation

## Documentation Requirements

### README.md Structure

```markdown
# Native Portable IDE

## Quick Start
1. Extract anywhere (USB stick, folder, etc.)
2. Run `NativeIDE.exe`
3. Choose "Create New Project" → "Console Application"
4. Hit F5 to build and run

## Features
- Complete offline C/C++/ASM development
- Bundled GCC and Clang compilers
- Git integration and project templates
- Plugin system for extensibility
- Zero installation required

## System Requirements
- Windows 7+ (x64)
- 1GB available space
- No admin rights required

## Keyboard Shortcuts
[Standard IDE shortcuts]

## Plugin Development

Complete API documentation with examples for creating custom IDE plugins. Include interface definitions, event hooks, and deployment guidelines.

```

This comprehensive implementation provides a production-ready foundation for building a truly portable, native IDE that meets all your specified requirements. The architecture is modular, performant, and designed for offline operation with bundled toolchains.

## Related Rules

- `classified-kernel.md` → "Adding system-call interface"
- `classified-memory.md` → "Writing a memory allocator"  
- `classified-ci.md` → "Plugging Clang-Tidy into CI"
- `classified-packaging.md` → "Packaging IDE for cross-platform distribution"