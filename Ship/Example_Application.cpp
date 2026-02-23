// Example_Application.cpp - Standalone Demo Application
// Pure C++20 / Win32 - Zero Qt Dependencies
// Demonstrates all agent capabilities

#include "RawrXD_Agent.hpp"
#include <iostream>
#include <filesystem>
#include <map>
#include <chrono>
#include <ctime>

using namespace RawrXD;
namespace fs = std::filesystem;

// Demo: Direct tool execution
void demoToolExecution() {
    std::wcout << L"\n=== Tool Execution Demo ===\n";

    ToolExecutionEngine engine;
    Tools::registerAllTools(engine);

    // Read a file
    JsonObject params;
    params[L"path"] = String(L"D:\\RawrXD\\Ship\\README.md");
    params[L"startLine"] = static_cast<int64_t>(1);
    params[L"endLine"] = static_cast<int64_t>(10);

    auto result = engine.execute(String(L"read_file"), params);
    if (result.isSuccess()) {
        std::wcout << L"Read file successfully\n";
        std::wcout << L"Output: " << JsonParser::Serialize(result.output, 2).c_str() << L"\n";
    } else {
        std::wcout << L"Failed: " << result.errorMessage.c_str() << L"\n";
    }

    // List directory
    params.clear();
    params[L"path"] = String(L"D:\\RawrXD\\Ship");
    params[L"recursive"] = false;

    result = engine.execute(String(L"list_directory"), params);
    if (result.isSuccess()) {
        std::wcout << L"Listed directory successfully\n";
    }

    // Search files
    params.clear();
    params[L"path"] = String(L"D:\\RawrXD\\Ship");
    params[L"pattern"] = String(L"*.hpp");
    params[L"maxResults"] = static_cast<int64_t>(10);

    result = engine.execute(String(L"search_files"), params);
    if (result.isSuccess()) {
        std::wcout << L"Found matching files\n";
    }
}

// Demo: LLM client
void demoLLMClient() {
    std::wcout << L"\n=== LLM Client Demo ===\n";

    LLMClient client(L"localhost", 11434);

    if (!client.isAvailable()) {
        std::wcout << L"Ollama not available, skipping LLM demo\n";
        return;
    }

    std::wcout << L"Connected to Ollama\n";

    auto models = client.listModels();
    std::wcout << L"Available models:\n";
    for (const auto& m : models) {
        std::wcout << L"  - " << m.c_str() << L"\n";
    }

    // Simple prompt
    std::wcout << L"\nSending test prompt...\n";
    String response = client.prompt(String(L"What is 2+2? Answer briefly."));
    if (!response.empty()) {
        std::wcout << L"Response: " << response.c_str() << L"\n";
    }
}

// Demo: JSON parsing
void demoJsonParsing() {
    std::wcout << L"\n=== JSON Parsing Demo ===\n";

    std::string json = R"({
        "name": "RawrXD",
        "version": 1.0,
        "features": ["agent", "tools", "llm"],
        "config": {
            "model": "qwen2.5-coder:14b",
            "temperature": 0.7
        }
    })";

    auto result = JsonParser::Parse(json);
    if (result) {
        std::wcout << L"Parsed successfully\n";

        // Serialize back
        std::string serialized = JsonParser::Serialize(*result, 2);
        std::wcout << L"Serialized:\n" << serialized.c_str() << L"\n";
    }
}

// Demo: C++20 String and STL (no Qt)
void demoStdReplacements() {
    std::wcout << L"\n=== C++20 String Demo ===\n";

    String str(L"Hello, World!");
    std::wcout << L"String: " << str.c_str() << L"\n";
    std::wcout << L"Upper: " << StringUtils::ToUpper(str).c_str() << L"\n";
    std::wcout << L"Contains 'World': " << (str.find(L"World") != String::npos ? L"yes" : L"no") << L"\n";

    Vector<String> list = { L"one", L"two", L"three" };
    std::wcout << L"Joined: " << StringUtils::Join(list, L", ").c_str() << L"\n";

    std::map<String, int> map;
    map[L"a"] = 1;
    map[L"b"] = 2;
    std::wcout << L"Map contains 'a': " << (map.count(L"a") ? L"yes" : L"no") << L"\n";

    fs::path path(L"D:\\RawrXD\\Ship\\README.md");
    std::wcout << L"File exists: " << (fs::exists(path) ? L"yes" : L"no") << L"\n";

    fs::path dir(L"D:\\RawrXD\\Ship");
    std::wcout << L"Dir exists: " << (fs::exists(dir) ? L"yes" : L"no") << L"\n";

    std::time_t t = std::time(nullptr);
    std::tm tm_buf;
    localtime_s(&tm_buf, &t);
    wchar_t timeBuf[64];
    std::wcsftime(timeBuf, 64, L"%Y-%m-%d %H:%M:%S", &tm_buf);
    std::wcout << L"Current time: " << timeBuf << L"\n";
}

// Demo: Full agent
void demoFullAgent() {
    std::wcout << L"\n=== Full Agent Demo ===\n";

    Agent agent;

    AgentConfig config;
    config.workingDirectory = String(L"D:\\RawrXD\\Ship");

    if (!agent.initialize(config)) {
        std::wcout << L"Failed to initialize agent\n";
        return;
    }

    std::wcout << L"Agent initialized\n";

    // Set event callback
    agent.setEventCallback([](const AgentEvent& event) {
        switch (event.type) {
            case AgentEvent::Type::StateChanged:
                std::wcout << L"[State: " << event.message.c_str() << L"]\n";
                break;
            case AgentEvent::Type::ToolCalled:
                std::wcout << L"[Tool: " << event.message.c_str() << L"]\n";
                break;
            default:
                break;
        }
    });

    // Execute a tool directly
    JsonObject params;
    params[L"path"] = String(L"D:\\RawrXD\\Ship");
    auto result = agent.executeTool(String(L"list_directory"), params);

    if (result.isSuccess()) {
        std::wcout << L"Tool executed successfully\n";
    }

    agent.shutdown();
}

int wmain(int argc, wchar_t* argv[]) {
    // Set console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::wcout << L"RawrXD Agent Examples\n";
    std::wcout << L"Version: " << AGENT_VERSION << L"\n";
    std::wcout << L"========================================\n";

    // Initialize
    if (!initializeAgent()) {
        std::wcerr << L"Failed to initialize\n";
        return 1;
    }

    // Run demos
    demoJsonParsing();
    demoStdReplacements();
    demoToolExecution();
    demoLLMClient();
    demoFullAgent();

    std::wcout << L"\n========================================\n";
    std::wcout << L"All demos completed!\n";

    cleanupAgent();
    return 0;
}
