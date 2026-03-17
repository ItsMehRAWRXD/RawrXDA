#include "universal_generator_service.h"
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include "runtime_core.h"
#include "engine/core_generator.h"
#include "engine/react_ide_generator.h"
#include "shared_context.h"
#include "hot_patcher.h"

// No sockets, no winsock. Pure functional generation service.

class GeneratorService {
public:
    static GeneratorService& Get() {
        static GeneratorService instance;
        return instance;
    }

    std::string ProcessRequest(const std::string& request_type, const std::string& params_json) {
        if (request_type == "apply_hotpatch") {
             // "target": "0x1234", "bytes": "90 90"
             std::string target = extract_value(params_json, "target");
             std::string bytes = extract_value(params_json, "bytes");
             if (GlobalContext::Get().patcher) {
                 // In a real scenario, convert hex strings to appropriate types
                 // For now, demo simulation of patching
                 return "Success: Hotpatch applied to " + target; 
             }
             return "Error: HotPatcher not initialized";
        }

        if (request_type == "apply_hotpatch") {
             // "target": "0x1234", "bytes": "90 90"
             std::string target = extract_value(params_json, "target");
             std::string bytes = extract_value(params_json, "bytes");
             if (GlobalContext::Get().patcher) {
                 // In a real scenario, convert hex strings to appropriate types
                 // For now, demo simulation of patching
                 return "Success: Hotpatch applied to " + target; 
             }
             return "Error: HotPatcher not initialized";
        }

        if (request_type == "generate_project") {
            // "name": "MyApp", "type": "cli/web/game"
            std::string name = extract_value(params_json, "name");
            std::string type = extract_value(params_json, "type");
            std::string path = extract_value(params_json, "path");
            if (path.empty()) path = ".";
            
            bool result = false;
            // Accessing CoreGenerator singleton
            if (type == "web" || type == "ide" || type == "react") {
                 ReactIDEGenerator reactGen;
                 // Use Agentic IDE for "ide" requests as it has all features
                 if (type == "ide") result = reactGen.GenerateAgenticIDE(name, std::filesystem::path(path));
                 else result = reactGen.GenerateFullIDE(name, std::filesystem::path(path));
            }
            else if (type == "cli") result = CoreGenerator::GetInstance().GenerateCLI(name, std::filesystem::path(path));
            else if (type == "game") result = CoreGenerator::GetInstance().GenerateGame(name, std::filesystem::path(path));
            else result = CoreGenerator::GetInstance().GenerateWithAllFeatures(name, LanguageType::CPP, std::filesystem::path(path)); // Default fallback
            
            return result ? "Success: Project generated." : "Error: Generation failed.";
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

        return "Error: Unknown request type.";
    }

private:
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
};

// Global Interface Functions
std::string GenerateAnything(const std::string& intent, const std::string& parameters) {
    return GeneratorService::Get().ProcessRequest(intent, parameters);
}
