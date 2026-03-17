// model_interface_examples.cpp - Usage Examples for Universal Model Interface
// This file demonstrates how to use the unified model interface for local and cloud models

#include "model_interface.h"
#include <iostream>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

// ============ EXAMPLE 1: Basic Initialization ============
void example_basic_initialization()
{
    // Create interface and load configuration
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Verify initialization
    if (!ai.isInitialized()) {
        std::cerr << "Failed to initialize ModelInterface\n";
        return;
    }
    
    std::cout << "Initialized with models: "
              << ai.getAvailableModels().size() << " total\n";
}

// ============ EXAMPLE 2: Single Generation with Local Model ============
void example_local_model_generation()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Generate with local GGUF model (fast, no API key needed)
    GenerationResult result = ai.generate(
        "Write a simple C++ function to calculate factorial",
        "quantumide-q4km"
    );
    
    if (result.success) {
        std::cout << "Local model response:\n" << result.content.toStdString() << "\n";
        std::cout << "Latency: " << result.latency_ms << "ms\n";
    } else {
        std::cerr << "Error: " << result.error.toStdString() << "\n";
    }
}

// ============ EXAMPLE 3: Cloud Model Generation (OpenAI) ============
void example_cloud_model_generation()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Set API key from environment
    setenv("OPENAI_API_KEY", "sk-...", 1);
    
    // Reload to pick up environment variables
    ai.loadConfig("e:/model_config.json");
    
    // Generate with GPT-4
    GenerationResult result = ai.generate(
        "Explain quantum computing in simple terms",
        "gpt-4"
    );
    
    if (result.success) {
        std::cout << "GPT-4 response:\n" << result.content.toStdString() << "\n";
    }
}

// ============ EXAMPLE 4: Cloud Model Generation (Claude) ============
void example_claude_generation()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Set API key
    setenv("ANTHROPIC_API_KEY", "sk-ant-...", 1);
    ai.loadConfig("e:/model_config.json");
    
    // Generate with Claude 3 Opus
    GenerationResult result = ai.generate(
        "Write a comprehensive security audit checklist",
        "claude-3-opus"
    );
    
    if (result.success) {
        std::cout << "Claude response:\n" << result.content.toStdString() << "\n";
    }
}

// ============ EXAMPLE 5: Cloud Model Generation (Multiple) ============
void example_multiple_cloud_providers()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    std::string prompt = "Write a recursive binary search function";
    
    // Set all API keys
    setenv("OPENAI_API_KEY", "sk-...", 1);
    setenv("ANTHROPIC_API_KEY", "sk-ant-...", 1);
    setenv("GOOGLE_API_KEY", "AIza...", 1);
    setenv("MOONSHOT_API_KEY", "sk-...", 1);
    
    ai.loadConfig("e:/model_config.json");
    
    // Generate from multiple providers
    QString prompt_str = QString::fromStdString(prompt);
    
    auto gpt4_result = ai.generate(prompt_str, "gpt-4");
    auto claude_result = ai.generate(prompt_str, "claude-3-opus");
    auto gemini_result = ai.generate(prompt_str, "gemini-pro");
    auto kimi_result = ai.generate(prompt_str, "kimi");
    
    std::cout << "GPT-4: " << gpt4_result.content.toStdString() << "\n\n";
    std::cout << "Claude: " << claude_result.content.toStdString() << "\n\n";
    std::cout << "Gemini: " << gemini_result.content.toStdString() << "\n\n";
    std::cout << "Kimi: " << kimi_result.content.toStdString() << "\n\n";
}

// ============ EXAMPLE 6: Streaming Response ============
void example_streaming()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    setenv("OPENAI_API_KEY", "sk-...", 1);
    ai.loadConfig("e:/model_config.json");
    
    std::cout << "Streaming response from GPT-4:\n";
    
    ai.generateStream(
        "Explain machine learning",
        "gpt-4",
        [](const QString& chunk) {
            // Called for each chunk as it arrives
            std::cout << chunk.toStdString() << std::flush;
        },
        [](const QString& error) {
            std::cerr << "Streaming error: " << error.toStdString() << "\n";
        }
    );
    
    std::cout << "\n";
}

// ============ EXAMPLE 7: Batch Processing ============
void example_batch_generation()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    setenv("OPENAI_API_KEY", "sk-...", 1);
    ai.loadConfig("e:/model_config.json");
    
    QStringList prompts = {
        "Write a bubble sort algorithm",
        "Write a merge sort algorithm",
        "Write a quicksort algorithm"
    };
    
    auto results = ai.generateBatch(prompts, "gpt-4");
    
    for (int i = 0; i < results.size(); ++i) {
        std::cout << "Prompt " << (i+1) << ":\n";
        std::cout << results[i].content.toStdString() << "\n\n";
    }
}

// ============ EXAMPLE 8: Smart Model Selection ============
void example_smart_model_selection()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Select best model for coding task (prefers local if available)
    QString best_model = ai.selectBestModel("code_generation", "cpp", true);
    std::cout << "Best model for C++ code: " << best_model.toStdString() << "\n";
    
    // Select fastest model
    QString fastest = ai.selectFastestModel("general");
    std::cout << "Fastest model: " << fastest.toStdString() << "\n";
    
    // Select cost-optimal model (max $0.10)
    QString cheap = ai.selectCostOptimalModel("Explain recursion", 0.10);
    std::cout << "Cost-optimal model: " << cheap.toStdString() << "\n";
}

// ============ EXAMPLE 9: Model Statistics and Monitoring ============
void example_statistics()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Make some requests
    ai.generate("Test prompt 1", "gpt-4");
    ai.generate("Test prompt 2", "claude-3-opus");
    ai.generate("Test prompt 3", "gpt-4");
    
    // Get statistics
    auto stats = ai.getUsageStatistics();
    std::cout << "Usage statistics:\n" << stats.dump(2).c_str() << "\n";
    
    // Per-model stats
    auto gpt4_stats = ai.getModelStats("gpt-4");
    std::cout << "GPT-4 stats:\n" << gpt4_stats.dump(2).c_str() << "\n";
    
    // Performance metrics
    std::cout << "Average latency: " << ai.getAverageLatency() << "ms\n";
    std::cout << "Success rate: " << ai.getSuccessRate() << "%\n";
    
    // Cost tracking
    std::cout << "Total cost: $" << ai.getTotalCost() << "\n";
    
    auto cost_breakdown = ai.getCostBreakdown();
    std::cout << "Cost breakdown:\n" << cost_breakdown.dump(2).c_str() << "\n";
}

// ============ EXAMPLE 10: Custom Model Registration ============
void example_custom_model_registration()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Register a custom local model
    ModelConfig custom_local;
    custom_local.backend = ModelBackend::LOCAL_GGUF;
    custom_local.model_id = "C:/models/my-custom-model.gguf";
    custom_local.description = "My fine-tuned local model";
    
    ai.registerModel("my-custom-model", custom_local);
    
    // Register a custom cloud model
    ModelConfig custom_cloud;
    custom_cloud.backend = ModelBackend::ANTHROPIC;
    custom_cloud.model_id = "claude-3-haiku";
    custom_cloud.api_key = "${ANTHROPIC_API_KEY}";
    custom_cloud.description = "Claude 3 Haiku - Fast and cheap";
    
    ai.registerModel("claude-haiku", custom_cloud);
    
    // List all models
    std::cout << "Available models after registration:\n";
    std::cout << ai.formatModelList().toStdString();
}

// ============ EXAMPLE 11: Asynchronous Operations ============
void example_async_operations()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    setenv("OPENAI_API_KEY", "sk-...", 1);
    ai.loadConfig("e:/model_config.json");
    
    // Production-ready async single generation with QEventLoop
    QEventLoop loop;
    bool asyncCompleted = false;
    
    ai.generateAsync("What is AI?", "gpt-4", 
        [&loop, &asyncCompleted](const GenerationResult& result) {
            if (result.success) {
                std::cout << "Async result: " << result.content.toStdString() << "\n";
            } else {
                std::cerr << "Async error: " << result.error.toStdString() << "\n";
            }
            asyncCompleted = true;
            loop.quit();
        });
    
    // Wait for async operation to complete with timeout
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);  // 30 second timeout
    loop.exec();
    
    if (!asyncCompleted) {
        std::cerr << "Warning: Async operation timed out after 30 seconds\n";
    }
    
    // Production-ready async batch processing with QEventLoop
    QEventLoop batchLoop;
    bool batchCompleted = false;
    QStringList batch_prompts = {"Prompt 1", "Prompt 2", "Prompt 3"};
    
    ai.generateBatchAsync(batch_prompts, "gpt-4",
        [&batchLoop, &batchCompleted](const QVector<GenerationResult>& results) {
            std::cout << "Batch complete with " << results.size() << " results\n";
            for (int i = 0; i < results.size(); ++i) {
                if (results[i].success) {
                    std::cout << "  Result " << (i+1) << ": " 
                              << results[i].content.left(50).toStdString() << "...\n";
                } else {
                    std::cerr << "  Result " << (i+1) << " error: " 
                              << results[i].error.toStdString() << "\n";
                }
            }
            batchCompleted = true;
            batchLoop.quit();
        });
    
    // Wait for batch operation with timeout
    QTimer::singleShot(60000, &batchLoop, &QEventLoop::quit);  // 60 second timeout for batch
    batchLoop.exec();
    
    if (!batchCompleted) {
        std::cerr << "Warning: Batch operation timed out after 60 seconds\n";
    }
}

// ============ EXAMPLE 12: Configuration Management ============
void example_configuration()
{
    ModelInterface ai;
    
    // Load configuration from file
    ai.loadConfig("e:/model_config.json");
    
    // Set default model
    ai.setDefaultModel("gpt-4");
    
    // List all models
    auto models = ai.getAvailableModels();
    std::cout << "Total models available: " << models.size() << "\n";
    std::cout << "Local models: " << ai.getLocalModels().size() << "\n";
    std::cout << "Cloud models: " << ai.getCloudModels().size() << "\n";
    
    // Get model info
    auto gpt4_info = ai.getModelInfo("gpt-4");
    std::cout << "GPT-4 info:\n" << gpt4_info.dump(2).c_str() << "\n";
    
    // Save current configuration
    ai.saveConfig("e:/model_config_backup.json");
}

// ============ EXAMPLE 13: Error Handling ============
void example_error_handling()
{
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Set custom error callback
    ai.setErrorCallback([](const QString& error) {
        std::cerr << "Model error: " << error.toStdString() << "\n";
    });
    
    // Set retry policy
    ai.setRetryPolicy(3, 1000);  // 3 retries, 1 second delay
    
    // Attempt generation with error handling
    try {
        auto result = ai.generate("Test", "nonexistent-model");
        if (!result.success) {
            std::cerr << "Generation failed: " << result.error.toStdString() << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

// ============ EXAMPLE 14: Comprehensive Application Integration ============
void example_comprehensive_application()
{
    // Initialize the unified model interface
    ModelInterface ai;
    ai.initialize("e:/model_config.json");
    
    // Set API keys from environment
    setenv("OPENAI_API_KEY", "sk-...", 1);
    setenv("ANTHROPIC_API_KEY", "sk-ant-...", 1);
    setenv("GOOGLE_API_KEY", "AIza...", 1);
    ai.loadConfig("e:/model_config.json");
    
    // Set default and error handling
    ai.setDefaultModel("gpt-4");
    ai.setRetryPolicy(3, 1000);
    
    // Example workflow: Code analysis
    QString code_snippet = "function fibonacci(n) { if (n <= 1) return n; return fibonacci(n-1) + fibonacci(n-2); }";
    QString analysis_prompt = QString("Analyze this code: %1\n\nProvide: 1) Complexity, 2) Improvements").arg(code_snippet);
    
    // Use multiple models for comparison
    std::cout << "=== Code Analysis with Multiple Models ===\n\n";
    
    // Local model (fast)
    auto local_analysis = ai.generate(analysis_prompt, "quantumide-q4km");
    std::cout << "Local Model Analysis:\n" << local_analysis.content.toStdString() << "\n\n";
    
    // GPT-4 (comprehensive)
    auto gpt4_analysis = ai.generate(analysis_prompt, "gpt-4");
    std::cout << "GPT-4 Analysis:\n" << gpt4_analysis.content.toStdString() << "\n\n";
    
    // Claude (detailed)
    auto claude_analysis = ai.generate(analysis_prompt, "claude-3-opus");
    std::cout << "Claude Analysis:\n" << claude_analysis.content.toStdString() << "\n\n";
    
    // Print statistics
    std::cout << "=== Statistics ===\n";
    std::cout << "Total requests: " << ai.getUsageStatistics().size() << "\n";
    std::cout << "Average latency: " << ai.getAverageLatency() << "ms\n";
    std::cout << "Success rate: " << ai.getSuccessRate() << "%\n";
    std::cout << "Total cost: $" << ai.getTotalCost() << "\n";
}

// ============ MAIN ENTRY POINT ============
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    // Run examples
    std::cout << "=== Example 1: Basic Initialization ===\n";
    example_basic_initialization();
    
    std::cout << "\n=== Example 2: Local Model Generation ===\n";
    example_local_model_generation();
    
    std::cout << "\n=== Example 3: Cloud Model Generation ===\n";
    example_cloud_model_generation();
    
    std::cout << "\n=== Example 8: Smart Model Selection ===\n";
    example_smart_model_selection();
    
    std::cout << "\n=== Example 10: Custom Model Registration ===\n";
    example_custom_model_registration();
    
    std::cout << "\n=== Example 14: Comprehensive Application ===\n";
    example_comprehensive_application();
    
    return 0;
}
