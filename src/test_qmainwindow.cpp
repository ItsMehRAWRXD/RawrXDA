#include <windows.h>
#include <cstdio>
#include <fstream>


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::ofstream log("D:\\temp\\test_qmainwindow.log");
    log << "TEST: WinMain entered\n"; log.flush();
    
    const char* argv_data[] = { "test", nullptr };
    char* argv[] = { const_cast<char*>(argv_data[0]), nullptr };
    int argc = 1;
    
    log << "TEST: Creating QApplication\n"; log.flush();
    
    QApplication app(argc, argv);
    
    log << "TEST: QApplication created\n"; log.flush();
    
    log << "TEST: Creating void\n"; log.flush();
    
    void window;
    window.setWindowTitle("Test Window");
    window.setMinimumSize(400, 300);
    
    log << "TEST: void created\n"; log.flush();
    
    window.show();
    
    log << "TEST: Window shown, entering event loop\n"; log.flush();
    
    int result = app.exec();
    
    log << "TEST: Event loop finished with code " << result << "\n"; log.flush();
    
    return result;
}

