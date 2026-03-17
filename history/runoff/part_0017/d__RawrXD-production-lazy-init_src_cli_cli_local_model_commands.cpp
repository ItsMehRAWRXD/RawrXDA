// ============================================================================
// CLI Local Model Commands - Production Implementation
// ============================================================================
// Registers local model commands with CLI argument parser
// ============================================================================

#include "cli_local_model.hpp"
#include "cli_compiler_engine.hpp"
#include <iostream>
#include <vector>
#include <algorithm>

namespace RawrXD {
namespace CLI {

// ============================================================================
// COMMAND REGISTRATION
// ============================================================================

void CLILocalModelCommands::registerCommands(CLIArgumentParser& parser) {
    // Single model commands
    parser.addArgument({
        "load-model", "lm",
        "Load a local GGUF model from disk",
        true,  // takes value
        "",    // no default
        false  // not required
    });
    
    parser.addArgument({
        "validate-model", "vm",
        "Validate a local GGUF model file",
        true,  // takes value
        "",    // no default
        false  // not required
    });
    
    parser.addArgument({
        "show-model-info", "smi",
        "Show detailed information about a local GGUF model",
        true,  // takes value
        "",    // no default
        false  // not required
    });
    
    // Batch commands
    parser.addArgument({
        "load-models", "lms",
        "Load multiple local GGUF models (comma-separated paths)",
        true,  // takes value
        "",    // no default
        false  // not required
    });
    
    parser.addArgument({
        "validate-models", "vms",
        "Validate multiple local GGUF models (comma-separated paths)",
        true,  // takes value
        "",    // no default
        false  // not required
    });
    
    // Flags
    parser.addArgument({
        "local", "l",
        "Force local model loading (skip cloud checks)",
        false, // no value
        "false", // default
        false  // not required
    });
    
    parser.addArgument({
        "verbose", "v",
        "Enable verbose output for model operations",
        false, // no value
        "false", // default
        false  // not required
    });
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

int CLILocalModelCommands::executeCommand(const std::string& command,
                                         const std::vector<std::string>& args,
                                         CLILocalModel& local_model) {
    if (command == "load-model" || command == "lm") {
        return cmdLoadModel(local_model, args);
    } else if (command == "validate-model" || command == "vm") {
        return cmdValidateModel(local_model, args);
    } else if (command == "show-model-info" || command == "smi") {
        return cmdShowModelInfo(local_model, args);
    } else if (command == "load-models" || command == "lms") {
        return cmdLoadModels(local_model, args);
    } else if (command == "validate-models" || command == "vms") {
        return cmdLoadModels(local_model, args); // Reuse load logic for validation
    }
    
    return static_cast<int>(CLILocalModelError::UNKNOWN_ERROR);
}

// ============================================================================
// INDIVIDUAL COMMAND IMPLEMENTATIONS
// ============================================================================

int CLILocalModelCommands::cmdLoadModel(CLILocalModel& local_model,
                                       const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No model path specified for load-model command" << std::endl;
        return static_cast<int>(CLILocalModelError::FILE_NOT_FOUND);
    }
    
    const std::string& path = args[0];
    return local_model.loadModel(path);
}

int CLILocalModelCommands::cmdValidateModel(CLILocalModel& local_model,
                                           const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No model path specified for validate-model command" << std::endl;
        return static_cast<int>(CLILocalModelError::FILE_NOT_FOUND);
    }
    
    const std::string& path = args[0];
    return local_model.validateModel(path);
}

int CLILocalModelCommands::cmdShowModelInfo(CLILocalModel& local_model,
                                           const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No model path specified for show-model-info command" << std::endl;
        return static_cast<int>(CLILocalModelError::FILE_NOT_FOUND);
    }
    
    const std::string& path = args[0];
    return local_model.showModelInfo(path);
}

int CLILocalModelCommands::cmdLoadModels(CLILocalModel& local_model,
                                        const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No model paths specified for load-models command" << std::endl;
        return static_cast<int>(CLILocalModelError::UNKNOWN_ERROR);
    }
    
    // Parse comma-separated paths
    std::vector<std::string> paths;
    for (const auto& arg : args) {
        // Split by comma
        size_t start = 0;
        size_t end = arg.find(',');
        while (end != std::string::npos) {
            paths.push_back(arg.substr(start, end - start));
            start = end + 1;
            end = arg.find(',', start);
        }
        paths.push_back(arg.substr(start));
    }
    
    return local_model.loadModels(paths);
}

// ============================================================================
// COMMAND LINE INTEGRATION
// ============================================================================

// Example usage in cli_main.cpp:
/*
void handleLocalModelCommands(CLIArgumentParser& parser) {
    CLILocalModel local_model;
    
    // Set verbose if flag is present
    if (parser.hasOption("verbose")) {
        local_model.setVerbose(true);
    }
    
    // Execute commands
    if (parser.hasOption("load-model")) {
        std::string path = parser.getOption("load-model");
        int result = local_model.loadModel(path);
        if (result != 0) {
            std::cerr << "Failed to load model: " << path << std::endl;
            exit(result);
        }
    }
    
    if (parser.hasOption("validate-model")) {
        std::string path = parser.getOption("validate-model");
        int result = local_model.validateModel(path);
        if (result != 0) {
            std::cerr << "Model validation failed: " << path << std::endl;
            exit(result);
        }
    }
    
    if (parser.hasOption("show-model-info")) {
        std::string path = parser.getOption("show-model-info");
        int result = local_model.showModelInfo(path);
        if (result != 0) {
            std::cerr << "Failed to show model info: " << path << std::endl;
            exit(result);
        }
    }
}
*/

} // namespace CLI
} // namespace RawrXD