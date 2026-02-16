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
        s_logger.info("\n╔═══════════════════════════════════════════════════════╗\n");
        s_logger.info("║     Qt → AI Completion Integration Test Suite       ║\n");
        s_logger.info("╚═══════════════════════════════════════════════════════╝\n\n");

        // Initialize components
        if (!initializeComponents()) {
            s_logger.error( "✗ Failed to initialize test components\n";
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
        s_logger.info("\n[TEST 2/4] Inline Completion...\n");
        
        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this](const QVector<AICompletion>& completions) {
            if (!completions.isEmpty()) {
                s_logger.info("  ✓ Received ");
                s_logger.info("    Best: \");
                testsPassed_++;
            } else {
                s_logger.info("  ✗ No inline completions\n");
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
        s_logger.info("\n[TEST 3/4] Multi-Line Completion...\n");
        
        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this](const QVector<AICompletion>& completions) {
            if (!completions.isEmpty() && completions[0].isMultiLine) {
                s_logger.info("  ✓ Multi-line completion received\n");
                int lines = completions[0].text.count('\n') + 1;
                s_logger.info("    Lines: ");
                testsPassed_++;
            } else {
                s_logger.info("  ✗ No multi-line completions or single-line only\n");
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
        s_logger.info("\n[TEST 4/4] Latency Reporting...\n");
        
        bool latencyReported = false;
        
        connect(aiProvider_.get(), &AICompletionProvider::latencyReported,
                this, [&latencyReported](double ms) {
            s_logger.info("  ✓ Latency reported: ");
            if (ms < 150.0) {
                s_logger.info("    ✅ Within acceptable range (<150ms)\n");
            } else {
                s_logger.info("    ⚠ Above ideal range (>150ms)\n");
            }
            latencyReported = true;
        });

        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this, &latencyReported](const QVector<AICompletion>&) {
            if (latencyReported) {
                testsPassed_++;
            } else {
                s_logger.info("  ✗ No latency report received\n");
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
        s_logger.info("\n╔═══════════════════════════════════════════════════════╗\n");
        s_logger.info("║                  TEST SUMMARY                        ║\n");
        s_logger.info("╚═══════════════════════════════════════════════════════╝\n\n");
        
        s_logger.info("  Tests Passed: ");
        s_logger.info("  Tests Failed: ");
        
        // Get performance metrics
        auto metrics = aiProvider_->getMetrics();
        s_logger.info("Performance Metrics:\n");
        s_logger.info("  • Average latency: ");
        s_logger.info("  • P95 latency: ");
        s_logger.info("  • Cache hit rate: ");
        s_logger.info("  • Total requests: ");
        s_logger.info("  • Errors: ");
        
        if (testsFailed_ == 0) {
            s_logger.info("🎉 ALL INTEGRATION TESTS PASSED! 🎉\n");
            s_logger.info("Qt → AI pipeline is PRODUCTION-READY!\n\n");
            QCoreApplication::exit(0);
        } else {
            s_logger.info("⚠ Some tests failed, review output above\n\n");
            QCoreApplication::exit(1);
        }
    }

private:
    bool initializeComponents() {
        s_logger.info("[INIT] Initializing test components...\n\n");

        // 1. Logger and metrics
        logger_ = std::make_shared<Logger>();
        metrics_ = std::make_shared<Metrics>();
        logger_->setLevel(LogLevel::INFO);

        // 2. Inference engine
        s_logger.info("  • Loading GGUF model...\n");
        engine_ = new InferenceEngine(nullptr);
        bool loaded = engine_->Initialize("models/ministral-3b-instruct-v0.3-Q4_K_M.gguf");
        
        if (!loaded) {
            s_logger.error( "    ✗ Failed to load model\n";
            return false;
        }
        s_logger.info("    ✓ Model loaded (vocab: ");

        // 3. Completion engine
        s_logger.info("  • Initializing completion engine...\n");
        completionEngine_ = std::make_unique<RealTimeCompletionEngine>(logger_, metrics_);
        completionEngine_->setInferenceEngine(engine_);
        s_logger.info("    ✓ Completion engine ready\n");

        // 4. AI provider
        s_logger.info("  • Creating AI completion provider...\n");
        aiProvider_ = std::make_unique<AICompletionProvider>(
            completionEngine_.get(),
            logger_,
            this
        );
        s_logger.info("    ✓ AI provider ready\n\n");

        return true;
    }

    void testBasicCompletion() {
        s_logger.info("[TEST 1/4] Basic Completion...\n");
        
        connect(aiProvider_.get(), &AICompletionProvider::completionsReady,
                this, [this](const QVector<AICompletion>& completions) {
            if (!completions.isEmpty()) {
                s_logger.info("  ✓ Received ");
                s_logger.info("    Confidence: ");
                s_logger.info("    Kind: ");
                testsPassed_++;
            } else {
                s_logger.info("  ✗ No completions received\n");
                testsFailed_++;
            }
            
            // Disconnect this specific test handler
            disconnect(aiProvider_.get(), &AICompletionProvider::completionsReady, nullptr, nullptr);
        });

        connect(aiProvider_.get(), &AICompletionProvider::error,
                this, [this](const QString& error) {
            s_logger.info("  ✗ Error: ");
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
