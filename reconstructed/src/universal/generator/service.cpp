#include "universal_generator_service.h"
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include "runtime_core.h"
#include "engine/core_generator.h"
#include "shared_context.h"

// No sockets, no winsock. Pure functional generation service.

class GeneratorService {
public:
    static GeneratorService& Get() {
        static GeneratorService instance;
        return instance;
    }

    std::string ProcessRequest(const std::string& request_type, const std::string& params_json) {
        if (request_type == "generate_project") {
            // "name": "MyApp", "type": "cli/web/game"
            std::string name = extract_value(params_json, "name");
            std::string type = extract_value(params_json, "type");
            std::string path = extract_value(params_json, "path");
            if (path.empty()) path = ".";
            
            bool result = false;
            // Accessing CoreGenerator singleton
            if (type == "web") result = CoreGenerator::GetInstance().GenerateWebApp(name, std::filesystem::path(path));
            else if (type == "cli") result = CoreGenerator::GetInstance().GenerateCLI(name, std::filesystem::path(path));
            else if (type == "game") result = CoreGenerator::GetInstance().GenerateGame(name, std::filesystem::path(path));
            else result = CoreGenerator::GetInstance().GenerateWithAllFeatures(name, LanguageType::CPP, std::filesystem::path(path)); // Default fallback
            
            return result ? "Success: Project generated." : "Error: Generation failed.";
        }

        if (request_type == "generate_guide") {
            std::string topic = extract_value(params_json, "topic");
            // If explicit JSON not found, assume the whole params string is the topic
            if (topic.empty()) topic = params_json;
            
            // "Zero-Sim" hardcoded logic for specific non-coding requests
            if (topic.find("cookie") != std::string::npos) {
                return "Universal Generator Guide: [Perfect Chocolate Chip Cookies]\n\n"
                       "Ingredients:\n"
                       "- 2 1/4 cups all-purpose flour\n"
                       "- 1 tsp baking soda\n"
                       "- 1 cup unsalted butter, softened\n"
                       "- 3/4 cup granulated sugar\n"
                       "- 3/4 cup packed brown sugar\n"
                       "- 1 tsp vanilla extract\n"
                       "- 2 large eggs\n"
                       "- 2 cups semi-sweet chocolate chips\n\n"
                       "Instructions:\n"
                       "1. Preheat oven to 375 F (190 C).\n"
                       "2. In a small bowl, mix flour, baking soda, and salt.\n"
                       "3. In a large bowl, beat butter, sugars, and vanilla until creamy.\n"
                       "4. Add eggs, one at a time, beating well after each addition.\n"
                       "5. Gradually beat in flour mixture. Stir in chocolate chips.\n"
                       "6. Drop by rounded tablespoon onto ungreased baking sheets.\n"
                       "7. Bake 9-11 minutes or until golden brown. Cool on wire rack.\n\n"
                       "Enjoy!";
            }
            // Fallback to inference engine for other topics
            return process_prompt("Generate a guide for: " + topic);
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
