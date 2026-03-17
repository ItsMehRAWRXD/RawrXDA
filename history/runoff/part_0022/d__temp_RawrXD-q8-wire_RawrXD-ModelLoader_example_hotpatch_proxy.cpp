#include "ollama_hotpatch_proxy.hpp"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qInfo() << "=== Ollama Hotpatch Proxy Example ===\n";
    
    // Create proxy
    OllamaHotpatchProxy proxy;
    proxy.setUpstreamUrl("http://localhost:11434");
    proxy.setDebugLogging(true);
    
    // Add hotpatch rules
    qInfo() << "Configuring hotpatch rules...";
    
    // 1. Token replacements - fix common mistakes
    proxy.addTokenReplacement("definately", "definitely");
    proxy.addTokenReplacement("alot", "a lot");
    proxy.addTokenReplacement("recieve", "receive");
    
    // 2. Tone adjustments - make responses more professional
    proxy.addRegexFilter(QRegularExpression(R"(\b(can't|won't|don't)\b)"), "cannot/will not/do not");
    proxy.addRegexFilter(QRegularExpression(R"(\b(kinda|sorta|gonna)\b)"), "somewhat/going to");
    
    // 3. Fact injections - add context
    proxy.addFactInjection("Paris", "Paris (capital of France, pop. 2.1M)");
    proxy.addFactInjection("Python", "Python (programming language created by Guido van Rossum in 1991)");
    proxy.addFactInjection("quantum computing", "quantum computing (computing using quantum-mechanical phenomena)");
    
    // 4. Safety filters - block sensitive content
    proxy.addSafetyFilter(QRegularExpression(R"(\b(password|secret|confidential|classified)\b)"));
    proxy.addSafetyFilter(QRegularExpression(R"(\b(hack|exploit|vulnerability)\b)"));
    
    // 5. Custom post-processor - add disclaimer to all responses
    proxy.addCustomPostProcessor([](const QString& text) -> QString {
        if (text.contains("medical") || text.contains("health") || text.contains("diagnosis")) {
            return text + "\n\n[DISCLAIMER: This is not medical advice. Consult a healthcare professional.]";
        }
        return text;
    });
    
    // 6. Custom processor - capitalize technical terms
    proxy.addCustomPostProcessor([](const QString& text) -> QString {
        QString result = text;
        QStringList terms = {"api", "cpu", "gpu", "ram", "sql", "http", "https", "json", "xml"};
        for (const QString& term : terms) {
            QRegularExpression regex("\\b" + term + "\\b", QRegularExpression::CaseInsensitiveOption);
            result = result.replace(regex, term.toUpper());
        }
        return result;
    });
    
    // Start proxy on port 11435
    if (!proxy.start(11435)) {
        qCritical() << "Failed to start proxy!";
        return 1;
    }
    
    qInfo() << "\n╔════════════════════════════════════════════════════════════╗";
    qInfo() << "║  Ollama Hotpatch Proxy RUNNING                             ║";
    qInfo() << "╠════════════════════════════════════════════════════════════╣";
    qInfo() << "║  Proxy URL:     http://localhost:11435                     ║";
    qInfo() << "║  Upstream URL:  http://localhost:11434                     ║";
    qInfo() << "║                                                            ║";
    qInfo() << "║  Active Rules:                                             ║";
    qInfo() << "║    • Token Replacements:    3                              ║";
    qInfo() << "║    • Regex Filters:         2                              ║";
    qInfo() << "║    • Fact Injections:       3                              ║";
    qInfo() << "║    • Safety Filters:        2                              ║";
    qInfo() << "║    • Custom Processors:     2                              ║";
    qInfo() << "║                                                            ║";
    qInfo() << "║  Test with:                                                ║";
    qInfo() << "║    curl http://localhost:11435/api/generate \\             ║";
    qInfo() << "║      -d '{\"model\":\"llama2\",\"prompt\":\"test\"}'            ║";
    qInfo() << "║                                                            ║";
    qInfo() << "║  Press Ctrl+C to stop                                      ║";
    qInfo() << "╚════════════════════════════════════════════════════════════╝\n";
    
    return app.exec();
}
