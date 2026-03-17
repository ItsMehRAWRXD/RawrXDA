// Integration.cpp - Main Agent Implementation
// Pure C++20 / Win32 - Zero Qt Dependencies
// Compile: cl /std:c++20 /EHsc /O2 Integration.cpp /link winhttp.lib ws2_32.lib comctl32.lib ole32.lib shell32.lib shlwapi.lib

#include "RawrXD_Agent.hpp"
#include <iostream>

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
        std::wcout << L"RawrXD Agent " << AGENT_VERSION << L"\n";
        std::wcout << L"Type 'quit' to exit, 'clear' to clear history\n";
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
        while (true) {
            std::wcout << L"You: ";
            std::getline(std::wcin, input);

            if (input == L"quit" || input == L"exit") {
                break;
            }

            if (input == L"clear") {
                m_agent->clearConversation();
                std::wcout << L"[Conversation cleared]\n\n";
                continue;
            }

            if (input.empty()) {
                continue;
            }

            m_isProcessing = true;
            m_agent->processMessage(QString(input));

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

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        QString arg(argv[i]);

        if (arg == L"--model" && i + 1 < argc) {
            config.model = QString(argv[++i]);
        } else if (arg == L"--host" && i + 1 < argc) {
            config.ollamaHost = QString(argv[++i]);
        } else if (arg == L"--port" && i + 1 < argc) {
            config.ollamaPort = std::stoi(argv[++i]);
        } else if (arg == L"--dir" && i + 1 < argc) {
            config.workingDirectory = QString(argv[++i]);
        } else if (arg == L"--auto-approve") {
            config.autoApproveTools = true;
        } else if (arg == L"--help" || arg == L"-h") {
            std::wcout << L"RawrXD Agent " << AGENT_VERSION << L"\n\n";
            std::wcout << L"Usage: RawrXD_Agent [options]\n\n";
            std::wcout << L"Options:\n";
            std::wcout << L"  --model <name>    Set LLM model (default: qwen2.5-coder:14b)\n";
            std::wcout << L"  --host <host>     Ollama host (default: localhost)\n";
            std::wcout << L"  --port <port>     Ollama port (default: 11434)\n";
            std::wcout << L"  --dir <path>      Working directory\n";
            std::wcout << L"  --auto-approve    Auto-approve dangerous tools\n";
            std::wcout << L"  --help            Show this help\n";
            return 0;
        }
    }

    ConsoleAgent agent;
    if (!agent.initialize(config)) {
        return 1;
    }

    agent.run();
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
            QString arg(argv[i]);
            if (arg == L"--model" && i + 1 < argc) {
                config.model = QString(argv[++i]);
            } else if (arg == L"--dir" && i + 1 < argc) {
                config.workingDirectory = QString(argv[++i]);
            }
        }
        LocalFree(argv);
    }

    AgentApp app(hInstance);
    if (!app.initialize(config)) {
        MessageBoxW(nullptr, L"Failed to initialize agent", L"Error", MB_ICONERROR);
        return 1;
    }

    return app.run();
}
