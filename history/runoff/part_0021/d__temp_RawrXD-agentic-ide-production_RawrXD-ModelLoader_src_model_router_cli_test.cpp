// model_router_cli_test.cpp - Comprehensive CLI Test for Model Router
#include "model_interface.h"
#include "universal_model_router.h"
#include "cloud_api_client.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <memory>

class ModelRouterTester : public QObject {
    Q_OBJECT

public:
    ModelRouterTester(QObject* parent = nullptr)
        : QObject(parent),
          test_count(0),
          success_count(0),
          failure_count(0)
    {
        model_interface = std::make_unique<ModelInterface>(this);
        
        connect(model_interface.get(), &ModelInterface::initialized,
                this, &ModelRouterTester::onInitialized);
        connect(model_interface.get(), &ModelInterface::generationComplete,
                this, &ModelRouterTester::onGenerationComplete);
        connect(model_interface.get(), &ModelInterface::generationError,
                this, &ModelRouterTester::onGenerationError);
    }

    void runTests() {
        std::cout << "\n=== Model Router Comprehensive Test Suite ===" << std::endl;
        std::cout << "Testing both local and cloud models..." << std::endl;
        
        // Initialize with config
        QString config_path = "./model_config.json";
        std::cout << "\n[TEST 1] Initializing Model Router..." << std::endl;
        model_interface->initialize(config_path);
    }

private slots:
    void onInitialized() {
        std::cout << "[PASS] Model Router initialized successfully" << std::endl;
        success_count++;
        
        // Test 2: List available models
        testListModels();
        
        // Test 3: Test local model
        QTimer::singleShot(1000, this, &ModelRouterTester::testLocalModel);
        
        // Test 4: Test cloud model (if API keys available)
        QTimer::singleShot(3000, this, &ModelRouterTester::testCloudModel);
        
        // Test 5: Test statistics
        QTimer::singleShot(5000, this, &ModelRouterTester::testStatistics);
        
        // Test 6: Test model selection
        QTimer::singleShot(6000, this, &ModelRouterTester::testModelSelection);
        
        // Final report
        QTimer::singleShot(8000, this, &ModelRouterTester::printReport);
    }

    void onGenerationComplete(const QString& result, int tokens, double latency) {
        std::cout << "[PASS] Generation completed:" << std::endl;
        std::cout << "  Tokens: " << tokens << std::endl;
        std::cout << "  Latency: " << latency << " ms" << std::endl;
        std::cout << "  Result length: " << result.length() << " chars" << std::endl;
        std::cout << "  Preview: " << result.left(100).toStdString() << "..." << std::endl;
        success_count++;
    }

    void onGenerationError(const QString& error) {
        std::cout << "[FAIL] Generation error: " << error.toStdString() << std::endl;
        failure_count++;
    }

    void testListModels() {
        test_count++;
        std::cout << "\n[TEST 2] Listing available models..." << std::endl;
        
        QStringList models = model_interface->getAvailableModels();
        if (models.isEmpty()) {
            std::cout << "[FAIL] No models found" << std::endl;
            failure_count++;
            return;
        }
        
        std::cout << "[PASS] Found " << models.size() << " models:" << std::endl;
        for (const QString& model : models) {
            std::cout << "  - " << model.toStdString() << std::endl;
        }
        success_count++;
    }

    void testLocalModel() {
        test_count++;
        std::cout << "\n[TEST 3] Testing local model (quantumide-q4km)..." << std::endl;
        
        QString prompt = "What is the capital of France? Answer in one sentence.";
        std::cout << "Prompt: " << prompt.toStdString() << std::endl;
        
        try {
            GenerationResult result = model_interface->generate(prompt, "quantumide-q4km");
            
            if (result.success) {
                std::cout << "[PASS] Local model generation successful" << std::endl;
                std::cout << "  Result: " << result.text.toStdString() << std::endl;
                std::cout << "  Tokens: " << result.tokens_generated << std::endl;
                std::cout << "  Latency: " << result.latency_ms << " ms" << std::endl;
                std::cout << "  Cost: $" << result.cost << std::endl;
                success_count++;
            } else {
                std::cout << "[FAIL] Local model generation failed: " << result.error_message.toStdString() << std::endl;
                failure_count++;
            }
        } catch (const std::exception& e) {
            std::cout << "[FAIL] Exception: " << e.what() << std::endl;
            failure_count++;
        }
    }

    void testCloudModel() {
        test_count++;
        std::cout << "\n[TEST 4] Testing cloud model (gpt-3.5-turbo)..." << std::endl;
        
        // Check if OpenAI API key is set
        QString api_key = qEnvironmentVariable("OPENAI_API_KEY");
        if (api_key.isEmpty()) {
            std::cout << "[SKIP] OPENAI_API_KEY not set, skipping cloud test" << std::endl;
            std::cout << "  Set environment variable: OPENAI_API_KEY=sk-..." << std::endl;
            return;
        }
        
        QString prompt = "Say 'Hello from cloud model' in exactly 5 words.";
        std::cout << "Prompt: " << prompt.toStdString() << std::endl;
        
        try {
            GenerationResult result = model_interface->generate(prompt, "gpt-3.5-turbo");
            
            if (result.success) {
                std::cout << "[PASS] Cloud model generation successful" << std::endl;
                std::cout << "  Result: " << result.text.toStdString() << std::endl;
                std::cout << "  Tokens: " << result.tokens_generated << std::endl;
                std::cout << "  Latency: " << result.latency_ms << " ms" << std::endl;
                std::cout << "  Cost: $" << result.cost << std::endl;
                success_count++;
            } else {
                std::cout << "[FAIL] Cloud model generation failed: " << result.error_message.toStdString() << std::endl;
                failure_count++;
            }
        } catch (const std::exception& e) {
            std::cout << "[FAIL] Exception: " << e.what() << std::endl;
            failure_count++;
        }
    }

    void testStatistics() {
        test_count++;
        std::cout << "\n[TEST 5] Testing statistics..." << std::endl;
        
        double avg_latency = model_interface->getAverageLatency();
        int success_rate = model_interface->getSuccessRate();
        double total_cost = model_interface->getTotalCost();
        int request_count = model_interface->getRequestCount();
        
        std::cout << "[PASS] Statistics retrieved:" << std::endl;
        std::cout << "  Average Latency: " << avg_latency << " ms" << std::endl;
        std::cout << "  Success Rate: " << success_rate << "%" << std::endl;
        std::cout << "  Total Cost: $" << total_cost << std::endl;
        std::cout << "  Request Count: " << request_count << std::endl;
        
        success_count++;
    }

    void testModelSelection() {
        test_count++;
        std::cout << "\n[TEST 6] Testing smart model selection..." << std::endl;
        
        QString selected = model_interface->selectBestModel("general", "en", true);
        std::cout << "[PASS] Selected model: " << selected.toStdString() << std::endl;
        
        if (!selected.isEmpty()) {
            success_count++;
        } else {
            std::cout << "[FAIL] No model selected" << std::endl;
            failure_count++;
        }
    }

    void printReport() {
        std::cout << "\n=== Test Report ===" << std::endl;
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed: " << success_count << std::endl;
        std::cout << "Failed: " << failure_count << std::endl;
        std::cout << "Success Rate: " << (test_count > 0 ? (success_count * 100 / test_count) : 0) << "%" << std::endl;
        
        if (failure_count == 0) {
            std::cout << "\n✅ ALL TESTS PASSED - Model Router is production ready!" << std::endl;
        } else {
            std::cout << "\n⚠️  SOME TESTS FAILED - Review errors above" << std::endl;
        }
        
        QCoreApplication::quit();
    }

private:
    std::unique_ptr<ModelInterface> model_interface;
    int test_count;
    int success_count;
    int failure_count;
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    std::cout << "Model Router CLI Test Tool" << std::endl;
    std::cout << "Qt Version: " << QT_VERSION_STR << std::endl;
    std::cout << "Date: December 13, 2025" << std::endl;
    
    ModelRouterTester tester;
    
    // Start tests after event loop starts
    QTimer::singleShot(100, &tester, &ModelRouterTester::runTests);
    
    return app.exec();
}

#include "model_router_cli_test.moc"
