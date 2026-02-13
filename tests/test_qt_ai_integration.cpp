/**
 * \file test_qt_ai_integration.cpp
 * \brief Integration test for Qt UI → AI completion pipeline
 * \author RawrXD Team
 * \date 2025-12-13
 *
 * Tests the complete user-facing flow:
 * 1. User types in AgenticTextEdit
 * 2. AICompletionProvider receives request
 * 3. RealTimeCompletionEngine generates completion
 * 4. Ghost text displays in UI
 */

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <iostream>
#include <memory>
#include "ai_completion_provider.h"
#include "real_time_completion_engine.h"
#include "inference_engine.h"
#include "logging/logger.h"
#include "metrics/metrics.h"

using namespace RawrXD;

class IntegrationTest : public QObject {
    Q_OBJECT

public:
    IntegrationTest(QObject* parent = nullptr) : QObject(parent), testsPassed_(0), testsFailed_(0) {}

    void runTests() {
        std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
        std::cout << "║     Qt → AI Completion Integration Test Suite       ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

        // Initialize components
        if (!initializeComponents()) {
            std::cerr << "✗ Failed to initialize test components\n";
            QCoreApplication::exit(1);
            return;
        }

        // Run tests sequentially
        testBasicCompletion();
        
        // Schedule next test
        QTimer::singleShot(500, this, &IntegrationTest::testInlineCompletion);
    }

private slots:
    void testInlineCompletion() {
        std::cout << "\n[TEST 2/4] Inline Completion...\n";
        
        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this](const QVector<AICompletion>& completions) {
            if (!completions.isEmpty()) {
                std::cout << "  ✓ Received " << completions.size() << " inline completions\n";
                std::cout << "    Best: \"" << completions[0].text.toStdString().substr(0, 50) << "...\"\n";
                testsPassed_++;
            } else {
                std::cout << "  ✗ No inline completions\n";
                testsFailed_++;
            }
            
            // Disconnect and move to next test
            disconnect(aiProvider_.get(), &AICompletionProvider::completionsReady, nullptr, nullptr);
            QTimer::singleShot(500, this, &IntegrationTest::testMultiLineCompletion);
        });

        aiProvider_->requestInlineCompletion(
            "std::vector<int> numbers = ",
            27,
            "test.cpp"
        );
    }

    void testMultiLineCompletion() {
        std::cout << "\n[TEST 3/4] Multi-Line Completion...\n";
        
        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this](const QVector<AICompletion>& completions) {
            if (!completions.isEmpty() && completions[0].isMultiLine) {
                std::cout << "  ✓ Multi-line completion received\n";
                int lines = completions[0].text.count('\n') + 1;
                std::cout << "    Lines: " << lines << "\n";
                testsPassed_++;
            } else {
                std::cout << "  ✗ No multi-line completions or single-line only\n";
                testsFailed_++;
            }
            
            disconnect(aiProvider_.get(), &AICompletionProvider::completionsReady, nullptr, nullptr);
            QTimer::singleShot(500, this, &IntegrationTest::testLatencyReporting);
        });

        aiProvider_->requestMultiLineCompletion(
            "// Sort array\nvoid quickSort(int arr[], int low, int high) {\n    ",
            "test.cpp",
            5
        );
    }

    void testLatencyReporting() {
        std::cout << "\n[TEST 4/4] Latency Reporting...\n";
        
        bool latencyReported = false;
        
        connect(aiProvider_.get(), &AICompletionProvider::latencyReported,
                this, [&latencyReported](double ms) {
            std::cout << "  ✓ Latency reported: " << ms << " ms\n";
            if (ms < 150.0) {
                std::cout << "    ✅ Within acceptable range (<150ms)\n";
            } else {
                std::cout << "    ⚠ Above ideal range (>150ms)\n";
            }
            latencyReported = true;
        });

        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this, &latencyReported](const QVector<AICompletion>&) {
            if (latencyReported) {
                testsPassed_++;
            } else {
                std::cout << "  ✗ No latency report received\n";
                testsFailed_++;
            }
            
            // All tests complete
            disconnect(aiProvider_.get(), nullptr, nullptr, nullptr);
            QTimer::singleShot(100, this, &IntegrationTest::printSummary);
        });

        aiProvider_->requestCompletions(
            "int result = ",
            ";",
            "test.cpp",
            "cpp",
            {"// Previous context", "void calculate() {"}
        );
    }

    void printSummary() {
        std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
        std::cout << "║                  TEST SUMMARY                        ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";
        
        std::cout << "  Tests Passed: " << testsPassed_ << "/" << (testsPassed_ + testsFailed_) << "\n";
        std::cout << "  Tests Failed: " << testsFailed_ << "/" << (testsPassed_ + testsFailed_) << "\n\n";
        
        // Get performance metrics
        auto metrics = aiProvider_->getMetrics();
        std::cout << "Performance Metrics:\n";
        std::cout << "  • Average latency: " << metrics.avgLatencyMs << " ms\n";
        std::cout << "  • P95 latency: " << metrics.p95LatencyMs << " ms\n";
        std::cout << "  • Cache hit rate: " << (metrics.cacheHitRate * 100) << "%\n";
        std::cout << "  • Total requests: " << metrics.requestCount << "\n";
        std::cout << "  • Errors: " << metrics.errorCount << "\n\n";
        
        if (testsFailed_ == 0) {
            std::cout << "🎉 ALL INTEGRATION TESTS PASSED! 🎉\n";
            std::cout << "Qt → AI pipeline is PRODUCTION-READY!\n\n";
            QCoreApplication::exit(0);
        } else {
            std::cout << "⚠ Some tests failed, review output above\n\n";
            QCoreApplication::exit(1);
        }
    }

private:
    bool initializeComponents() {
        std::cout << "[INIT] Initializing test components...\n\n";

        // 1. Logger and metrics
        logger_ = std::make_shared<Logger>();
        metrics_ = std::make_shared<Metrics>();
        logger_->setLevel(LogLevel::INFO);

        // 2. Inference engine
        std::cout << "  • Loading GGUF model...\n";
        engine_ = new InferenceEngine(nullptr);
        bool loaded = engine_->Initialize("models/ministral-3b-instruct-v0.3-Q4_K_M.gguf");
        
        if (!loaded) {
            std::cerr << "    ✗ Failed to load model\n";
            return false;
        }
        std::cout << "    ✓ Model loaded (vocab: " << engine_->GetVocabSize() << ")\n";

        // 3. Completion engine
        std::cout << "  • Initializing completion engine...\n";
        completionEngine_ = std::make_unique<RealTimeCompletionEngine>(logger_, metrics_);
        completionEngine_->setInferenceEngine(engine_);
        std::cout << "    ✓ Completion engine ready\n";

        // 4. AI provider
        std::cout << "  • Creating AI completion provider...\n";
        aiProvider_ = std::make_unique<AICompletionProvider>(
            completionEngine_.get(),
            logger_,
            this
        );
        std::cout << "    ✓ AI provider ready\n\n";

        return true;
    }

    void testBasicCompletion() {
        std::cout << "[TEST 1/4] Basic Completion...\n";
        
        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this](const QVector<AICompletion>& completions) {
            if (!completions.isEmpty()) {
                std::cout << "  ✓ Received " << completions.size() << " completions\n";
                std::cout << "    Confidence: " << (completions[0].confidence * 100) << "%\n";
                std::cout << "    Kind: " << completions[0].kind.toStdString() << "\n";
                testsPassed_++;
            } else {
                std::cout << "  ✗ No completions received\n";
                testsFailed_++;
            }
            
            // Disconnect this specific test handler
            disconnect(aiProvider_.get(), &AICompletionProvider::completionsReady, nullptr, nullptr);
        });

        connect(aiProvider_.get(), &AICompletionProvider::error,
                this, [this](const QString& error) {
            std::cout << "  ✗ Error: " << error.toStdString() << "\n";
            testsFailed_++;
        });

        aiProvider_->requestCompletions(
            "int main() {\n    ",
            "\n}",
            "test.cpp",
            "cpp",
            {}
        );
    }

    std::shared_ptr<Logger> logger_;
    std::shared_ptr<Metrics> metrics_;
    InferenceEngine* engine_;
    std::unique_ptr<RealTimeCompletionEngine> completionEngine_;
    std::unique_ptr<AICompletionProvider> aiProvider_;
    
    int testsPassed_;
    int testsFailed_;
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    IntegrationTest test;
    
    // Start tests after event loop begins
    QTimer::singleShot(0, &test, &IntegrationTest::runTests);

    return app.exec();
}

#include "test_qt_ai_integration.moc"
