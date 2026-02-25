// benchmark_suite.cpp - Performance Benchmarks for RawrXD Agent
// Pure C++20 / Win32 - Zero Qt Dependencies

#include "RawrXD_Agent.hpp"
#include <iostream>
#include <chrono>
#include <numeric>
#include <algorithm>

using namespace RawrXD;

// Benchmark result
struct BenchmarkResult {
    std::wstring name;
    double minMs = 0;
    double maxMs = 0;
    double avgMs = 0;
    double medianMs = 0;
    int iterations = 0;
};

// Run a benchmark
template<typename Func>
BenchmarkResult runBenchmark(const std::wstring& name, Func func, int iterations = 1000) {
    std::vector<double> times;
    times.reserve(iterations);

    // Warmup
    for (int i = 0; i < 10; ++i) {
        func();
    }

    // Measure
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(ms);
    }

    // Calculate statistics
    BenchmarkResult result;
    result.name = name;
    result.iterations = iterations;

    result.minMs = *std::min_element(times.begin(), times.end());
    result.maxMs = *std::max_element(times.begin(), times.end());
    result.avgMs = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    std::sort(times.begin(), times.end());
    result.medianMs = times[times.size() / 2];

    return result;
}

void printResult(const BenchmarkResult& r) {
    std::wcout << L"  " << r.name << L":\n";
    std::wcout << L"    Iterations: " << r.iterations << L"\n";
    std::wcout << L"    Min:    " << std::fixed << std::setprecision(3) << r.minMs << L" ms\n";
    std::wcout << L"    Max:    " << r.maxMs << L" ms\n";
    std::wcout << L"    Avg:    " << r.avgMs << L" ms\n";
    std::wcout << L"    Median: " << r.medianMs << L" ms\n";
    std::wcout << L"\n";
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmarkJsonParsing() {
    std::wcout << L"\n=== JSON Parsing Benchmarks ===\n\n";

    // Small JSON
    std::string smallJson = R"({"name": "test", "value": 42})";
    auto r1 = runBenchmark(L"Small JSON parse", [&]() {
        JsonParser::Parse(smallJson);
    });
    printResult(r1);

    // Medium JSON
    std::string mediumJson = R"({
        "users": [
            {"id": 1, "name": "Alice", "email": "alice@example.com"},
            {"id": 2, "name": "Bob", "email": "bob@example.com"},
            {"id": 3, "name": "Charlie", "email": "charlie@example.com"}
        ],
        "metadata": {
            "total": 3,
            "page": 1,
            "perPage": 10
        }
    })";
    auto r2 = runBenchmark(L"Medium JSON parse", [&]() {
        JsonParser::Parse(mediumJson);
    });
    printResult(r2);

    // Serialize
    JsonObject obj;
    obj[L"name"] = String(L"test");
    obj[L"value"] = static_cast<int64_t>(42);
    obj[L"active"] = true;

    auto r3 = runBenchmark(L"JSON serialize", [&]() {
        JsonParser::Serialize(obj, 2);
    });
    printResult(r3);
}

void benchmarkStringOperations() {
    std::wcout << L"\n=== String Operation Benchmarks ===\n\n";

    String testStr = L"Hello, World! This is a test string for benchmarking purposes.";

    auto r1 = runBenchmark(L"UTF-8 conversion (round trip)", [&]() {
        auto utf8 = StringUtils::ToUtf8(testStr);
        StringUtils::FromUtf8(utf8);
    });
    printResult(r1);

    auto r2 = runBenchmark(L"String split", [&]() {
        StringUtils::Split(testStr, L" ");
    });
    printResult(r2);

    auto r3 = runBenchmark(L"String replace", [&]() {
        StringUtils::Replace(testStr, L"test", L"benchmark");
    });
    printResult(r3);

    QString qstr(testStr);
    auto r4 = runBenchmark(L"QString operations", [&]() {
        qstr.toLower();
        qstr.toUpper();
        qstr.contains(QString(L"test"));
    });
    printResult(r4);
}

void benchmarkToolExecution() {
    std::wcout << L"\n=== Tool Execution Benchmarks ===\n\n";

    ToolExecutionEngine engine;
    Tools::registerAllTools(engine);

    // file_exists (no actual I/O)
    JsonObject params;
    params[L"path"] = String(L"C:\\Windows\\System32\\kernel32.dll");

    auto r1 = runBenchmark(L"Tool: file_exists", [&]() {
        engine.execute(QString("file_exists"), params);
    }, 100);
    printResult(r1);

    // get_system_info
    auto r2 = runBenchmark(L"Tool: get_system_info", [&]() {
        engine.execute(QString("get_system_info"), JsonObject{});
    }, 100);
    printResult(r2);

    // Tool lookup
    auto r3 = runBenchmark(L"Tool lookup", [&]() {
        engine.hasTool(QString("read_file"));
        engine.hasTool(QString("write_file"));
        engine.hasTool(QString("run_command"));
    }, 10000);
    printResult(r3);

    // Get tools schema
    auto r4 = runBenchmark(L"Get tools schema", [&]() {
        engine.getToolsSchema();
    }, 100);
    printResult(r4);
}

void benchmarkQtReplacements() {
    std::wcout << L"\n=== Qt Replacement Benchmarks ===\n\n";

    // QStringList operations
    QStringList list;
    for (int i = 0; i < 100; ++i) {
        list.push_back(QString(L"item_") + QString(std::to_wstring(i)));
    }

    auto r1 = runBenchmark(L"QStringList::join", [&]() {
        list.join(QString(L","));
    }, 1000);
    printResult(r1);

    auto r2 = runBenchmark(L"QStringList::contains", [&]() {
        list.contains(QString(L"item_50"));
    }, 10000);
    printResult(r2);

    // QMap operations
    QMap<QString, int> map;
    for (int i = 0; i < 100; ++i) {
        map.insert(QString(L"key_") + QString(std::to_wstring(i)), i);
    }

    auto r3 = runBenchmark(L"QMap::value", [&]() {
        map.value(QString(L"key_50"));
    }, 10000);
    printResult(r3);

    // QDateTime
    auto r4 = runBenchmark(L"QDateTime::currentDateTime", [&]() {
        QDateTime::currentDateTime();
    }, 10000);
    printResult(r4);

    // QUuid
    auto r5 = runBenchmark(L"QUuid::createUuid", [&]() {
        QUuid::createUuid();
    }, 1000);
    printResult(r5);
}

void benchmarkConversationManager() {
    std::wcout << L"\n=== Conversation Manager Benchmarks ===\n\n";

    ConversationManager conv;
    conv.setSystemPrompt(QString("You are a helpful assistant."));

    auto r1 = runBenchmark(L"Add message", [&]() {
        conv.addUserMessage(QString("Hello"));
    }, 1000);
    printResult(r1);

    // Pre-fill conversation
    for (int i = 0; i < 50; ++i) {
        conv.addUserMessage(QString("Message ") + QString(std::to_wstring(i)));
        conv.addAssistantMessage(QString("Response ") + QString(std::to_wstring(i)));
    }

    auto r2 = runBenchmark(L"Get messages (100 messages)", [&]() {
        conv.getMessages();
    }, 1000);
    printResult(r2);
}

void benchmarkAgentContext() {
    std::wcout << L"\n=== Agent Context Benchmarks ===\n\n";

    AgentContext ctx;
    ctx.setWorkingDirectory(QString("D:\\RawrXD\\Ship"));

    auto r1 = runBenchmark(L"Add open file", [&]() {
        ctx.addOpenFile(QString("test.cpp"));
    }, 10000);
    printResult(r1);

    // Pre-fill files
    for (int i = 0; i < 100; ++i) {
        ctx.addOpenFile(QString(L"file_") + QString(std::to_wstring(i)) + QString(L".cpp"));
    }

    auto r2 = runBenchmark(L"Context to JSON (100 files)", [&]() {
        ctx.toJson();
    }, 1000);
    printResult(r2);
}

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::wcout << L"========================================\n";
    std::wcout << L"RawrXD Agent Benchmark Suite\n";
    std::wcout << L"========================================\n";

    if (!initializeAgent()) {
        std::wcerr << L"Failed to initialize\n";
        return 1;
    }

    auto totalStart = std::chrono::high_resolution_clock::now();

    benchmarkJsonParsing();
    benchmarkStringOperations();
    benchmarkToolExecution();
    benchmarkQtReplacements();
    benchmarkConversationManager();
    benchmarkAgentContext();

    auto totalEnd = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();

    std::wcout << L"========================================\n";
    std::wcout << L"Total benchmark time: " << std::fixed << std::setprecision(2) << totalMs << L" ms\n";
    std::wcout << L"========================================\n";

    cleanupAgent();
    return 0;
}
