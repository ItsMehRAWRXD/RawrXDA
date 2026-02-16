#include <iostream>
#include <memory>
#include <chrono>
#include <vector>
#include <iomanip>

#include "response_parser.h"
#include "model_tester.h"
#include "library_integration.h"
#include "logging/logger.h"
#include "metrics/metrics.h"

/**
 * Phase 1 & 2 Integration Demo
 * 
 * Demonstrates:
 * - Phase 1.3: Response parsing with boundary splitting
 * - Phase 1.4: Real model testing with latency measurement
 * - Phase 2: Library integration (curl, zstd, JSON)
 */

class DemoApplication {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<ResponseParser> m_parser;
    std::shared_ptr<ModelTester> m_tester;
    std::shared_ptr<LibraryIntegration> m_libIntegration;

public:
    DemoApplication() {
        // Initialize core components
        m_logger = std::make_shared<Logger>();
        m_metrics = std::make_shared<Metrics>();

        m_logger->info("=== Phase 1 & 2 Integration Demo ===");
        m_logger->info("Initializing components...");

        // Initialize Phase 1 components
        m_parser = std::make_shared<ResponseParser>(m_logger, m_metrics);
        m_tester = std::make_shared<ModelTester>(m_logger, m_metrics, m_parser);

        // Initialize Phase 2 components
        m_libIntegration = std::make_shared<LibraryIntegration>(m_logger, m_metrics);
        m_libIntegration->initializeAll();

        m_logger->info("All components initialized successfully");
    }

    /**
     * Demo 1: Response Parsing (Phase 1.3)
     * Shows how model responses are split by boundaries
     */
    void demo1_ResponseParsing() {
        m_logger->info("\n=== Demo 1: Response Parsing (Phase 1.3) ===");

        std::vector<std::string> testResponses = {
            // Example 1: C++ function
            "void greet() {\n  s_logger.info( \"Hello\";\n}\n\nint main() {",

            // Example 2: Python code
            "def add(a, b):\n  return a + b\n\nresult = add(1, 2)",

            // Example 3: JavaScript
            "function sum(nums) {\n  return nums.reduce((a, b) => a + b);\n};",

            // Example 4: Mixed statements
            "x = 42; y = 100; z = x + y; if (z > 100) { print('yes'); }"
        };

        for (size_t i = 0; i < testResponses.size(); i++) {
            m_logger->info("\n--- Test Response {} ---", i + 1);
            m_logger->info("Input ({}  chars): {}", testResponses[i].length(), 
                          testResponses[i].substr(0, 50) + "...");

            auto completions = m_parser->parseResponse(testResponses[i]);

            m_logger->info("Parsed {} completions:", completions.size());
            for (size_t j = 0; j < completions.size() && j < 3; j++) {
                m_logger->info("  [{}] {} chars, boundary='{}', confidence={:.2f}, complete={}",
                               j + 1, completions[j].text.length(), completions[j].boundary,
                               completions[j].confidence, completions[j].isComplete);
            }
        }
    }

    /**
     * Demo 2: Streaming Response Parsing
     * Shows incremental parsing of model responses
     */
    void demo2_StreamingParsing() {
        m_logger->info("\n=== Demo 2: Streaming Response Parsing ===");

        std::string response = "def fibonacci(n):\n  if n <= 1:\n    return n\n  return fib(n-1)";
        
        // Simulate streaming chunks
        std::vector<std::string> chunks = {
            "def fib",
            "onacci(n):\n",
            "  if n <= 1:\n",
            "    return n\n",
            "  return fib(n-1)"
        };

        m_logger->info("Simulating streaming response in {} chunks", chunks.size());

        m_parser->reset();
        std::vector<ParsedCompletion> allCompletions;

        for (size_t i = 0; i < chunks.size(); i++) {
            m_logger->info("\nChunk {}: '{}' ({} chars)", i + 1, chunks[i], chunks[i].length());
            auto completions = m_parser->parseChunk(chunks[i]);
            m_logger->info("  Extracted: {} completions", completions.size());

            allCompletions.insert(allCompletions.end(), completions.begin(), completions.end());
        }

        // Flush remaining data
        auto final = m_parser->flush();
        allCompletions.insert(allCompletions.end(), final.begin(), final.end());

        m_logger->info("\nTotal completions from stream: {}", allCompletions.size());
    }

    /**
     * Demo 3: Model Testing (Phase 1.4)
     * Demonstrates latency measurement and testing
     */
    void demo3_ModelTesting() {
        m_logger->info("\n=== Demo 3: Model Testing (Phase 1.4) ===");

        std::vector<std::string> testPrompts = {
            "print('hello')",
            "function add(a, b) {",
            "const x = 42;",
            "for i in range(10):",
            "SELECT * FROM users WHERE"
        };

        m_logger->info("Testing with {} prompts", testPrompts.size());

        std::vector<ModelTestResult> results;
        for (const auto& prompt : testPrompts) {
            m_logger->info("\n--- Testing prompt: '{}' ---", prompt);
            auto result = m_tester->testWithOllama("llama2", prompt, 50);

            m_logger->info("  Latency: {} us (time-to-first: {} us)",
                           result.totalLatencyUs, result.timeToFirstTokenUs);
            m_logger->info("  Tokens: {}, Quality: {:.2f}%",
                           result.tokenCount, result.responseQuality * 100);
            m_logger->info("  Parsed: {} completions, Success: {}",
                           result.completionCount, result.parseSuccessful);

            results.push_back(result);
        }

        // Print summary
        m_logger->info("\n--- Test Summary ---");
        double avgLatency = 0;
        for (const auto& result : results) {
            avgLatency += result.totalLatencyUs;
        }
        avgLatency /= results.size() / 1000.0; // Convert to ms

        m_logger->info("Average latency: {:.2f} ms", avgLatency);
    }

    /**
     * Demo 4: Latency Benchmarking
     * Shows distribution of latencies across multiple runs
     */
    void demo4_LatencyBenchmarking() {
        m_logger->info("\n=== Demo 4: Latency Benchmarking ===");

        std::vector<std::string> models = {"llama2", "mistral"};
        std::vector<std::string> testPrompts = {
            "function test()",
            "class Example:",
            "const value ="
        };

        m_logger->info("Benchmarking {} models with {} prompts (3 runs each)",
                       models.size(), testPrompts.size());

        auto benchmarks = m_tester->benchmarkModels(models, testPrompts, 3);

        m_logger->info("\n=== Benchmark Results ===");
        for (const auto& bench : benchmarks) {
            m_logger->info("\nModel: {}", bench.modelName);
            m_logger->info("  Total requests: {}", bench.totalRequests);
            m_logger->info("  Avg latency: {:.2f} ms", bench.avgLatencyMs);
            m_logger->info("  Min latency: {:.2f} ms", bench.minLatencyMs);
            m_logger->info("  Max latency: {:.2f} ms", bench.maxLatencyMs);
            m_logger->info("  P50: {:.2f} ms, P95: {:.2f} ms, P99: {:.2f} ms",
                           bench.p50LatencyMs, bench.p95LatencyMs, bench.p99LatencyMs);
            m_logger->info("  Throughput: {:.2f} tokens/sec", bench.throughputTokensPerSecond);
        }
    }

    /**
     * Demo 5: HTTP Client (Phase 2)
     * Demonstrates HTTP integration with Ollama API
     */
    void demo5_HTTPClient() {
        m_logger->info("\n=== Demo 5: HTTP Client (Phase 2) ===");

        auto httpClient = m_libIntegration->getHTTPClient();

        // Demo GET request
        m_logger->info("\nDemo: GET request to http://localhost:11434/api/tags");
        auto getResponse = httpClient->get("http://localhost:11434/api/tags");
        m_logger->info("  Status: {}", getResponse.statusCode);
        m_logger->info("  Response: {} bytes", getResponse.body.length());

        // Demo POST request with JSON
        m_logger->info("\nDemo: POST request with JSON payload");
        std::string jsonPayload = R"({
            "model": "llama2",
            "prompt": "Hello",
            "stream": false
        })";

        auto postResponse = httpClient->postJSON(
            "http://localhost:11434/api/generate",
            jsonPayload
        );
        m_logger->info("  Status: {}", postResponse.statusCode);
        m_logger->info("  Response: {} bytes", postResponse.body.length());

        // Demo streaming
        m_logger->info("\nDemo: Streaming request");
        std::vector<std::string> chunks;
        httpClient->streamRequest(
            HTTPRequest{
                "GET",
                "http://localhost:11434/api/generate?stream=true",
                "", {}, 30
            },
            [&chunks](const std::string& chunk) {
                chunks.push_back(chunk);
            }
        );
        m_logger->info("  Received {} chunks", chunks.size());
    }

    /**
     * Demo 6: JSON Handling (Phase 2)
     * Demonstrates JSON parsing and generation
     */
    void demo6_JSONHandling() {
        m_logger->info("\n=== Demo 6: JSON Handling (Phase 2) ===");

        auto jsonHandler = m_libIntegration->getJSONHandler();

        // Demo 1: Parse JSON
        std::string jsonData = R"({"model": "llama2", "tokens": 42, "quality": 0.95})";
        m_logger->info("\nParsing JSON: {}", jsonData);
        bool valid = jsonHandler->parseJSON(jsonData);
        m_logger->info("  Valid: {}", valid);

        // Demo 2: Extract values
        m_logger->info("\nExtracting values:");
        m_logger->info("  model: '{}'", jsonHandler->extractValue(jsonData, "model"));
        m_logger->info("  tokens: '{}'", jsonHandler->extractValue(jsonData, "tokens"));

        // Demo 3: Generate JSON
        m_logger->info("\nGenerating JSON from data:");
        std::vector<std::pair<std::string, std::string>> data = {
            {"name", "test_model"},
            {"version", "1.0"},
            {"status", "ready"}
        };
        std::string generated = jsonHandler->generateJSON(data);
        m_logger->info("  Generated:\n{}", generated);

        // Demo 4: Pretty print
        m_logger->info("\nPretty-printing JSON:");
        std::string compact = R"({"a":1,"b":2,"c":{"d":3}})";
        std::string pretty = jsonHandler->prettyPrint(compact);
        m_logger->info("  {}", pretty);

        // Demo 5: Minify
        m_logger->info("\nMinifying JSON:");
        std::string minified = jsonHandler->minify(pretty);
        m_logger->info("  {}", minified);
    }

    /**
     * Demo 7: Compression (Phase 2)
     * Demonstrates Zstd compression
     */
    void demo7_Compression() {
        m_logger->info("\n=== Demo 7: Compression (Phase 2) ===");

        auto compressionHandler = m_libIntegration->getCompressionHandler();

        // Create test data
        std::string testData = std::string(1000, 'A');
        for (int i = 0; i < 100; i++) {
            testData += "Hello World! This is a test of compression. ";
        }

        std::vector<uint8_t> data(testData.begin(), testData.end());

        m_logger->info("\nCompressing {} bytes of data...", data.size());

        // Compress
        auto compressed = compressionHandler->compress(data, 3);
        m_logger->info("  Compressed to {} bytes ({:.1f}%)",
                       compressed.size(),
                       (compressed.size() * 100.0) / data.size());

        // Decompress
        auto decompressed = compressionHandler->decompress(compressed);
        m_logger->info("  Decompressed to {} bytes", decompressed.size());
        m_logger->info("  Match: {}", 
                       (decompressed.size() == data.size() && decompressed == data) ? "YES" : "NO");

        // Get statistics
        auto stats = compressionHandler->getStatistics();
        m_logger->info("\nCompression statistics:");
        for (const auto& [name, value] : stats) {
            m_logger->info("  {}: {}", name, static_cast<int64_t>(value));
        }
    }

    /**
     * Demo 8: Library Status
     * Shows all integrated libraries and their versions
     */
    void demo8_LibraryStatus() {
        m_logger->info("\n=== Demo 8: Library Integration Status ===");

        std::string status = m_libIntegration->getStatus();
        m_logger->info("\n{}", status);

        // Check individual libraries
        m_logger->info("Library Availability:");
        m_logger->info("  curl: {}", 
                       m_libIntegration->LibraryIntegration::isLibraryAvailable("curl") ? "YES" : "NO");
        m_logger->info("  zstd: {}", 
                       m_libIntegration->LibraryIntegration::isLibraryAvailable("zstd") ? "YES" : "NO");
        m_logger->info("  json: {}", 
                       m_libIntegration->LibraryIntegration::isLibraryAvailable("json") ? "YES" : "NO");
    }

    /**
     * Run all demos
     */
    void runAllDemos() {
        try {
            demo1_ResponseParsing();
            demo2_StreamingParsing();
            demo3_ModelTesting();
            demo4_LatencyBenchmarking();
            demo5_HTTPClient();
            demo6_JSONHandling();
            demo7_Compression();
            demo8_LibraryStatus();

            // Generate final report
            m_logger->info("\n=== Final Test Report ===");
            std::string report = m_tester->generateTestReport();
            m_logger->info("\n{}", report);

        } catch (const std::exception& e) {
            m_logger->error("Demo execution failed: {}", e.what());
        }
    }
};

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    try {
        DemoApplication app;

        if (argc > 1 && std::string(argv[1]) == "--full") {
            app.runAllDemos();
        } else {
            // Run individual demos based on arguments
            if (argc == 1 || std::string(argv[1]).find("parsing") != std::string::npos) {
                app.demo1_ResponseParsing();
                app.demo2_StreamingParsing();
            }

            if (argc == 1 || std::string(argv[1]).find("model") != std::string::npos) {
                app.demo3_ModelTesting();
                app.demo4_LatencyBenchmarking();
            }

            if (argc == 1 || std::string(argv[1]).find("library") != std::string::npos) {
                app.demo5_HTTPClient();
                app.demo6_JSONHandling();
                app.demo7_Compression();
                app.demo8_LibraryStatus();
            }
        }

        return 0;

    } catch (const std::exception& e) {
        s_logger.error( "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
