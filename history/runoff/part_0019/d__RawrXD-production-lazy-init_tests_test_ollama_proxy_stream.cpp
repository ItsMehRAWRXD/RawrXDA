#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include "../include/ollama_proxy.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    OllamaProxy proxy;

    QObject::connect(&proxy, &OllamaProxy::tokenArrived, [](const QString& token){
        qInfo() << "[TEST] tokenArrived:" << token;
    });

    QObject::connect(&proxy, &OllamaProxy::generationComplete, [&](){
        qInfo() << "[TEST] generationComplete - exiting";
        QTimer::singleShot(100, &app, &QCoreApplication::quit);
    });

    QObject::connect(&proxy, &OllamaProxy::error, [&](const QString& msg){
        qWarning() << "[TEST] OllamaProxy error:" << msg;
        QTimer::singleShot(100, &app, &QCoreApplication::quit);
    });

    QString modelName = "llama3.2:3b";
    qInfo() << "[TEST] Checking if Ollama is available...";

    QTimer::singleShot(10, [&proxy, modelName](){
        if (!proxy.isOllamaAvailable()) {
            qWarning() << "[TEST] Ollama server not reachable (http://localhost:11434)";
            QTimer::singleShot(10, QCoreApplication::instance(), &QCoreApplication::quit);
            return;
        }

        qInfo() << "[TEST] Ollama available. Checking model availability for" << modelName;
        if (!proxy.isModelAvailable(modelName)) {
            qWarning() << "[TEST] Model" << modelName << "not found in Ollama registry";
            // Still proceed; Ollama may stream if model is running locally
        }

        qInfo() << "[TEST] Setting model and generating sample prompt...";
        proxy.setModel(modelName);
        proxy.generateResponse("Hello from RawrXD streaming test. Provide a short completion.", 0.0f, 64);
    });

    return app.exec();
}
