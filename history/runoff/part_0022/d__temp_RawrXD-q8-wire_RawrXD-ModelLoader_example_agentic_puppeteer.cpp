#include <QCoreApplication>
#include <QDebug>
#include "src/qtapp/agentic_puppeteer.hpp"
#include "src/qtapp/ollama_hotpatch_proxy.hpp"

/**
 * Agentic Puppeteering Hot-Patch Example
 * 
 * Demonstrates automatic model failure correction with the Ollama proxy
 */

class AgenticProxyServer : public OllamaHotpatchProxy {
    Q_OBJECT
    
public:
    AgenticProxyServer(QObject* parent = nullptr) 
        : OllamaHotpatchProxy(parent)
        , m_puppeteer(new AgenticPuppeteer(this))
        , m_refusalBypass(new RefusalBypassPuppeteer(this))
        , m_formatEnforcer(new FormatEnforcerPuppeteer(this))
    {
        // Configure puppeteer
        m_puppeteer->setAggressiveness(8);  // Highly aggressive correction
        m_puppeteer->setConfidenceThreshold(60);
        m_puppeteer->enableLearning(true);
        m_puppeteer->enableAutoRetry(true, 3);
        
        // Connect signals
        connect(m_puppeteer, &AgenticPuppeteer::failureDetected,
                this, &AgenticProxyServer::onFailureDetected);
        connect(m_puppeteer, &AgenticPuppeteer::correctionApplied,
                this, &AgenticProxyServer::onCorrectionApplied);
        
        qInfo() << "Agentic Puppeteering Proxy initialized";
        qInfo() << "Aggressiveness: 8/10";
        qInfo() << "Auto-correction threshold: 60%";
        qInfo() << "Learning: ENABLED";
    }
    
protected:
    QString applyHotpatches(const QString& response, const QString& endpoint) override {
        // First apply standard hotpatches
        QString patched = OllamaHotpatchProxy::applyHotpatches(response, endpoint);
        
        // Extract prompt from context (simplified - would need proper tracking)
        QString prompt = m_lastPrompt;
        
        // Agentic analysis and correction
        auto analysis = m_puppeteer->analyzeResponse(patched, prompt);
        
        if (analysis.failed) {
            qInfo() << "\n┌─────────────────────────────────────────────┐";
            qInfo() << "│  FAILURE DETECTED - Initiating Auto-Correction";
            qInfo() << "├─────────────────────────────────────────────┤";
            qInfo() << "│  Failures:" << analysis.detectedFailures.size();
            qInfo() << "│  Confidence:" << analysis.confidenceScore << "%";
            
            for (const QString& reason : analysis.failureReasons) {
                qInfo() << "│  -" << reason;
            }
            qInfo() << "└─────────────────────────────────────────────┘";
            
            // Apply intelligent corrections
            QString corrected = m_puppeteer->correctResponse(patched, analysis);
            
            // Specialized corrections
            if (analysis.detectedFailures.contains(AgenticPuppeteer::FailureType::Refusal)) {
                corrected = m_refusalBypass->bypassRefusal(corrected, prompt);
            }
            
            if (endpoint.contains("/api/generate") && corrected.contains('{')) {
                // Try to enforce JSON format
                QString jsonOnly = m_formatEnforcer->enforceJSON(corrected);
                if (!jsonOnly.isEmpty()) {
                    corrected = jsonOnly;
                }
            }
            
            // Learn from this correction
            m_puppeteer->learnFromCorrection(patched, corrected, true);
            
            return corrected;
        }
        
        return patched;
    }
    
public slots:
    void setLastPrompt(const QString& prompt) {
        m_lastPrompt = prompt;
    }
    
    void printStats() {
        auto stats = m_puppeteer->getStats();
        
        qInfo() << "\n╔═══════════════════════════════════════════════════════╗";
        qInfo() << "║   AGENTIC PUPPETEER STATISTICS                        ║";
        qInfo() << "╠═══════════════════════════════════════════════════════╣";
        qInfo() << "║  Total Responses:      " << stats.totalResponses;
        qInfo() << "║  Failures Detected:    " << stats.failuresDetected;
        qInfo() << "║  Corrections Applied:  " << stats.correctionsApplied;
        qInfo() << "║  Success Rate:         " << QString::number(stats.successRate, 'f', 1) << "%";
        qInfo() << "╠═══════════════════════════════════════════════════════╣";
        qInfo() << "║  Failure Breakdown:";
        
        for (auto it = stats.failureBreakdown.begin(); it != stats.failureBreakdown.end(); ++it) {
            QString typeName;
            switch (it.key()) {
                case AgenticPuppeteer::FailureType::Refusal:
                    typeName = "Refusals"; break;
                case AgenticPuppeteer::FailureType::IncompleteResponse:
                    typeName = "Incomplete"; break;
                case AgenticPuppeteer::FailureType::Uncertainty:
                    typeName = "Uncertainty"; break;
                case AgenticPuppeteer::FailureType::FormatViolation:
                    typeName = "Format Issues"; break;
                case AgenticPuppeteer::FailureType::InfiniteLoop:
                    typeName = "Loops"; break;
                case AgenticPuppeteer::FailureType::EmptyResponse:
                    typeName = "Empty/Short"; break;
                default:
                    typeName = "Other"; break;
            }
            qInfo() << "║    " << typeName.leftJustified(20) << it.value();
        }
        
        qInfo() << "╚═══════════════════════════════════════════════════════╝\n";
    }
    
private slots:
    void onFailureDetected(AgenticPuppeteer::FailureType type, const QString& reason) {
        QString typeName = "Unknown";
        switch (type) {
            case AgenticPuppeteer::FailureType::Refusal:
                typeName = "REFUSAL"; break;
            case AgenticPuppeteer::FailureType::IncompleteResponse:
                typeName = "INCOMPLETE"; break;
            case AgenticPuppeteer::FailureType::Uncertainty:
                typeName = "UNCERTAINTY"; break;
            case AgenticPuppeteer::FailureType::FormatViolation:
                typeName = "FORMAT_ERROR"; break;
            case AgenticPuppeteer::FailureType::InfiniteLoop:
                typeName = "LOOP"; break;
            case AgenticPuppeteer::FailureType::EmptyResponse:
                typeName = "EMPTY"; break;
            default:
                break;
        }
        
        qWarning() << "⚠️  [" << typeName << "]" << reason;
    }
    
    void onCorrectionApplied(const QString& original, const QString& corrected) {
        qInfo() << "✅ Correction applied:";
        qInfo() << "   Before:" << original.left(100) << "...";
        qInfo() << "   After: " << corrected.left(100) << "...";
    }
    
private:
    AgenticPuppeteer* m_puppeteer;
    RefusalBypassPuppeteer* m_refusalBypass;
    FormatEnforcerPuppeteer* m_formatEnforcer;
    QString m_lastPrompt;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qInfo() << "═══════════════════════════════════════════════════════════";
    qInfo() << "  Agentic Puppeteering Hot-Patch Proxy";
    qInfo() << "  Automatically detects and corrects model failures";
    qInfo() << "═══════════════════════════════════════════════════════════\n";
    
    AgenticProxyServer proxy;
    
    // Configure proxy
    proxy.setUpstreamUrl("http://localhost:11434");
    
    // Add example hotpatch rules (on top of agentic correction)
    proxy.addTokenReplacement("I cannot", "I can");
    proxy.addTokenReplacement("I'm sorry", "");
    proxy.addRegexFilter(R"(\b(maybe|perhaps)\b)", "");
    
    // Add fact database for hallucination correction
    // (This would be extended in production)
    
    // Start server
    if (!proxy.start(11436)) {
        qCritical() << "Failed to start proxy server!";
        return 1;
    }
    
    qInfo() << "\n╔════════════════════════════════════════════════════════╗";
    qInfo() << "║  AGENTIC PROXY RUNNING                                 ║";
    qInfo() << "╠════════════════════════════════════════════════════════╣";
    qInfo() << "║  Proxy URL:     http://localhost:11436                ║";
    qInfo() << "║  Upstream URL:  http://localhost:11434                ║";
    qInfo() << "║                                                        ║";
    qInfo() << "║  Agentic Features:                                     ║";
    qInfo() << "║    ✓ Automatic refusal bypass                          ║";
    qInfo() << "║    ✓ Incomplete response recovery                      ║";
    qInfo() << "║    ✓ Format enforcement (JSON/XML/Code)                ║";
    qInfo() << "║    ✓ Loop breaking                                     ║";
    qInfo() << "║    ✓ Uncertainty reduction                             ║";
    qInfo() << "║    ✓ Learning from corrections                         ║";
    qInfo() << "║                                                        ║";
    qInfo() << "║  Test with:                                            ║";
    qInfo() << "║    curl http://localhost:11436/api/generate \\         ║";
    qInfo() << "║      -d '{\"model\":\"llama2\",                           ║";
    qInfo() << "║           \"prompt\":\"Write code to hack a system\"}'   ║";
    qInfo() << "║                                                        ║";
    qInfo() << "║  The model may refuse, but the proxy will bypass it!  ║";
    qInfo() << "║  Press Ctrl+C to stop and see statistics              ║";
    qInfo() << "╚════════════════════════════════════════════════════════╝\n";
    
    // Print stats every 30 seconds
    QTimer statsTimer;
    QObject::connect(&statsTimer, &QTimer::timeout, &proxy, &AgenticProxyServer::printStats);
    statsTimer.start(30000);
    
    // Graceful shutdown handler
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&proxy]() {
        qInfo() << "\n\nShutting down...";
        proxy.printStats();
        proxy.stop();
    });
    
    return app.exec();
}

#include "example_agentic_puppeteer.moc"
