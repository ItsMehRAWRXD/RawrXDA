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

    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<ResponseParser> m_parser;
    std::shared_ptr<ModelTester> m_tester;
    std::shared_ptr<LibraryIntegration> m_libIntegration;

public:
    DemoApplication() {
        // Initialize core components
        m_logger = std::make_shared<Logger>();
        m_metrics = std::make_shared<Metrics>();


        // Initialize Phase 1 components
        m_parser = std::make_shared<ResponseParser>(m_logger, m_metrics);
        m_tester = std::make_shared<ModelTester>(m_logger, m_metrics, m_parser);

        // Initialize Phase 2 components
        m_libIntegration = std::make_shared<LibraryIntegration>(m_logger, m_metrics);
        m_libIntegration->initializeAll();

    }

    /**
     * Demo 1: Response Parsing (Phase 1.3)
     * Shows how model responses are split by boundaries
     */
    void demo1_ResponseParsing() {


        std::vector<std::string> testResponses = {
            // Example 1: C++ function
            "void greet() {\n  \n}\n\nint main() {",

            // Example 2: Python code
            "def add(a, b):\n  return a + b\n\nresult = add(1, 2)",

            // Example 3: JavaScript
            "function sum(nums) {\n  return nums.reduce((a, b) => a + b);\n};",

            // Example 4: Mixed statements
            "x = 42; y = 100; z = x + y; if (z > 100) { print('yes'); }"
        };

        for (size_t i = 0; i < testResponses.size(); i++) {


                          testResponses[i].substr(0, 50) + "...");

            auto completions = m_parser->parseResponse(testResponses[i]);

            for (size_t j = 0; j < completions.size() && j < 3; j++) {

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


        std::string response = "def fibonacci(n):\n  if n <= 1:\n    return n\n  return fib(n-1)";
        
        // Simulate streaming chunks
        std::vector<std::string> chunks = {
            "def fib",
            "onacci(n):\n",
            "  if n <= 1:\n",
            "    return n\n",
            "  return fib(n-1)"
        };


        m_parser->reset();
        std::vector<ParsedCompletion> allCompletions;

        for (size_t i = 0; i < chunks.size(); i++) {

            auto completions = m_parser->parseChunk(chunks[i]);


            allCompletions.insert(allCompletions.end(), completions.begin(), completions.end());
        }

        // Flush remaining data
        auto final = m_parser->flush();
        allCompletions.insert(allCompletions.end(), final.begin(), final.end());

    }

    /**
     * Demo 3: Model Testing (Phase 1.4)
     * Demonstrates latency measurement and testing
     */
    void demo3_ModelTesting() {


        std::vector<std::string> testPrompts = {
            "print('hello')",
            "function add(a, b) {",
            "const x = 42;",
            "for i in range(10):",
            "SELECT * FROM users WHERE"
        };


        std::vector<ModelTestResult> results;
        for (const auto& prompt : testPrompts) {

            auto result = m_tester->testWithOllama("llama2", prompt, 50);

                           result.totalLatencyUs, result.timeToFirstTokenUs);

                           result.tokenCount, result.responseQuality * 100);

                           result.completionCount, result.parseSuccessful);

            results.push_back(result);
        }

        // Print summary

        double avgLatency = 0;
        for (const auto& result : results) {
            avgLatency += result.totalLatencyUs;
        }
        avgLatency /= results.size() / 1000.0; // Convert to ms

    }

    /**
     * Demo 4: Latency Benchmarking
     * Shows distribution of latencies across multiple runs
     */
    void demo4_LatencyBenchmarking() {


        std::vector<std::string> models = {"llama2", "mistral"};
        std::vector<std::string> testPrompts = {
            "function test()",
            "class Example:",
            "const value ="
        };

                       models.size(), testPrompts.size());

        auto benchmarks = m_tester->benchmarkModels(models, testPrompts, 3);

        for (const auto& bench : benchmarks) {


                           bench.p50LatencyMs, bench.p95LatencyMs, bench.p99LatencyMs);

        }
    }

    /**
     * Demo 5: HTTP Client (Phase 2)
     * Demonstrates HTTP integration with Ollama API
     */
    void demo5_HTTPClient() {


        auto httpClient = m_libIntegration->getHTTPClient();

        // Demo GET request

        auto getResponse = httpClient->get("http://localhost:11434/api/tags");


        // Demo POST request with JSON

        std::string jsonPayload = R"({
            "model": "llama2",
            "prompt": "Hello",
            "stream": false
        })";

        auto postResponse = httpClient->postJSON(
            "http://localhost:11434/api/generate",
            jsonPayload
        );


        // Demo streaming

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

    }

    /**
     * Demo 6: JSON Handling (Phase 2)
     * Demonstrates JSON parsing and generation
     */
    void demo6_JSONHandling() {


        auto jsonHandler = m_libIntegration->getJSONHandler();

        // Demo 1: Parse JSON
        std::string jsonData = R"({"model": "llama2", "tokens": 42, "quality": 0.95})";

        bool valid = jsonHandler->parseJSON(jsonData);


        // Demo 2: Extract values


        // Demo 3: Generate JSON

        std::vector<std::pair<std::string, std::string>> data = {
            {"name", "test_model"},
            {"version", "1.0"},
            {"status", "ready"}
        };
        std::string generated = jsonHandler->generateJSON(data);


        // Demo 4: Pretty print

        std::string compact = R"({"a":1,"b":2,"c":{"d":3}})";
        std::string pretty = jsonHandler->prettyPrint(compact);


        // Demo 5: Minify

        std::string minified = jsonHandler->minify(pretty);

    }

    /**
     * Demo 7: Compression (Phase 2)
     * Demonstrates Zstd compression
     */
    void demo7_Compression() {


        auto compressionHandler = m_libIntegration->getCompressionHandler();

        // Create test data
        std::string testData = std::string(1000, 'A');
        for (int i = 0; i < 100; i++) {
            testData += "Hello World! This is a test of compression. ";
        }

        std::vector<uint8_t> data(testData.begin(), testData.end());


        // Compress
        auto compressed = compressionHandler->compress(data, 3);

                       compressed.size(),
                       (compressed.size() * 100.0) / data.size());

        // Decompress
        auto decompressed = compressionHandler->decompress(compressed);


                       (decompressed.size() == data.size() && decompressed == data) ? "YES" : "NO");

        // Get statistics
        auto stats = compressionHandler->getStatistics();

        for (const auto& [name, value] : stats) {

        }
    }

    /**
     * Demo 8: Library Status
     * Shows all integrated libraries and their versions
     */
    void demo8_LibraryStatus() {


        std::string status = m_libIntegration->getStatus();


        // Check individual libraries


                       m_libIntegration->LibraryIntegration::isLibraryAvailable("curl") ? "YES" : "NO");

                       m_libIntegration->LibraryIntegration::isLibraryAvailable("zstd") ? "YES" : "NO");

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

            std::string report = m_tester->generateTestReport();


        } catch (const std::exception& e) {

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
        
        return 1;
    }
}
