// ============================================================================
// Auto Documenter — Automated Documentation Generation
// Generates comprehensive documentation from code and specifications
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../editor/syntax_highlighter.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <fstream>

namespace RawrXD::Docs {

enum class DocFormat {
    MARKDOWN,
    HTML,
    PDF,
    XML
};

struct APIEndpoint {
    std::string path;
    std::string method;
    std::string description;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::string returnType;
    std::vector<std::string> examples;
};

struct APIChanges {
    std::vector<APIEndpoint> added;
    std::vector<APIEndpoint> modified;
    std::vector<APIEndpoint> removed;
    std::chrono::system_clock::time_point timestamp;
};

struct SystemLayout {
    std::string name;
    std::vector<std::string> components;
    std::map<std::string, std::vector<std::string>> dependencies;
    std::vector<std::pair<std::string, std::string>> connections;
};

struct DocSpec {
    DocFormat format;
    bool includeExamples;
    bool includeDiagrams;
    bool includeChangelog;
    std::string targetAudience;
};

struct Documentation {
    std::string title;
    std::string content;
    DocFormat format;
    std::map<std::string, std::string> sections;
    std::chrono::system_clock::time_point generatedAt;
    std::vector<std::string> generatedFiles;
};

class AutoDocumenter {
public:
    explicit AutoDocumenter(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {}

    Documentation GenerateDocs(const std::string& code, const DocSpec& spec) {
        Documentation docs;
        docs.format = spec.format;
        docs.generatedAt = std::chrono::system_clock::now();
        
        // Parse code structure
        auto functions = ParseFunctions(code);
        auto classes = ParseClasses(code);
        
        // Generate documentation sections
        docs.sections["overview"] = GenerateOverview(code);
        docs.sections["api_reference"] = GenerateAPIReference(functions, spec);
        
        if (spec.includeExamples) {
            docs.sections["examples"] = GenerateExamples(functions);
        }
        
        if (spec.includeChangelog) {
            docs.sections["changelog"] = GenerateChangelog();
        }
        
        // AI-enhanced documentation
        if (m_aiClient && m_aiClient->IsLoaded()) {
            docs.sections["ai_enhanced"] = GenerateAIEnhancedDocs(code);
        }
        
        // Compile final documentation
        docs.content = CompileDocumentation(docs, spec);
        
        return docs;
    }

    void UpdateAPIDocumentation(const APIChanges& changes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update changelog
        std::ostringstream changelog;
        changelog << "## API Changes - " << FormatTime(changes.timestamp) << "\n\n";
        
        if (!changes.added.empty()) {
            changelog << "### Added\n";
            for (const auto& endpoint : changes.added) {
                changelog << "- `" << endpoint.method << " " << endpoint.path << "`\n";
            }
            changelog << "\n";
        }
        
        if (!changes.modified.empty()) {
            changelog << "### Modified\n";
            for (const auto& endpoint : changes.modified) {
                changelog << "- `" << endpoint.method << " " << endpoint.path << "`\n";
            }
            changelog << "\n";
        }
        
        if (!changes.removed.empty()) {
            changelog << "### Removed\n";
            for (const auto& endpoint : changes.removed) {
                changelog << "- `" << endpoint.method << " " << endpoint.path << "`\n";
            }
            changelog << "\n";
        }
        
        m_changelog += changelog.str();
    }

    std::string GenerateArchitectureDiagrams(const SystemLayout& layout) {
        std::ostringstream diagram;
        
        // Generate Mermaid diagram
        diagram << "```mermaid\n";
        diagram << "graph TD\n";
        
        // Add components
        for (const auto& component : layout.components) {
            diagram << "    " <> component << "[" << component << "]\n";
        }
        
        // Add connections
        for (const auto& [from, to] : layout.connections) {
            diagram << "    " << from << " --> " << to << "\n";
        }
        
        diagram << "```\n";
        
        return diagram.str();
    }

    bool ExportDocumentation(const Documentation& docs, const std::string& outputPath) {
        std::string extension;
        switch (docs.format) {
            case DocFormat::MARKDOWN: extension = ".md"; break;
            case DocFormat::HTML: extension = ".html"; break;
            case DocFormat::PDF: extension = ".pdf"; break;
            case DocFormat::XML: extension = ".xml"; break;
        }
        
        std::string filename = outputPath + "/documentation" + extension;
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << docs.content;
        file.close();
        
        return true;
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::string m_changelog;

    struct FunctionInfo {
        std::string name;
        std::string returnType;
        std::vector<std::pair<std::string, std::string>> parameters;
        std::string description;
        std::string docComment;
    };

    struct ClassInfo {
        std::string name;
        std::vector<FunctionInfo> methods;
        std::vector<std::pair<std::string, std::string>> members;
        std::string description;
    };

    std::vector<FunctionInfo> ParseFunctions(const std::string& code) {
        std::vector<FunctionInfo> functions;
        
        // Parse function declarations
        std::regex funcPattern(R"((\w+)\s+(\w+)\s*\(([^)]*)\))");
        std::sregex_iterator iter(code.begin(), code.end(), funcPattern);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            FunctionInfo func;
            func.returnType = (*iter)[1];
            func.name = (*iter)[2];
            
            // Parse parameters
            std::string params = (*iter)[3];
            // ... parameter parsing logic
            
            functions.push_back(func);
        }
        
        return functions;
    }

    std::vector<ClassInfo> ParseClasses(const std::string& code) {
        std::vector<ClassInfo> classes;
        
        // Parse class declarations
        std::regex classPattern(R"(class\s+(\w+))");
        std::sregex_iterator iter(code.begin(), code.end(), classPattern);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            ClassInfo cls;
            cls.name = (*iter)[1];
            classes.push_back(cls);
        }
        
        return classes;
    }

    std::string GenerateOverview(const std::string& code) {
        std::ostringstream overview;
        overview << "# Overview\n\n";
        overview << "This document provides comprehensive API documentation.\n\n";
        return overview.str();
    }

    std::string GenerateAPIReference(const std::vector<FunctionInfo>& functions,
                                    const DocSpec& spec) {
        std::ostringstream reference;
        reference << "# API Reference\n\n";
        
        for (const auto& func : functions) {
            reference << "## " << func.name << "\n\n";
            reference << func.description << "\n\n";
            reference << "**Returns:** " << func.returnType << "\n\n";
            
            if (!func.parameters.empty()) {
                reference << "**Parameters:**\n";
                for (const auto& [type, name] : func.parameters) {
                    reference << "- `" << name << "`: " << type << "\n";
                }
                reference << "\n";
            }
        }
        
        return reference.str();
    }

    std::string GenerateExamples(const std::vector<FunctionInfo>& functions) {
        std::ostringstream examples;
        examples << "# Examples\n\n";
        
        for (const auto& func : functions) {
            examples << "## " << func.name << "\n\n";
            examples << "```cpp\n";
            examples << "// Example usage of " << func.name << "\n";
            examples << "auto result = " << func.name << "();\n";
            examples << "```\n\n";
        }
        
        return examples.str();
    }

    std::string GenerateChangelog() {
        std::ostringstream changelog;
        changelog << "# Changelog\n\n";
        changelog << m_changelog;
        return changelog.str();
    }

    std::string GenerateAIEnhancedDocs(const std::string& code) {
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return "";
        }

        std::string prompt = "Generate enhanced documentation for this code:\n```\n" + 
                            code.substr(0, 1000) + "\n```";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a technical documentation expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            return "## AI-Enhanced Documentation\n\n" + result.response;
        }
        
        return "";
    }

    std::string CompileDocumentation(const Documentation& docs, const DocSpec& spec) {
        std::ostringstream compiled;
        
        // Add header
        compiled << "# " << docs.title << "\n\n";
        compiled << "*Generated: " << FormatTime(docs.generatedAt) << "*\n\n";
        
        // Add sections
        for (const auto& [section, content] : docs.sections) {
            compiled << content << "\n\n";
        }
        
        return compiled.str();
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Docs
