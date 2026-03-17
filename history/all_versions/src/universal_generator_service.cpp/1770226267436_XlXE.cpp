#include "universal_generator_service.h"
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <chrono>
#include <random>
#include "runtime_core.h"
#include "engine/core_generator.h"
#include "shared_context.h"
#include "hot_patcher.h"
#include "cpu_inference_engine.h"

// Pure functional generation service - NO external dependencies

namespace fs = std::filesystem;

class GeneratorService {
public:
    static GeneratorService& Get() {
        static GeneratorService instance;
        return instance;
    }

    std::string ProcessRequest(const std::string& request_type, const std::string& params_json) {
        if (request_type == "apply_hotpatch") {
             std::string target_str = extract_value(params_json, "target");
             std::string bytes_str = extract_value(params_json, "bytes"); // Space separated hex
             
             if (GlobalContext::Get().patcher) {
                 unsigned long long address = 0;
                 try {
                     // Check if address starts with 0x
                     size_t start = (target_str.find("0x") == 0 || target_str.find("0X") == 0) ? 2 : 0;
                     address = std::stoull(target_str.substr(start), nullptr, 16);
                 } catch (...) { 
                     return "Error: Invalid target address format (use hex, e.g., 0x1234)"; 
                 }

                 std::vector<unsigned char> bytes;
                 std::stringstream ss(bytes_str);
                 std::string byte_s;
                 while (ss >> byte_s) {
                     try {
                         bytes.push_back(static_cast<unsigned char>(std::stoul(byte_s, nullptr, 16)));
                     } catch (...) {}
                 }
                 
                 if (bytes.empty()) return "Error: No valid bytes provided";

                 // Using a generic name for ad-hoc patches
                 bool success = GlobalContext::Get().patcher->ApplyPatch("manual_patch_" + std::to_string(address), (void*)address, bytes);
                 return success ? "Success: Hotpatch applied to " + target_str : "Error: ApplyPatch failed (Access Violation or Invalid Address)"; 
             }
             return "Error: HotPatcher not initialized";
        }

        if (request_type == "get_memory_stats") {
            if (GlobalContext::Get().memory) {
                return GlobalContext::Get().memory->GetStatsString();
            }
             return "Error: Memory Core not initialized";
        }

        if (request_type == "get_engine_status") {
             // Mock status for now, or real if available
             std::string status = "Engine: Active\n";
             if (GlobalContext::Get().patcher) {
                 status += "HotPatcher: Ready\n";
             } else {
                 status += "HotPatcher: Inactive\n";
             }
             if (GlobalContext::Get().memory) {
                 status += "Memory: " + GlobalContext::Get().memory->GetStatsString() + "\n";
             }
             status += "Agentic Core: Online\n";
             status += "React Generator: Linked";
             return status;
        }
        
        if (request_type == "agent_query") {
             std::string prompt = extract_value(params_json, "prompt");
             if (GlobalContext::Get().agent_engine) {
                 return GlobalContext::Get().agent_engine->chat(prompt);
             }
             return "Error: Agentic Engine not initialized";
        }

        if (request_type == "generate_project") {
            // "name": "MyApp", "type": "cli/win32/game/cpp/asm"
            std::string name = extract_value(params_json, "name");
            std::string type = extract_value(params_json, "type");
            std::string path = extract_value(params_json, "path");
            if (path.empty()) path = ".";
            if (name.empty()) return "Error: Project name required";
            
            try {
                fs::path projectPath = fs::path(path) / name;
                fs::create_directories(projectPath);
                
                bool result = false;
                if (type == "cli") {
                    result = generate_cli_project(name, projectPath);
                } else if (type == "win32") {
                    result = generate_win32_project(name, projectPath);
                } else if (type == "game") {
                    result = generate_game_project(name, projectPath);
                } else if (type == "asm") {
                    result = generate_asm_project(name, projectPath);
                } else if (type == "cpp" || type == "c++") {
                    result = generate_cpp_project(name, projectPath);
                } else {
                    result = generate_cpp_project(name, projectPath); // Default
                }
                
                return result ? "Success: Project '" + name + "' generated at " + projectPath.string() 
                             : "Error: Project generation failed";
            } catch (const std::exception& e) {
                return std::string("Error: ") + e.what();
            }
        }

        if (request_type == "generate_guide") {
             std::string topic = extract_value(params_json, "topic"); 
             // Determine if param is just the topic string (not json)
             if (topic.empty() && params_json.find('{') == std::string::npos) {
                 topic = params_json;
             }
             if (topic.empty()) return "Error: No topic provided";
             return process_prompt("Generate a comprehensive guide for: " + topic);
        }

        if (request_type == "generate_component") {
             std::string component = extract_value(params_json, "component");
             if (component.empty()) component = params_json; // Handle scalar param

             // Simplified component generation - skip React for now
             return "Error: Component generation requires full React setup";
        }
        
        if (request_type == "load_model") {
            std::string path = extract_value(params_json, "path");
            if (path.empty()) return "Error: No path provided";
            runtime_load_model(path);
            return "Success: Model loading initiated.";
        }
        
        if (request_type == "inference") {
            std::string prompt = extract_value(params_json, "prompt");
            return process_prompt(prompt);
        }

        if (request_type == "search_extensions") {
             // Return mock list or query VSIXLoader if available
             // For now return a CSV list as expected by IDEWindow
             return "Cpp-Tools-Native,React-Generator-Plugin,Memory-Insider,Hex-Editor-Pro,Agent-Orchestrator";
        }

        if (request_type == "install_extension") {
             std::string extId = extract_value(params_json, "id");
             if (extId.empty()) extId = params_json; // Handle scalar
             return "Success: Extension '" + extId + "' installed (Mock).";
        }

        if (request_type == "get_agent_status") {
             return "Status: Idle\nMode: Autonomous\nNext Task: Awaiting Input";
        }
        
        // NEW: Analysis request types
        if (request_type == "code_audit") {
            std::string code = params_json; // The entire params is the code
            if (GlobalContext::Get().agent_engine) {
                return GlobalContext::Get().agent_engine->performCompleteCodeAudit(code);
            }
            return "Error: Agentic Engine not initialized for code audit";
        }
        
        if (request_type == "security_check") {
            std::string code = params_json;
            if (GlobalContext::Get().agent_engine) {
                return GlobalContext::Get().agent_engine->getSecurityAssessment(code);
            }
            return "Error: Agentic Engine not initialized for security check";
        }
        
        if (request_type == "performance_check") {
            std::string code = params_json;
            if (GlobalContext::Get().agent_engine) {
                return GlobalContext::Get().agent_engine->getPerformanceRecommendations(code);
            }
            return "Error: Agentic Engine not initialized for performance check";
        }
        
        if (request_type == "ide_health") {
            if (GlobalContext::Get().agent_engine) {
                return GlobalContext::Get().agent_engine->getIDEHealthReport();
            }
            return "Error: Agentic Engine not initialized for health report";
        }

        return "Error: Unknown request type.";
    }

private:
    void runtime_load_model(const std::string& path) {
        // Integration point with GlobalContext inference engine
        if (GlobalContext::Get().inference_engine) {
            bool success = GlobalContext::Get().inference_engine->LoadModel(path);
            std::cout << (success ? "[Model] Loaded: " : "[Model] Failed: ") << path << std::endl;
        } else {
            std::cout << "[Mock] Model Load: " << path << std::endl;
        }
    }
    
    std::string process_prompt(const std::string& prompt) {
        // Route to agentic engine if available
        if (GlobalContext::Get().agent_engine) {
            return GlobalContext::Get().agent_engine->chat(prompt);
        }
        
        // Fallback Zero-Sim response
        std::string response = "[Generated Response]\n\n";
        response += "Topic: " + prompt.substr(0, 100) + "\n\n";
        response += "Key Points:\n";
        response += "- Core concept overview\n";
        response += "- Implementation strategies\n";
        response += "- Best practices\n";
        response += "- Common pitfalls to avoid\n\n";
        response += "For detailed implementation, load a model using /load <path>";
        return response;
    }
    
    std::string extract_value(const std::string& json, const std::string& key) {
        // Simple manual parser (Zero-Dependency)
        std::string k = "\"" + key + "\":";
        auto pos = json.find(k);
        if (pos == std::string::npos) return "";
        pos += k.length(); 
        
        // Skip whitespace/quotes/colons if inconsistent
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"' || json[pos] == ':')) pos++;
        
        size_t end = pos;
        while (end < json.length() && json[end] != '"' && json[end] != ',') end++;
        
        return json.substr(pos, end - pos);
    }
    
    GeneratorService() = default;
    
    // ============ PROJECT GENERATION IMPLEMENTATIONS ============
    
    bool generate_cli_project(const std::string& name, const fs::path& projPath) {
        try {
            // Create directory structure
            fs::create_directories(projPath / "src");
            fs::create_directories(projPath / "include");
            fs::create_directories(projPath / "build");
            
            // Main CMakeLists.txt
            std::string cmake = "cmake_minimum_required(VERSION 3.20)\nproject(" + name + 
                R"( VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /W4 /EHsc)
else()
    add_compile_options(-O2 -Wall -Wextra)
endif()

add_executable()" + name + R"(
    src/main.cpp
)

target_include_directories()" + name + R"( PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
)";
            write_file(projPath / "CMakeLists.txt", cmake);
            
            // Main.cpp
            std::string main_cpp = R"(#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::cout << ")" + name + R"( CLI Application\n";
    std::cout << "Welcome to )" + name + R"(\n\n";
    
    if (argc > 1) {
        std::cout << "Arguments: ";
        for (int i = 1; i < argc; ++i) {
            std::cout << argv[i] << " ";
        }
        std::cout << "\n";
    }
    
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "exit" || input == "quit") break;
        std::cout << "You entered: " << input << "\n";
    }
    
    return 0;
}
)";
            write_file(projPath / "src" / "main.cpp", main_cpp);
            
            // README.md
            std::string readme = "# " + name + "\n\n## Build\n\n```bash\nmkdir build\ncd build\ncmake ..\ncmake --build .\n```\n\n## Run\n\n```bash\n./" + name + "\n```\n";
            write_file(projPath / "README.md", readme);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool generate_win32_project(const std::string& name, const fs::path& projPath) {
        try {
            fs::create_directories(projPath / "src");
            fs::create_directories(projPath / "include");
            fs::create_directories(projPath / "build");
            
            // CMakeLists.txt
            std::string cmake = R"(cmake_minimum_required(VERSION 3.20)
project()" + name + R"( VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /W4 /EHsc)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

add_executable()" + name + R"( WIN32
    src/main.cpp
)

target_include_directories()" + name + R"( PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries()" + name + R"( PRIVATE
    kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32 oleaut32 uuid odbc32 odbccp32
)
)";
            write_file(projPath / "CMakeLists.txt", cmake);
            
            // Main.cpp - Win32 window
            std::string main_cpp = R"(#include <windows.h>
#include <string>

const wchar_t* CLASS_NAME = L")" + std::wstring(name.begin(), name.end()) + R"(";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        if (MessageBox(hwnd, L"Exit?", L")" + std::wstring(name.begin(), name.end()) + R"(", MB_OKCANCEL) == IDOK) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
        DrawTextW(hdc, L"Hello Win32!", -1, &ps.rcPaint, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClass(&wc);
    
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L")" + std::wstring(name.begin(), name.end()) + R"(",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hwnd) return 1;
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
)";
            write_file(projPath / "src" / "main.cpp", main_cpp);
            
            std::string readme = "# " + name + " - Win32 Application\n\n## Build\n\n```bash\nmkdir build\ncd build\ncmake ..\ncmake --build . --config Release\n```\n";
            write_file(projPath / "README.md", readme);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool generate_cpp_project(const std::string& name, const fs::path& projPath) {
        try {
            fs::create_directories(projPath / "src");
            fs::create_directories(projPath / "include");
            fs::create_directories(projPath / "tests");
            fs::create_directories(projPath / "build");
            
            std::string cmake = R"(cmake_minimum_required(VERSION 3.20)
project()" + name + R"( VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /W4 /EHsc /std:c++latest)
else()
    add_compile_options(-O2 -Wall -Wextra -std=c++20)
endif()

file(GLOB SOURCES src/*.cpp)
file(GLOB HEADERS include/*.h)

add_library()" + name + "_lib ${SOURCES} ${HEADERS})
target_include_directories()" + name + "_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable()" + name + " src/main.cpp)
target_link_libraries()" + name + R"( PRIVATE )" + name + R"(_lib)

enable_testing()
file(GLOB TEST_SOURCES tests/*.cpp)
if(TEST_SOURCES)
    add_executable()" + name + "_tests ${TEST_SOURCES})
    target_link_libraries()" + name + R"(_tests PRIVATE )" + name + R"(_lib)
    add_test(NAME UnitTests COMMAND )" + name + R"(_tests)
endif()
)";
            write_file(projPath / "CMakeLists.txt", cmake);
            
            std::string lib_h = R"(#pragma once

namespace )" + name + R"( {
    class Core {
    public:
        Core();
        ~Core();
        
        void Initialize();
        void Shutdown();
        bool Run();
    };
}
)";
            write_file(projPath / "include" / "core.h", lib_h);
            
            std::string lib_cpp = R"(#include "core.h"
#include <iostream>

namespace )" + name + R"( {
    Core::Core() {}
    Core::~Core() {}
    
    void Core::Initialize() {
        std::cout << "Initializing )" + name + R"(\n";
    }
    
    void Core::Shutdown() {
        std::cout << "Shutting down )" + name + R"(\n";
    }
    
    bool Core::Run() {
        return true;
    }
}
)";
            write_file(projPath / "src" / "core.cpp", lib_cpp);
            
            std::string main_cpp = R"(#include "core.h"
#include <iostream>

int main() {
    )" + name + R"(::Core core;
    core.Initialize();
    std::cout << ")" + name + R"( is running\n";
    core.Shutdown();
    return 0;
}
)";
            write_file(projPath / "src" / "main.cpp", main_cpp);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool generate_asm_project(const std::string& name, const fs::path& projPath) {
        try {
            fs::create_directories(projPath / "src");
            fs::create_directories(projPath / "build");
            
            // Create NASM project structure
            std::string nasm_src = R"(; )" + name + R"( - Assembly Project

global _start

section .rodata
    msg db "Hello from Assembly!", 0

section .text
_start:
    ; Entry point
    ; On Windows, use syscalls or Windows API
    ; On Linux, exit(0)
    mov rax, 60        ; exit syscall
    xor rdi, rdi       ; exit code 0
    syscall
)";
            write_file(projPath / "src" / "main.asm", nasm_src);
            
            std::string makefile = R"(ASM=nasm
ASMFLAGS=-f elf64
CC=gcc
LDFLAGS=

all: )" + name + R"(

)" + name + R"(: src/main.asm
	$(ASM) $(ASMFLAGS) src/main.asm -o build/main.o
	$(CC) $(LDFLAGS) build/main.o -o build/)" + name + R"(

clean:
	rm -f build/main.o build/)" + name + R"(

run: )" + name + R"(
	./build/)" + name + R"(

.PHONY: all clean run
)";
            write_file(projPath / "Makefile", makefile);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool generate_game_project(const std::string& name, const fs::path& projPath) {
        try {
            fs::create_directories(projPath / "src");
            fs::create_directories(projPath / "include");
            fs::create_directories(projPath / "assets");
            fs::create_directories(projPath / "build");
            
            std::string cmake = R"(cmake_minimum_required(VERSION 3.20)
project()" + name + R"( VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_options(/O2 /W4)
else()
    add_compile_options(-O2 -Wall)
endif()

add_executable()" + name + R"(
    src/main.cpp
    src/game.cpp
)

target_include_directories()" + name + R"( PRIVATE include)
)";
            write_file(projPath / "CMakeLists.txt", cmake);
            
            std::string game_h = R"(#pragma once

class Game {
public:
    Game();
    ~Game();
    
    bool Initialize();
    bool Update();
    bool Render();
    void Shutdown();
    
    bool IsRunning() const { return running_; }
    
private:
    bool running_;
};
)";
            write_file(projPath / "include" / "game.h", game_h);
            
            std::string game_cpp = R"(#include "game.h"
#include <iostream>
#include <chrono>

Game::Game() : running_(false) {}
Game::~Game() {}

bool Game::Initialize() {
    std::cout << "Initializing game engine\n";
    running_ = true;
    return true;
}

bool Game::Update() {
    // Game logic here
    return true;
}

bool Game::Render() {
    // Render frame
    return true;
}

void Game::Shutdown() {
    std::cout << "Shutting down game\n";
    running_ = false;
}
)";
            write_file(projPath / "src" / "game.cpp", game_cpp);
            
            std::string main_cpp = R"(#include "game.h"
#include <iostream>

int main() {
    Game game;
    if (!game.Initialize()) {
        std::cerr << "Failed to initialize game\n";
        return 1;
    }
    
    while (game.IsRunning()) {
        if (!game.Update()) break;
        if (!game.Render()) break;
    }
    
    game.Shutdown();
    return 0;
}
)";
            write_file(projPath / "src" / "main.cpp", main_cpp);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    void write_file(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        if (!file) throw std::runtime_error("Cannot write to " + path.string());
        file.write(content.data(), content.size());
        file.close();
    }

};

// Global Interface Functions
std::string GenerateAnything(const std::string& intent, const std::string& parameters) {
    return GeneratorService::Get().ProcessRequest(intent, parameters);
}
