#include <windows.h>
#include <cstdio>
#include <fstream>

#include "agentic_ide.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::ofstream log("D:\\temp\\test_ide.log");
    log << "TEST_IDE: WinMain entered\n"; log.flush();
    
    // Step 1: void included
    log << "TEST_IDE: void header included\n"; log.flush();
    
    // Step 2: Convert command line
    const char* argv_data[] = { "test_ide", nullptr };
    char* argv[] = { const_cast<char*>(argv_data[0]), nullptr };
    int argc = 1;
    
    log << "TEST_IDE: About to create void\n"; log.flush();
    
    void app(argc, argv);
    
    log << "TEST_IDE: void created\n"; log.flush();
    
    log << "TEST_IDE: About to create AgenticIDE\n"; log.flush();
    
    AgenticIDE ide;
    
    log << "TEST_IDE: AgenticIDE created\n"; log.flush();
    
    MessageBoxA(nullptr, "AgenticIDE created successfully!", "SUCCESS", MB_OK);
    
    log << "TEST_IDE: Done\n"; log.flush();
    return 0;
}

