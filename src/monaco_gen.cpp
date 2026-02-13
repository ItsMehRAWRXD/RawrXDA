#include "engine/react_ide_generator.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct Args {
    std::string name = "rawrxd-ide";
    std::string template_name = "minimal";
    std::filesystem::path output_dir = std::filesystem::current_path();
};

void PrintUsage(const char* exe) {
    std::cout << "Usage: " << exe << " [--name <name>] [--template <minimal|full|agentic>] [--out <dir>]\n";
    std::cout << "Examples:\n";
    std::cout << "  " << exe << " --name RawrXD-IDE --template minimal --out .\\rawrxd-ide\n";
    std::cout << "  " << exe << " --template full --out D:\\apps\\rawrxd-ide\n";
}

bool ParseArgs(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--name" && i + 1 < argc) {
            args.name = argv[++i];
        } else if (arg == "--template" && i + 1 < argc) {
            args.template_name = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            args.output_dir = std::filesystem::path(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            return false;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char** argv) {
    Args args;
    if (!ParseArgs(argc, argv, args)) {
        PrintUsage(argv[0]);
        return 1;
    }

    ReactIDEGenerator generator;
    bool success = false;

    if (args.template_name == "minimal") {
        success = generator.GenerateMinimalIDE(args.name, args.output_dir);
    } else if (args.template_name == "full") {
        success = generator.GenerateFullIDE(args.name, args.output_dir);
    } else if (args.template_name == "agentic") {
        success = generator.GenerateAgenticIDE(args.name, args.output_dir);
    } else {
        std::cerr << "Unknown template: " << args.template_name << "\n";
        PrintUsage(argv[0]);
        return 1;
    }

    if (!success) {
        std::cerr << "Failed to generate Monaco IDE output.\n";
        return 1;
    }

    std::cout << "Monaco IDE generated at: " << args.output_dir << "\n";
    std::cout << "Next steps:\n";
    std::cout << "  cd " << args.output_dir << "\\" << args.name << "\n";
    std::cout << "  npm install\n";
    std::cout << "  npm run dev\n";
    return 0;
}
