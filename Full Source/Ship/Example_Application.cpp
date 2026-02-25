// Example_Application.cpp - Standalone Demo Application
// Pure C++20 / Win32 - Zero Qt Dependencies
// Demonstrates all agent capabilities

#include "RawrXD_Agent.hpp"
#include <iostream>

using namespace RawrXD;

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

    auto result = engine.execute(QString("read_file"), params);
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

    result = engine.execute(QString("list_directory"), params);
    if (result.isSuccess()) {
        std::wcout << L"Listed directory successfully\n";
    }

    // Search files
    params.clear();
    params[L"path"] = String(L"D:\\RawrXD\\Ship");
    params[L"pattern"] = String(L"*.hpp");
    params[L"maxResults"] = static_cast<int64_t>(10);

    result = engine.execute(QString("search_files"), params);
    if (result.isSuccess()) {
        std::wcout << L"Found matching files\n";
    }
}

// Demo: LLM client
void demoLLMClient() {
    std::wcout << L"\n=== LLM Client Demo ===\n";

    LLMClient client("localhost", 11434);

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
    QString response = client.prompt(QString("What is 2+2? Answer briefly."));
    if (!response.isEmpty()) {
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

// Demo: Qt replacements
void demoQtReplacements() {
    std::wcout << L"\n=== Qt Replacements Demo ===\n";

    // QString
    QString str("Hello, World!");
    std::wcout << L"QString: " << str.c_str() << L"\n";
    std::wcout << L"Upper: " << str.toUpper().c_str() << L"\n";
    std::wcout << L"Contains 'World': " << (str.contains(QString("World")) ? L"yes" : L"no") << L"\n";

    // QStringList
    QStringList list;
    list.push_back(QString("one"));
    list.push_back(QString("two"));
    list.push_back(QString("three"));
    std::wcout << L"Joined: " << list.join(QString(", ")).c_str() << L"\n";

    // QMap
    QMap<QString, int> map;
    map.insert(QString("a"), 1);
    map.insert(QString("b"), 2);
    std::wcout << L"Map contains 'a': " << (map.contains(QString("a")) ? L"yes" : L"no") << L"\n";

    // QFile
    QString path("D:\\RawrXD\\Ship\\README.md");
    std::wcout << L"File exists: " << (QFile::exists(path) ? L"yes" : L"no") << L"\n";

    // QDir
    QDir dir("D:\\RawrXD\\Ship");
    std::wcout << L"Dir exists: " << (dir.exists() ? L"yes" : L"no") << L"\n";

    // QDateTime
    auto now = QDateTime::currentDateTime();
    std::wcout << L"Current time: " << now.toString().c_str() << L"\n";

    // QUuid
    auto uuid = QUuid::createUuid();
    std::wcout << L"UUID: " << uuid.toString().c_str() << L"\n";
}

// Demo: Full agent
void demoFullAgent() {
    std::wcout << L"\n=== Full Agent Demo ===\n";

    Agent agent;

    AgentConfig config;
    config.workingDirectory = QString("D:\\RawrXD\\Ship");

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
    auto result = agent.executeTool(QString("list_directory"), params);

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
    demoQtReplacements();
    demoToolExecution();
    demoLLMClient();
    demoFullAgent();

    std::wcout << L"\n========================================\n";
    std::wcout << L"All demos completed!\n";

    cleanupAgent();
    return 0;
}
