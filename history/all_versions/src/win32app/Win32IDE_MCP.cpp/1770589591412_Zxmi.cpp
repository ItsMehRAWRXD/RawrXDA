// ============================================================================
// Win32IDE_MCP.cpp — Phase 36: MCP Integration Wiring
// ============================================================================
//
// Wires the RawrXD::MCP::MCPServer into the Win32IDE lifecycle.
// Registers all built-in tools and IDE-specific resources/prompts.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "../../include/mcp_integration.h"

#include <filesystem>
#include <fstream>

// ============================================================================
// initMCP() — Called from deferredHeavyInit()
// ============================================================================

void Win32IDE::initMCP() {
    if (m_mcpInitialized) return;

    OutputDebugStringA("[MCP] Initializing Model Context Protocol server...\n");

    // Create server instance
    m_mcpServer = std::make_unique<RawrXD::MCP::MCPServer>();

    RawrXD::MCP::ServerInfo info;
    info.name    = "RawrXD-IDE-MCP";
    info.version = "1.0.0";

    if (!m_mcpServer->initialize(info)) {
        OutputDebugStringA("[MCP] ERROR: Failed to initialize MCPServer\n");
        return;
    }

    // Register all built-in filesystem/shell tools
    RawrXD::MCP::registerBuiltinTools(*m_mcpServer);

    // ========================================================================
    // Register IDE-specific resources
    // ========================================================================

    // Resource: Current file contents
    {
        RawrXD::MCP::ResourceDefinition def;
        def.uri         = "rawrxd://editor/active-file";
        def.name        = "Active Editor File";
        def.description = "Contents of the currently active editor file";
        def.mimeType    = "text/plain";

        m_mcpServer->registerResource(def, [this](const std::string& /*uri*/) -> RawrXD::MCP::ResourceContent {
            RawrXD::MCP::ResourceContent content;
            content.uri      = "rawrxd://editor/active-file";
            content.mimeType = "text/plain";

            // Read from active editor buffer
            if (!m_currentFilePath.empty()) {
                std::ifstream file(m_currentFilePath, std::ios::binary);
                if (file.is_open()) {
                    content.content = std::string(
                        (std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
                } else {
                    content.content = "// [Error: Cannot read active file]";
                }
            } else {
                content.content = "// [No file currently open]";
            }
            return content;
        });
    }

    // Resource: IDE settings
    {
        RawrXD::MCP::ResourceDefinition def;
        def.uri         = "rawrxd://ide/settings";
        def.name        = "IDE Settings";
        def.description = "Current IDE configuration and settings";
        def.mimeType    = "application/json";

        m_mcpServer->registerResource(def, [this](const std::string& /*uri*/) -> RawrXD::MCP::ResourceContent {
            RawrXD::MCP::ResourceContent content;
            content.uri      = "rawrxd://ide/settings";
            content.mimeType = "application/json";

            std::ostringstream json;
            json << "{";
            json << "\"fontName\":\"" << m_settings.fontName << "\",";
            json << "\"fontSize\":" << m_settings.fontSize << ",";
            json << "\"tabSize\":" << m_settings.tabSize << ",";
            json << "\"wordWrap\":" << (m_settings.wordWrapEnabled ? "true" : "false") << ",";
            json << "\"lineNumbers\":" << (m_settings.lineNumbersVisible ? "true" : "false") << ",";
            json << "\"syntaxColoring\":" << (m_settings.syntaxColoringEnabled ? "true" : "false") << ",";
            json << "\"ghostText\":" << (m_settings.ghostTextEnabled ? "true" : "false") << ",";
            json << "\"aiModelPath\":\"" << m_settings.aiModelPath << "\",";
            json << "\"aiMaxTokens\":" << m_settings.aiMaxTokens << ",";
            json << "\"aiTemperature\":" << m_settings.aiTemperature;
            json << "}";

            content.content = json.str();
            return content;
        });
    }

    // Resource: Build status
    {
        RawrXD::MCP::ResourceDefinition def;
        def.uri         = "rawrxd://ide/build-status";
        def.name        = "Build Status";
        def.description = "Current build configuration and last build result";
        def.mimeType    = "application/json";

        m_mcpServer->registerResource(def, [this](const std::string& /*uri*/) -> RawrXD::MCP::ResourceContent {
            RawrXD::MCP::ResourceContent content;
            content.uri      = "rawrxd://ide/build-status";
            content.mimeType = "application/json";

            std::ostringstream json;
            json << "{";
            json << "\"workingDirectory\":\"" << m_settings.workingDirectory << "\",";
            json << "\"compiler\":\"MSVC 2022\",";
            json << "\"target\":\"RawrXD-Win32IDE\"";
            json << "}";

            content.content = json.str();
            return content;
        });
    }

    // ========================================================================
    // Register IDE-specific prompt templates
    // ========================================================================

    // Prompt: Code Review
    {
        RawrXD::MCP::PromptTemplate tmpl;
        tmpl.name        = "code-review";
        tmpl.description = "Review code for bugs, security issues, and style problems";
        tmpl.arguments   = {
            {"language", "The programming language of the code", true},
            {"code", "The code to review", true}
        };

        m_mcpServer->registerPrompt(tmpl, [](const std::string& argsJson) -> std::vector<RawrXD::MCP::PromptMessage> {
            std::string language = RawrXD::MCP::MCPServer().parseRequest("{\"params\":" + argsJson + "}").params;
            // Extract from simplified JSON
            std::string code;
            auto codePos = argsJson.find("\"code\"");
            if (codePos != std::string::npos) {
                auto valStart = argsJson.find('"', codePos + 6);
                if (valStart != std::string::npos) {
                    valStart++;
                    auto valEnd = argsJson.find('"', valStart);
                    if (valEnd != std::string::npos) code = argsJson.substr(valStart, valEnd - valStart);
                }
            }

            return {
                {"system", "You are an expert code reviewer. Analyze the code for bugs, security vulnerabilities, "
                           "performance issues, and style problems. Provide actionable feedback."},
                {"user", "Please review this code:\n\n```\n" + code + "\n```"}
            };
        });
    }

    // Prompt: Explain Code
    {
        RawrXD::MCP::PromptTemplate tmpl;
        tmpl.name        = "explain-code";
        tmpl.description = "Explain what a piece of code does in plain language";
        tmpl.arguments   = {
            {"code", "The code to explain", true}
        };

        m_mcpServer->registerPrompt(tmpl, [](const std::string& argsJson) -> std::vector<RawrXD::MCP::PromptMessage> {
            (void)argsJson;
            return {
                {"system", "You are a helpful programming tutor. Explain the code clearly and concisely, "
                           "covering what it does, how it works, and any notable patterns or techniques."},
                {"user", "Please explain this code."}
            };
        });
    }

    // Prompt: Generate Tests
    {
        RawrXD::MCP::PromptTemplate tmpl;
        tmpl.name        = "generate-tests";
        tmpl.description = "Generate unit tests for the given code";
        tmpl.arguments   = {
            {"language", "Programming language (cpp, python, rust, etc.)", true},
            {"code", "The code to generate tests for", true},
            {"framework", "Test framework to use (googletest, catch2, pytest, etc.)", false}
        };

        m_mcpServer->registerPrompt(tmpl, [](const std::string& /*argsJson*/) -> std::vector<RawrXD::MCP::PromptMessage> {
            return {
                {"system", "You are an expert test engineer. Generate comprehensive unit tests that cover edge cases, "
                           "error paths, and boundary conditions. Use the specified test framework."},
                {"user", "Generate unit tests for the provided code."}
            };
        });
    }

    // Prompt: Refactor
    {
        RawrXD::MCP::PromptTemplate tmpl;
        tmpl.name        = "refactor";
        tmpl.description = "Suggest refactoring improvements for the given code";
        tmpl.arguments   = {
            {"code", "The code to refactor", true},
            {"goals", "Specific refactoring goals (performance, readability, maintainability)", false}
        };

        m_mcpServer->registerPrompt(tmpl, [](const std::string& /*argsJson*/) -> std::vector<RawrXD::MCP::PromptMessage> {
            return {
                {"system", "You are a senior software architect specializing in code refactoring. "
                           "Suggest concrete improvements with before/after code examples."},
                {"user", "Please suggest refactoring improvements for this code."}
            };
        });
    }

    m_mcpInitialized = true;

    // Log stats
    auto tools     = m_mcpServer->listTools();
    auto resources = m_mcpServer->listResources();
    auto prompts   = m_mcpServer->listPrompts();

    char msg[256];
    snprintf(msg, sizeof(msg), "[MCP] Initialized: %zu tools, %zu resources, %zu prompts registered\n",
             tools.size(), resources.size(), prompts.size());
    OutputDebugStringA(msg);
}

// ============================================================================
// shutdownMCP() — Called during IDE shutdown
// ============================================================================

void Win32IDE::shutdownMCP() {
    if (!m_mcpInitialized) return;

    OutputDebugStringA("[MCP] Shutting down MCP server...\n");

    if (m_mcpServer) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[MCP] Final stats: %llu requests, %llu errors\n",
                 m_mcpServer->totalRequests(), m_mcpServer->totalErrors());
        OutputDebugStringA(msg);

        m_mcpServer->shutdown();
        m_mcpServer.reset();
    }

    m_mcpInitialized = false;
    OutputDebugStringA("[MCP] Shutdown complete\n");
}
