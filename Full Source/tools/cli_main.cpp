#include <windows.h>
#include <string>
#include <vector>
#include "logging/logger.h"

static Logger s_logger("CLI");

void PrintHelp() {
    s_logger.info("RawrXD Agent v1.0 (Qt-Free)");
    s_logger.info("USAGE: RawrXD_CLI [OPTIONS]");
    s_logger.info("OPTIONS:");
    s_logger.info("  --help, -h      Show help");
    s_logger.info("  --version, -v   Show version");
    s_logger.info("  --list, -l      List models");
    s_logger.info("  --port <n>      Server port");
    s_logger.info("  --host <ip>     Bind address");
}

void PrintVersion() {
    s_logger.info("RawrXD Agent v1.0.0 (Qt-Free Build)");
    s_logger.info("Architecture: x64");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintHelp();
        return 0;
    }
    
    std::string arg = argv[1];
    if (arg == "--help" || arg == "-h") { PrintHelp(); return 0; }
    if (arg == "--version" || arg == "-v") { PrintVersion(); return 0; }
    if (arg == "--list" || arg == "-l") { 
        s_logger.info("Available models:\n  - gpt-oss:120b\n  - llama3");
        return 0; 
    }
    
    s_logger.info("Starting RawrXD Agent...");
    return 0;
}
