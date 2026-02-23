// Integration.cpp - Main Agent Implementation
// Pure C++20 / Win32 - Zero Qt Dependencies
// Compile: cl /std:c++20 /EHsc /O2 Integration.cpp /link winhttp.lib ws2_32.lib comctl32.lib ole32.lib shell32.lib shlwapi.lib

#include "StdReplacements.hpp"
#include "RawrXD_Agent.hpp"
#include "../src/masm/RawrXD_NativeHttpServer.h"
#include <iostream>
#include <algorithm>
#include <cctype>

namespace {
    RawrXD::NativeHttpServerPtr g_httpServer;
    volatile bool g_consoleExitRequested = false;

    BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
        if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
            g_consoleExitRequested = true;
            return TRUE;
        }
        return FALSE;
    }

    static void TrimInput(RawrXD::String& s) {
        auto start = s.find_first_not_of(L" \t\r\n");
        if (start == RawrXD::String::npos) {
            s.clear();
            return;
        }
        auto end = s.find_last_not_of(L" \t\r\n");
        s = s.substr(start, end == RawrXD::String::npos ? RawrXD::String::npos : end - start + 1);
    }

    bool InitializeHttpServer(const RawrXD::AgentConfig& config) {
        try {
            g_httpServer = RawrXD::CreateNativeHttpServer(NATIVE_HTTP_SERVER_DEFAULT_PORT);
            if (!g_httpServer || !g_httpServer->IsRunning()) {
                OutputDebugStringA("RawrXD HTTP server failed to start.\n");
                return false;
            }

            if (!config.model.empty()) {
                g_httpServer->LoadModel(RawrXD::StringUtils::ToUtf8(config.model));
            }

            OutputDebugStringA("RawrXD HTTP server started on port 23959.\n");
            return true;
        } catch (const std::exception& ex) {
            std::string message = "RawrXD HTTP server init error: ";
            message += ex.what();
            message += "\n";
            std::cerr << message;
            OutputDebugStringA(message.c_str());
#ifdef _CONSOLE
            (void)0; // CLI: error already on stderr
#else
            MessageBoxA(nullptr, message.c_str(), "HTTP Server Init Error", MB_ICONERROR | MB_OK);
#endif
            return false;
        }
    }

    void ShutdownHttpServer() {
        g_httpServer.reset();
    }
}

using namespace RawrXD;

// Console mode agent
class ConsoleAgent {
public:
    bool initialize(const AgentConfig& config = AgentConfig()) {
        if (!initializeAgent()) {
            std::wcerr << L"Failed to initialize agent\n";
            return false;
        }

        m_agent = std::make_unique<Agent>();
        if (!m_agent->initialize(config)) {
            std::wcerr << L"Failed to initialize agent components\n";
            return false;
        }

        // Set up event callback
        m_agent->setEventCallback([this](const AgentEvent& event) {
            handleEvent(event);
        });

        return true;
    }

    void run() {
        std::wcout << L"RawrXD CLI " << AGENT_VERSION << L" - Full Chat & Agentic (101% Win32 GUI parity)\n";
        std::wcout << L"Commands: quit|exit, clear, models, /tools, /status, /smoke, /run-tool <name> [json]\n";
        std::wcout << L"Agentic: tools auto-invoked (read_file, run_command, search_files, etc.)\n";
        std::wcout << L"----------------------------------------\n\n";

        // Check LLM availability
        if (m_agent->isLLMAvailable()) {
            std::wcout << L"[Connected to Ollama]\n";
            auto models = m_agent->listModels();
            if (!models.empty()) {
                std::wcout << L"Available models:\n";
                for (const auto& m : models) {
                    std::wcout << L"  - " << m.c_str() << L"\n";
                }
            }
        } else {
            std::wcout << L"[Warning: Ollama not available]\n";
        }

        std::wcout << L"\n";

        std::wstring input;
        while (!g_consoleExitRequested) {
            std::wcout << L"You: ";
            if (!std::getline(std::wcin, input)) {
                break;  // EOF (e.g. pipe closed)
            }
            TrimInput(input);

            if (input == L"quit" || input == L"exit") {
                break;
            }

            if (input == L"help" || input == L"/help" || input == L"?") {
                std::wcout << L"Commands: quit|exit, clear, models, /tools, /status, /smoke, /run-tool <name> [json]\n";
                std::wcout << L"  /tools          List all agent tools\n";
                std::wcout << L"  /status        Show agent state, model, LLM, tool count\n";
                std::wcout << L"  /smoke         Run agentic smoke test (create agent_smoke_test.txt)\n";
                std::wcout << L"  /run-tool n j  Run tool by name with optional JSON params\n\n";
                continue;
            }

            if (input == L"clear") {
                m_agent->clearConversation();
                std::wcout << L"[Conversation cleared]\n\n";
                continue;
            }

            if (input == L"models" || input == L"/models") {
                if (m_agent->isLLMAvailable()) {
                    auto models = m_agent->listModels();
                    std::wcout << L"Available models:\n";
                    for (const auto& m : models) {
                        std::wcout << L"  - " << m.c_str() << L"\n";
                    }
                } else {
                    std::wcout << L"[Ollama not available - start with: ollama serve]\n";
                }
                std::wcout << L"\n";
                continue;
            }

            // /tools - list all tools (mirrors GUI View > Agent Tools)
            if (input == L"/tools" || input == L"tools") {
                if (m_agent->toolEngine()) {
                    auto defs = m_agent->toolEngine()->getAllToolDefinitions();
                    std::wcout << L"Tools (" << defs.size() << L"):\n";
                    for (const auto& d : defs) {
                        std::wcout << L"  " << d.name.c_str() << L" - " << d.description.c_str();
                        if (d.isDangerous) std::wcout << L" [dangerous]";
                        std::wcout << L"\n";
                    }
                } else {
                    std::wcout << L"[Tool engine not initialized]\n";
                }
                std::wcout << L"\n";
                continue;
            }

            // /status - agent status (mirrors GUI Agent > View Status)
            if (input == L"/status" || input == L"status") {
                String stateStr = agentStateToString(m_agent->state());
                std::wcout << L"State: " << stateStr.c_str() << L"\n";
                std::wcout << L"Model: " << m_agent->config().model.c_str() << L"\n";
                std::wcout << L"LLM: " << (m_agent->isLLMAvailable() ? L"connected" : L"not available") << L"\n";
                if (m_agent->toolEngine()) {
                    auto defs = m_agent->toolEngine()->getAllToolDefinitions();
                    std::wcout << L"Tools: " << defs.size() << L" registered\n";
                }
                std::wcout << L"\n";
                continue;
            }

            // /smoke - run agentic smoke test (mirrors Win32 Agent > Run Smoke Test)
            if (input == L"/smoke" || input == L"smoke") {
                String smokePrompt = L"SMOKE TEST: Create a file named 'agent_smoke_test.txt' in the current working directory. "
                    L"Content should be 'RawrXD Agentic Smoke Test Successful at ' + [Current Timestamp]. "
                    L"After creating the file, run 'dir' or 'ls' to verify it exists and output the result. "
                    L"Finally, read the file back to confirm content integrity.";
                std::wcout << L"[Smoke] Running agentic smoke test (same as Win32 Agent > Run Smoke Test)...\n";
                std::wcout << L"[Smoke] Goal: Verify multi-step tools (filesystem + shell).\n\n";
                m_isProcessing = true;
                m_agent->processMessage(smokePrompt);
                while (m_isProcessing) {
                    Sleep(100);
                }
                std::wcout << L"[Smoke] Done. Check agent_smoke_test.txt for verification.\n\n";
                continue;
            }

            // /run-tool <name> [json] - direct tool execution (mirrors GUI Agent > Run Tool)
            if ((input.size() > 10 && input.substr(0, 10) == L"/run-tool ") ||
                (input.size() > 9 && input.substr(0, 9) == L"run-tool ")) {
                size_t nameStart = (input[0] == L'/') ? 10 : 9;
                size_t spacePos = input.find(L' ', nameStart);
                String toolName = spacePos != String::npos ? input.substr(nameStart, spacePos - nameStart) : input.substr(nameStart);
                String jsonParams = spacePos != String::npos && spacePos + 1 < input.size() ? input.substr(spacePos + 1) : L"{}";
                if (toolName.empty()) {
                    std::wcout << L"Usage: /run-tool <name> [json params]\n";
                    std::wcout << L"Example: /run-tool list_directory {\"path\":\"C:\\\\temp\"}\n\n";
                    continue;
                }
                JsonObject params;
                std::string utf8Params = StringUtils::ToUtf8(jsonParams);
                auto parsed = JsonParser::Parse(utf8Params);
                if (parsed && std::holds_alternative<JsonObject>(*parsed)) {
                    params = std::get<JsonObject>(*parsed);
                }
                ToolResult res = m_agent->executeTool(toolName, params);
                if (res.isSuccess()) {
                    if (res.output.has_value()) {
                        std::string outStr = JsonParser::Serialize(res.output, 2);
                        std::wcout << L"[Tool " << toolName.c_str() << L"] " << StringUtils::FromUtf8(outStr).c_str() << L"\n";
                    } else {
                        std::wcout << L"[Tool " << toolName.c_str() << L"] ok\n";
                    }
                } else {
                    std::wcerr << L"[Tool " << toolName.c_str() << L"] Error: " << res.errorMessage.c_str() << L"\n";
                }
                std::wcout << L"\n";
                continue;
            }

            if (input.empty()) {
                continue;
            }

            m_isProcessing = true;
            m_agent->processMessage(String(input));

            // Wait for completion
            while (m_isProcessing) {
                Sleep(100);
            }

            std::wcout << L"\n";
        }
    }

private:
    void handleEvent(const AgentEvent& event) {
        switch (event.type) {
            case AgentEvent::Type::StateChanged:
                // Silent
                break;

            case AgentEvent::Type::MessageReceived:
                std::wcout << L"\nAssistant: " << event.message.c_str() << L"\n";
                break;

            case AgentEvent::Type::ToolCalled:
                std::wcout << L"\n[Calling tool: " << event.message.c_str() << L"]\n";
                break;

            case AgentEvent::Type::ToolResult:
                std::wcout << L"[Tool completed]\n";
                break;

            case AgentEvent::Type::Error:
                std::wcerr << L"\n[Error: " << event.message.c_str() << L"]\n";
                m_isProcessing = false;
                break;

            case AgentEvent::Type::StreamChunk:
                std::wcout << event.message.c_str();
                std::wcout.flush();
                break;

            case AgentEvent::Type::Completed:
                m_isProcessing = false;
                break;
        }
    }

    UniquePtr<Agent> m_agent;
    std::atomic<bool> m_isProcessing{false};
};

// Entry point for console mode
int wmain(int argc, wchar_t* argv[]) {
    // Set console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    AgentConfig config;

    // Parse command line arguments (C++20: String = std::wstring)
    for (int i = 1; i < argc; ++i) {
        String arg(argv[i]);

        if (arg == L"--model" && i + 1 < argc) {
            config.model = String(argv[++i]);
        } else if (arg == L"--host" && i + 1 < argc) {
            config.ollamaHost = String(argv[++i]);
        } else if (arg == L"--port" && i + 1 < argc) {
            config.ollamaPort = std::stoi(argv[++i]);
        } else if (arg == L"--dir" && i + 1 < argc) {
            config.workingDirectory = String(argv[++i]);
        } else if (arg == L"--auto-approve") {
            config.autoApproveTools = true;
        } else if (arg == L"--version" || arg == L"-v") {
            std::wcout << L"RawrXD CLI " << AGENT_VERSION << L" - Full Chat & Agentic (100% Win32 GUI parity)\n";
            std::wcout << L"Build: " << AGENT_BUILD_DATE << L" | " << AGENT_NAME << L"\n";
            return 0;
        } else if (arg == L"--help" || arg == L"-h") {
            std::wcout << L"RawrXD CLI / Agent " << AGENT_VERSION << L" - Full Chat & Agentic (101% Win32 GUI parity)\n\n";
            std::wcout << L"Pure CLI with 101% feature parity to Win32 GUI:\n";
            std::wcout << L"  - Full interactive chat (streaming)\n";
            std::wcout << L"  - Agentic autonomous mode (tool calling, multi-step execution)\n";
            std::wcout << L"  - All 44+ tools: read_file, write_file, run_command, search_files, etc.\n";
            std::wcout << L"  - In-chat: /tools, /status, /smoke, /run-tool <name> [json]\n\n";
            std::wcout << L"Usage: RawrXD_CLI [options]  (or RawrXD_Agent_Console)\n\n";
            std::wcout << L"Options:\n";
            std::wcout << L"  --model <name>    Set LLM model (default: qwen2.5-coder:14b)\n";
            std::wcout << L"  --host <host>     Ollama host (default: localhost)\n";
            std::wcout << L"  --port <port>     Ollama port (default: 11434)\n";
            std::wcout << L"  --dir <path>      Working directory\n";
            std::wcout << L"  --auto-approve    Auto-approve dangerous tools\n";
            std::wcout << L"  --list, -l        List available models and exit\n";
            std::wcout << L"  --version, -v     Show version and exit\n";
            std::wcout << L"  --help, -h        Show this help\n";
            return 0;
        } else if (arg == L"--list" || arg == L"-l") {
            // List models and exit - requires init, so we do minimal init
            if (!initializeAgent()) {
                std::wcerr << L"Failed to initialize\n";
                return 1;
            }
            Agent tempAgent;
            if (!tempAgent.initialize(config)) {
                std::wcerr << L"Failed to initialize agent\n";
                return 1;
            }
            if (tempAgent.isLLMAvailable()) {
                auto models = tempAgent.listModels();
                std::wcout << L"Available models:\n";
                for (const auto& m : models) {
                    std::wcout << L"  - " << m.c_str() << L"\n";
                }
            } else {
                std::wcout << L"Ollama not available. Start Ollama (ollama serve) first.\n";
            }
            return 0;
        }
    }

    ConsoleAgent agent;
    if (!agent.initialize(config)) {
        return 1;
    }

    if (!InitializeHttpServer(config)) {
        std::cerr << "Failed to start HTTP server. Some functionality may be limited.\n";
    }

    agent.run();
    ShutdownHttpServer();
    return 0;
}

// Windows GUI entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR cmdLine, int) {
    AgentConfig config;

    // Parse command line for GUI mode
    int argc;
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);
    if (argv) {
        for (int i = 0; i < argc; ++i) {
            String arg(argv[i]);
            if (arg == L"--model" && i + 1 < argc) {
                config.model = String(argv[++i]);
            } else if (arg == L"--dir" && i + 1 < argc) {
                config.workingDirectory = String(argv[++i]);
            }
        }
        LocalFree(argv);
    }

    AgentApp app(hInstance);
    if (!InitializeHttpServer(config)) {
        MessageBoxW(nullptr, L"Failed to start HTTP server", L"Error", MB_ICONERROR);
        return 1;
    }
    if (!app.initialize(config)) {
        MessageBoxW(nullptr, L"Failed to initialize agent", L"Error", MB_ICONERROR);
        ShutdownHttpServer();
        return 1;
    }

    int exitCode = app.run();
    ShutdownHttpServer();
    return exitCode;
}
