#include "systems-ide-core-clean.hpp"
#include "keychain-integration.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "Systems IDE - Native C++ Implementation\n";
    std::cout << "TLS Beaconism Integration Active\n";
    std::cout << "========================================\n\n";
    
    // Initialize core IDE
    SystemsIDECore ide_core;
    
    if (!ide_core.Initialize()) {
        std::cerr << "Failed to initialize IDE core!\n";
        return 1;
    }
    
    std::cout << "✓ Core IDE initialized\n";
    
    // Initialize keychain integration
    KeychainIntegration keychain;
    
    if (!keychain.Initialize()) {
        std::cerr << "Failed to initialize keychain integration!\n";
        return 1;
    }
    
    std::cout << "✓ Keychain integration initialized\n";
    
    // Detect portable toolchains
    auto toolchains = keychain.DetectPortableToolchains();
    std::cout << "\n🔧 Detected Toolchains:\n";
    
    for (const auto& toolchain : toolchains) {
        std::cout << "  - " << toolchain.name << " (" << toolchain.version << ")\n";
        std::cout << "    Path: " << toolchain.executable_path << "\n";
        
        for (const auto& lang : toolchain.supported_languages) {
            std::cout << "    • " << lang << "\n";
        }
        std::cout << "\n";
    }
    
    // Test D: drive access
    std::cout << "📁 Testing D: drive access...\n";
    auto drive_contents = ide_core.ListDirectory("D:");
    
    std::cout << "Found " << drive_contents.size() << " items in D: drive\n";
    
    // Show first few items
    size_t max_show = std::min(drive_contents.size(), size_t(10));
    for (size_t i = 0; i < max_show; ++i) {
        std::cout << "  " << drive_contents[i] << "\n";
    }
    
    if (drive_contents.size() > 10) {
        std::cout << "  ... and " << (drive_contents.size() - 10) << " more items\n";
    }
    
    // Test multi-language compilation
    std::cout << "\n🚀 Testing Multi-Language Compilation:\n";
    
    if (argc > 1) {
        std::string input_file = argv[1];
        std::cout << "Compiling: " << input_file << "\n";
        
        std::string output_file = input_file + ".exe";
        
        if (keychain.CompileWithKeychain(input_file, output_file, "")) {
            std::cout << "✓ Compilation successful: " << output_file << "\n";
        } else {
            std::cout << "✗ Compilation failed\n";
        }
    } else {
        std::cout << "Usage: ide-main.exe <source_file>\n";
        std::cout << "Example: ide-main.exe test.cpp\n";
    }
    
    // Performance stats
    std::cout << "\n📊 Performance Statistics:\n";
    std::cout << "Cache Hit Ratio: " << (ide_core.GetCacheHitRatio() * 100.0) << "%\n";
    
    // Keep running until Ctrl+C
    std::cout << "\n🔄 IDE Ready - Press Ctrl+C to exit\n";
    std::cout << "TLS Beaconism: ✓ Active (1 year validity)\n";
    std::cout << "Portable Toolchains: ✓ " << toolchains.size() << " detected\n";
    std::cout << "Header-free Compilation: ✓ Enabled\n\n";
    
    // Simple command loop for demonstration
    std::string command;
    while (true) {
        std::cout << "ide> ";
        std::getline(std::cin, command);
        
        if (command == "exit" || command == "quit") {
            break;
        } else if (command.starts_with("ls ")) {
            std::string path = command.substr(3);
            auto files = ide_core.ListDirectory(path);
            for (const auto& file : files) {
                std::cout << file << "\n";
            }
        } else if (command.starts_with("read ")) {
            std::string path = command.substr(5);
            std::string content = ide_core.ReadFileOptimized(path);
            if (!content.empty()) {
                std::cout << "File size: " << content.size() << " bytes\n";
                // Show first 500 chars for demo
                if (content.size() > 500) {
                    std::cout << content.substr(0, 500) << "...\n";
                } else {
                    std::cout << content << "\n";
                }
            } else {
                std::cout << "Failed to read file\n";
            }
        } else if (command == "stats") {
            std::cout << "Cache Hit Ratio: " << (ide_core.GetCacheHitRatio() * 100.0) << "%\n";
            std::cout << "Toolchains: " << toolchains.size() << "\n";
        } else if (command == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  ls <path>     - List directory contents\n";
            std::cout << "  read <file>   - Read file contents\n";
            std::cout << "  stats         - Show performance stats\n";
            std::cout << "  help          - Show this help\n";
            std::cout << "  exit/quit     - Exit IDE\n";
        } else if (!command.empty()) {
            std::cout << "Unknown command. Type 'help' for available commands.\n";
        }
    }
    
    // Cleanup
    ide_core.Shutdown();
    keychain.Shutdown();
    
    std::cout << "\n✓ Systems IDE shutdown complete\n";
    return 0;
}