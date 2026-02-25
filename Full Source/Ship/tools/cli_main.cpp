#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

void PrintHelp() {
    std::cout << "RawrXD Agent v1.0 (Qt-Free)\n";
    std::cout << "USAGE: RawrXD_CLI [OPTIONS]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "  --help, -h      Show help\n";
    std::cout << "  --version, -v   Show version\n";
    std::cout << "  --list, -l      List models\n";
    std::cout << "  --port <n>      Server port\n";
    std::cout << "  --host <ip>     Bind address\n";
}

void PrintVersion() {
    std::cout << "RawrXD Agent v1.0.0 (Qt-Free Build)\n";
    std::cout << "Architecture: x64\n";
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        PrintHelp();
        return 0;
    }
    
    std::wstring arg = argv[1];
    if (arg == L"--help" || arg == L"-h") { PrintHelp(); return 0; }
    if (arg == L"--version" || arg == L"-v") { PrintVersion(); return 0; }
    if (arg == L"--list" || arg == L"-l") { 
        std::cout << "Available models:\n  - gpt-oss:120b\n  - llama3\n"; 
        return 0; 
    }
    
    std::cout << "Starting RawrXD Agent...\n";
    return 0;
}
