#include <windows.h>
#include <cstdio>
#include <fstream>
#include <QApplication>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::ofstream log("D:\\temp\\test_ide.log");
    log << "TEST_IDE: WinMain entered\n"; log.flush();
    
    // Step 1: QApplication included
    log << "TEST_IDE: QApplication header included\n"; log.flush();
    
    // Step 2: Convert command line
    const char* argv_data[] = { "test_ide", nullptr };
    char* argv[] = { const_cast<char*>(argv_data[0]), nullptr };
    int argc = 1;
    
    log << "TEST_IDE: About to create QApplication\n"; log.flush();
    
    QApplication app(argc, argv);
    
    log << "TEST_IDE: QApplication created\n"; log.flush();
    
    MessageBoxA(nullptr, "QApplication created successfully!", "SUCCESS", MB_OK);
    
    log << "TEST_IDE: Done\n"; log.flush();
    return 0;
}
