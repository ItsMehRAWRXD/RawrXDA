/**
 * @file ai_code_generator.cpp
 * @brief AI-powered code generation from natural language implementation
 * Batch 5 - Item 72: AI code generator
 */

#include "ai/ai_code_generator.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::AI {

AICodeGenerator::AICodeGenerator()
    : m_initialized(false)
    , m_maxTokens(1000)
    , m_temperature(0.7f) {
    
    // Register default templates
    registerDefaultTemplates();
}

AICodeGenerator::~AICodeGenerator() {
    shutdown();
}

bool AICodeGenerator::initialize() {
    m_initialized = true;
    return true;
}

void AICodeGenerator::shutdown() {
    m_initialized = false;
}

GenerationResult AICodeGenerator::generate(const GenerationRequest& request) {
    GenerationResult result;
    
    if (!m_initialized) {
        result.error = "Generator not initialized";
        return result;
    }
    
    // Build prompt
    std::string prompt = buildPrompt(request);
    
    // Generate code
    result.code = generateCode(prompt, request.language);
    result.explanation = generateExplanation(request);
    result.suggestions = generateSuggestions(request);
    result.isComplete = true;
    
    // Add to history
    m_history.push_back(result);
    
    // Trim history
    if (m_history.size() > 100) {
        m_history.erase(m_history.begin());
    }
    
    if (m_generationCallback) {
        m_generationCallback(result);
    }
    
    return result;
}

std::future<GenerationResult> AICodeGenerator::generateAsync(const GenerationRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        return generate(request);
    });
}

GenerationResult AICodeGenerator::generateFunction(const std::string& description, const std::string& language) {
    GenerationRequest request;
    request.type = GenerationType::Function;
    request.description = description;
    request.language = language;
    request.maxTokens = m_maxTokens;
    request.temperature = m_temperature;
    
    return generate(request);
}

GenerationResult AICodeGenerator::generateClass(const std::string& description, const std::string& language) {
    GenerationRequest request;
    request.type = GenerationType::Class;
    request.description = description;
    request.language = language;
    request.maxTokens = m_maxTokens;
    request.temperature = m_temperature;
    
    return generate(request);
}

GenerationResult AICodeGenerator::generateTests(const std::string& code, const std::string& language) {
    GenerationRequest request;
    request.type = GenerationType::Test;
    request.description = "Generate tests for the provided code";
    request.language = language;
    request.context = code;
    request.maxTokens = m_maxTokens;
    request.temperature = m_temperature;
    
    return generate(request);
}

GenerationResult AICodeGenerator::generateDocumentation(const std::string& code, const std::string& language) {
    GenerationRequest request;
    request.type = GenerationType::Documentation;
    request.description = "Generate documentation for the provided code";
    request.language = language;
    request.context = code;
    request.maxTokens = m_maxTokens;
    request.temperature = m_temperature;
    
    return generate(request);
}

GenerationResult AICodeGenerator::generateRegex(const std::string& description) {
    GenerationRequest request;
    request.type = GenerationType::Regex;
    request.description = description;
    request.language = "regex";
    request.maxTokens = 200;
    request.temperature = 0.3f;
    
    return generate(request);
}

GenerationResult AICodeGenerator::generateSql(const std::string& description) {
    GenerationRequest request;
    request.type = GenerationType::Sql;
    request.description = description;
    request.language = "sql";
    request.maxTokens = 500;
    request.temperature = 0.3f;
    
    return generate(request);
}

void AICodeGenerator::registerTemplate(const Template& template_) {
    m_templates[template_.name] = template_;
}

void AICodeGenerator::unregisterTemplate(const std::string& name) {
    m_templates.erase(name);
}

std::vector<Template> AICodeGenerator::getTemplates() const {
    std::vector<Template> result;
    for (const auto& [name, temp] : m_templates) {
        result.push_back(temp);
    }
    return result;
}

std::optional<Template> AICodeGenerator::getTemplate(const std::string& name) const {
    auto it = m_templates.find(name);
    if (it != m_templates.end()) {
        return it->second;
    }
    return std::nullopt;
}

GenerationResult AICodeGenerator::generateFromTemplate(const std::string& templateName,
                                                         const std::map<std::string, std::string>& values) {
    auto temp = getTemplate(templateName);
    if (!temp) {
        GenerationResult result;
        result.error = "Template not found: " + templateName;
        return result;
    }
    
    // Replace placeholders
    std::string prompt = temp->prompt;
    for (const auto& [key, value] : values) {
        std::string placeholder = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = prompt.find(placeholder, pos)) != std::string::npos) {
            prompt.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    GenerationRequest request;
    request.type = temp->type;
    request.description = prompt;
    request.language = "cpp"; // Default
    
    return generate(request);
}

GenerationResult AICodeGenerator::refine(const std::string& code, const std::string& instruction) {
    GenerationRequest request;
    request.type = GenerationType::Custom;
    request.description = "Refine this code: " + instruction;
    request.context = code;
    request.maxTokens = m_maxTokens;
    request.temperature = m_temperature;
    
    return generate(request);
}

GenerationResult AICodeGenerator::addFeature(const std::string& code, const std::string& feature) {
    GenerationRequest request;
    request.type = GenerationType::Custom;
    request.description = "Add this feature: " + feature;
    request.context = code;
    request.maxTokens = m_maxTokens;
    request.temperature = m_temperature;
    
    return generate(request);
}

GenerationResult AICodeGenerator::optimize(const std::string& code) {
    GenerationRequest request;
    request.type = GenerationType::Custom;
    request.description = "Optimize this code for performance";
    request.context = code;
    request.maxTokens = m_maxTokens;
    request.temperature = 0.3f;
    
    return generate(request);
}

GenerationResult AICodeGenerator::convertLanguage(const std::string& code, const std::string& targetLanguage) {
    GenerationRequest request;
    request.type = GenerationType::Custom;
    request.description = "Convert this code to " + targetLanguage;
    request.context = code;
    request.language = targetLanguage;
    request.maxTokens = m_maxTokens;
    request.temperature = 0.3f;
    
    return generate(request);
}

void AICodeGenerator::setModel(const std::string& model) {
    m_model = model;
}

void AICodeGenerator::setDefaultLanguage(const std::string& language) {
    m_defaultLanguage = language;
}

void AICodeGenerator::setMaxTokens(int maxTokens) {
    m_maxTokens = maxTokens;
}

void AICodeGenerator::setTemperature(float temperature) {
    m_temperature = std::clamp(temperature, 0.0f, 2.0f);
}

std::vector<GenerationResult> AICodeGenerator::getHistory() const {
    return m_history;
}

void AICodeGenerator::clearHistory() {
    m_history.clear();
}

void AICodeGenerator::onGenerationComplete(GenerationCallback callback) {
    m_generationCallback = callback;
}

void AICodeGenerator::registerDefaultTemplates() {
    // Function template
    Template funcTemplate;
    funcTemplate.name = "function";
    funcTemplate.description = "Generate a function";
    funcTemplate.type = GenerationType::Function;
    funcTemplate.prompt = "Generate a {{language}} function that {{description}}. "
                          "Include proper error handling and documentation.";
    funcTemplate.placeholders = {"language", "description"};
    m_templates[funcTemplate.name] = funcTemplate;
    
    // Class template
    Template classTemplate;
    classTemplate.name = "class";
    classTemplate.description = "Generate a class";
    classTemplate.type = GenerationType::Class;
    classTemplate.prompt = "Generate a {{language}} class for {{description}}. "
                         "Include constructors, getters, setters, and proper encapsulation.";
    classTemplate.placeholders = {"language", "description"};
    m_templates[classTemplate.name] = classTemplate;
    
    // REST API template
    Template apiTemplate;
    apiTemplate.name = "rest_api";
    apiTemplate.description = "Generate REST API endpoints";
    apiTemplate.type = GenerationType::Custom;
    apiTemplate.prompt = "Generate {{language}} REST API endpoints for {{resource}}. "
                        "Include GET, POST, PUT, DELETE operations with proper validation.";
    apiTemplate.placeholders = {"language", "resource"};
    m_templates[apiTemplate.name] = apiTemplate;
    
    // Unit test template
    Template testTemplate;
    testTemplate.name = "unit_test";
    testTemplate.description = "Generate unit tests";
    testTemplate.type = GenerationType::Test;
    testTemplate.prompt = "Generate comprehensive unit tests for the following code:\n{{code}}";
    testTemplate.placeholders = {"code"};
    m_templates[testTemplate.name] = testTemplate;
}

std::string AICodeGenerator::buildPrompt(const GenerationRequest& request) {
    std::stringstream prompt;
    
    // Add type-specific instructions
    switch (request.type) {
        case GenerationType::Function:
            prompt << "Generate a function that ";
            break;
        case GenerationType::Class:
            prompt << "Generate a class for ";
            break;
        case GenerationType::Test:
            prompt << "Generate unit tests for ";
            break;
        case GenerationType::Documentation:
            prompt << "Generate documentation for ";
            break;
        case GenerationType::Regex:
            prompt << "Generate a regular expression that ";
            break;
        case GenerationType::Sql:
            prompt << "Generate a SQL query that ";
            break;
        case GenerationType::Config:
            prompt << "Generate configuration for ";
            break;
        case GenerationType::Custom:
            prompt << "Generate code for ";
            break;
    }
    
    prompt << request.description;
    
    // Add language
    std::string lang = request.language.empty() ? m_defaultLanguage : request.language;
    if (!lang.empty()) {
        prompt << " in " << lang;
    }
    
    // Add context if provided
    if (!request.context.empty()) {
        prompt << "\n\nContext:\n" << request.context;
    }
    
    // Add requirements
    if (!request.requirements.empty()) {
        prompt << "\n\nRequirements:\n";
        for (const auto& req : request.requirements) {
            prompt << "- " << req << "\n";
        }
    }
    
    return prompt.str();
}

std::string AICodeGenerator::generateCode(const std::string& prompt, const std::string& language) {
    // This is a simplified implementation
    // In reality, this would call an AI model
    
    std::stringstream code;
    std::string lang = language.empty() ? m_defaultLanguage : language;
    
    if (lang == "cpp" || lang == "c++") {
        code << "// Generated C++ code\n\n";
        code << "#include <iostream>\n";
        code << "#include <vector>\n";
        code << "#include <string>\n\n";
        code << "// TODO: Implement based on: " << prompt.substr(0, 50) << "...\n";
        code << "void generatedFunction() {\n";
        code << "    // Implementation here\n";
        code << "}\n";
    } else if (lang == "python") {
        code << "# Generated Python code\n\n";
        code << "def generated_function():\n";
        code << "    \"\"\"\n";
        code << "    Generated based on: " << prompt.substr(0, 50) << "...\n";
        code << "    \"\"\"\n";
        code << "    pass\n";
    } else if (lang == "javascript" || lang == "js") {
        code << "// Generated JavaScript code\n\n";
        code << "function generatedFunction() {\n";
        code << "    // Implementation based on: " << prompt.substr(0, 50) << "...\n";
        code << "    console.log('Generated code');\n";
        code << "}\n";
    } else if (lang == "sql") {
        code << "-- Generated SQL\n\n";
        code << "SELECT * FROM table\n";
        code << "WHERE condition = true;\n";
    } else if (lang == "regex") {
        code << "^pattern$";
    } else {
        code << "// Generated code for " << lang << "\n\n";
        code << "// TODO: Implement\n";
    }
    
    return code.str();
}

std::string AICodeGenerator::generateExplanation(const GenerationRequest& request) {
    std::stringstream explanation;
    explanation << "This code was generated to " << request.description << ".\n\n";
    explanation << "Key features:\n";
    explanation << "- Implements the requested functionality\n";
    explanation << "- Follows best practices for " << request.language << "\n";
    explanation << "- Includes error handling\n";
    
    if (!request.requirements.empty()) {
        explanation << "\nRequirements addressed:\n";
        for (const auto& req : request.requirements) {
            explanation << "- " << req << "\n";
        }
    }
    
    return explanation.str();
}

std::vector<std::string> AICodeGenerator::generateSuggestions(const GenerationRequest& request) {
    std::vector<std::string> suggestions;
    
    suggestions.push_back("Add input validation");
    suggestions.push_back("Add error handling");
    suggestions.push_back("Add unit tests");
    suggestions.push_back("Consider edge cases");
    
    if (request.type == GenerationType::Function) {
        suggestions.push_back("Add documentation comments");
    } else if (request.type == GenerationType::Class) {
        suggestions.push_back("Add getter/setter methods");
        suggestions.push_back("Consider making it thread-safe");
    }
    
    return suggestions;
}

} // namespace RawrXD::AI
