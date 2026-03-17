#include "universal_generator_service.h"
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include "runtime_core.h"
#include "engine/core_generator.h"
#include "modules/react_generator.h"  // Use the finished module generator
#include "shared_context.h"
#include "hot_patcher.h"
#include "cpu_inference_engine.h"

// No sockets, no winsock. Pure functional generation service.

class GeneratorService {
public:
    static GeneratorService& Get() {
        static GeneratorService instance;
        return instance;
    }

    std::string ProcessRequest(const std::string& request_type, const std::string& params_json) {
        if (request_type == "apply_hotpatch") {
             std::string target_str = extract_value(params_json, "target");
             std::string bytes_str = extract_value(params_json, "bytes"); // Space separated hex
             
             if (GlobalContext::Get().patcher) {
                 unsigned long long address = 0;
                 try {
                     // Check if address starts with 0x
                     size_t start = (target_str.find("0x") == 0 || target_str.find("0X") == 0) ? 2 : 0;
                     address = std::stoull(target_str.substr(start), nullptr, 16);
                 } catch (...) { 
                     return "Error: Invalid target address format (use hex, e.g., 0x1234)"; 
                 }

                 std::vector<unsigned char> bytes;
                 std::stringstream ss(bytes_str);
                 std::string byte_s;
                 while (ss >> byte_s) {
                     try {
                         bytes.push_back(static_cast<unsigned char>(std::stoul(byte_s, nullptr, 16)));
                     } catch (...) {}
                 }
                 
                 if (bytes.empty()) return "Error: No valid bytes provided";

                 // Using a generic name for ad-hoc patches
                 bool success = GlobalContext::Get().patcher->ApplyPatch("manual_patch_" + std::to_string(address), (void*)address, bytes);
                 return success ? "Success: Hotpatch applied to " + target_str : "Error: ApplyPatch failed (Access Violation or Invalid Address)"; 
             }
             return "Error: HotPatcher not initialized";
        }

        if (request_type == "get_memory_stats") {
            if (GlobalContext::Get().memory) {
                return GlobalContext::Get().memory->GetStatsString();
            }
             return "Error: Memory Core not initialized";
        }

        if (request_type == "get_engine_status") {
             // Mock status for now, or real if available
             std::string status = "Engine: Active\n";
             if (GlobalContext::Get().patcher) {
                 status += "HotPatcher: Ready\n";
             } else {
                 status += "HotPatcher: Inactive\n";
             }
             if (GlobalContext::Get().memory) {
                 status += "Memory: " + GlobalContext::Get().memory->GetStatsString() + "\n";
             }
             status += "Agentic Core: Online\n";
             status += "React Generator: Linked";
             return status;
        }
        
        if (request_type == "agent_query") {
             std::string prompt = extract_value(params_json, "prompt");
             if (GlobalContext::Get().agent_engine) {
                 return GlobalContext::Get().agent_engine->chat(prompt);
             }
             return "Error: Agentic Engine not initialized";
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
                 RawrXD::ReactServerConfig config;
                 config.name = name;
                 
                 if (type == "ide") {
                     config.include_ide_features = true;
                     config.include_monaco_editor = true;
                     config.include_agent_modes = true;
                     config.include_hotpatch_controls = true;
                     config.description = "RawrXD Agentic IDE (Generated)";
                 } else {
                     config.include_ide_features = false;
                 }
                 
                 result = RawrXD::ReactServerGenerator::Generate(path, config);
                 if (result && type == "ide") {
                     // Ensure IDE specific components are generated
                     result = RawrXD::ReactServerGenerator::GenerateIDEComponents(std::filesystem::path(path) / name, config);
                 }
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

        if (request_type == "generate_component") {
             std::string component = extract_value(params_json, "component");
             if (component.empty()) component = params_json; // Handle scalar param

             RawrXD::ReactServerConfig config;
             config.name = "RawrXDAgenticIDE";
             
             // Create a temp path for generation
             std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "rawr_gen" / component;
             std::filesystem::create_directories(tempPath);
             std::filesystem::create_directories(tempPath / "src" / "components" / "ide");

             bool result = false;
             std::filesystem::path outFile;

             if (component == "agent_mode") {
                 result = RawrXD::ReactServerGenerator::GenerateAgentModePanel(tempPath, config);
                 outFile = tempPath / "src" / "components" / "ide" / "AgentModes.js";
             }
             else if (component == "engine_manager") {
                 result = RawrXD::ReactServerGenerator::GenerateEngineManager(tempPath, config);
                 outFile = tempPath / "src" / "components" / "ide" / "EngineManager.js";
             }
             else if (component == "memory_viewer") {
                 result = RawrXD::ReactServerGenerator::GenerateMemoryViewer(tempPath, config);
                 outFile = tempPath / "src" / "components" / "ide" / "MemoryViewer.js";
             }
             else if (component == "re_tools") {
                 result = RawrXD::ReactServerGenerator::GenerateREToolsPanel(tempPath, config);
                 outFile = tempPath / "src" / "components" / "ide" / "RETools.js";
             }
             else {
                 return "Error: Unknown component type";
             }

             if (result && std::filesystem::exists(outFile)) {
                 return outFile.string();
             }
             return "Error: Generation failed";
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

        if (request_type == "search_extensions") {
             // Return mock list or query VSIXLoader if available
             // For now return a CSV list as expected by IDEWindow
             return "Cpp-Tools-Native,React-Generator-Plugin,Memory-Insider,Hex-Editor-Pro,Agent-Orchestrator";
        }

        if (request_type == "install_extension") {
             std::string extId = extract_value(params_json, "id");
             if (extId.empty()) extId = params_json; // Handle scalar
             return "Success: Extension '" + extId + "' installed (Mock).";
        }

        if (request_type == "get_agent_status") {
             return "Status: Idle\nMode: Autonomous\nNext Task: Awaiting Input";
        }

        return "Error: Unknown request type.";
    }

private:
    void runtime_load_model(const std::string& path) {
        // Integration point with GlobalContext inference engine
        if (GlobalContext::Get().inference_engine) {
            bool success = GlobalContext::Get().inference_engine->LoadModel(path);
            std::cout << (success ? "[Model] Loaded: " : "[Model] Failed: ") << path << std::endl;
        } else {
            std::cout << "[Mock] Model Load: " << path << std::endl;
        }
    }
    
    std::string process_prompt(const std::string& prompt) {
        // Route to agentic engine if available
        if (GlobalContext::Get().agent_engine) {
            return GlobalContext::Get().agent_engine->chat(prompt);
        }
        
        // Fallback Zero-Sim response
        std::string response = "[Generated Response]\n\n";
        response += "Topic: " + prompt.substr(0, 100) + "\n\n";
        response += "Key Points:\n";
        response += "- Core concept overview\n";
        response += "- Implementation strategies\n";
        response += "- Best practices\n";
        response += "- Common pitfalls to avoid\n\n";
        response += "For detailed implementation, load a model using /load <path>";
        return response;
    }
    
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
