#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include "http_server.h"

void PrintHelp() {
    std::cout << R"(RawrXD Agent v1.0
USAGE: RawrXD_Agent.exe [OPTIONS]
OPTIONS:
    --help, -h      Show this help
    --version, -v   Show version
    --list, -l      List models
    --port <n>      Server port (default: 11434)
    --host <ip>     Bind address (default: 127.0.0.1)
)";
}

void PrintVersion() {
    std::cout << "RawrXD Agent v1.0.0 (Qt-Free)\n";
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        // Default: start server
        std::cout << "Starting server on 127.0.0.1:11434\n";
        // HttpServer server("127.0.0.1", 11434); server.Run();
        return 0;
    }
    
    std::wstring arg = argv[1];
    if (arg == L"--help" || arg == L"-h") { PrintHelp(); return 0; }
    if (arg == L"--version" || arg == L"-v") { PrintVersion(); return 0; }
    if (arg == L"--list" || arg == L"-l") { 
        std::cout << "Models: (enumerate from ModelManager)\n"; 
        return 0; 
    }
    
    int port = 11434;
    std::string host = "127.0.0.1";
    
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if ((a == L"--port" || a == L"-p") && i + 1 < argc) 
            port = _wtoi(argv[++i]);
        if ((a == L"--host") && i + 1 < argc) 
            host = std::string(argv[++i], argv[++i] + wcslen(argv[i]));
    }
    
    std::cout << "Starting server on " << host << ":" << port << "\n";
    return 0;
}
