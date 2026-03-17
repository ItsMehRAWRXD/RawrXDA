#include "gguf_server.hpp"
#include "inference_engine.hpp"
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>

void testEndpoint(const QString& method, const QString& endpoint, const QByteArray& body = QByteArray()) {
    QNetworkAccessManager manager;
    QUrl url("http://localhost:11434" + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = nullptr;
    if (method == "GET") {
        reply = manager.get(request);
    } else if (method == "POST") {
        reply = manager.post(request, body);
    } else if (method == "DELETE") {
        reply = manager.deleteResource(request);
    }
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    qInfo() << "\n=== Test:" << method << endpoint << "===";
    qInfo() << "Status Code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    QByteArray response = reply->readAll();
    if (!response.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (!doc.isNull()) {
            qInfo() << "Response:" << doc.toJson(QJsonDocument::Indented);
        } else {
            qInfo() << "Response (raw):" << response;
        }
    }
    
    reply->deleteLater();
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qInfo() << "=== GGUF Server Standalone Test ===\n";
    
    // Create minimal inference engine (stub)
    InferenceEngine* engine = new InferenceEngine();
    
    // Create and start server
    GGUFServer* server = new GGUFServer(engine);
    
    qInfo() << "Starting GGUF server...";
    if (!server->start(11434)) {
        qCritical() << "Failed to start server!";
        return 1;
    }
    
    qInfo() << "Server started on port:" << server->port();
    qInfo() << "Waiting 2 seconds for server to initialize...\n";
    QThread::sleep(2);
    
    // Test 1: Health endpoint
    testEndpoint("GET", "/health");
    
    // Test 2: Root endpoint (Ollama compatibility)
    testEndpoint("GET", "/");
    
    // Test 3: Tags endpoint (list models)
    testEndpoint("GET", "/api/tags");
    
    // Test 4: Show endpoint
    QJsonObject showRequest;
    showRequest["name"] = "test-model";
    QJsonDocument showDoc(showRequest);
    testEndpoint("POST", "/api/show", showDoc.toJson(QJsonDocument::Compact));
    
    // Test 5: Generate endpoint (non-streaming)
    QJsonObject genRequest;
    genRequest["model"] = "test-model";
    genRequest["prompt"] = "Hello, world!";
    genRequest["stream"] = false;
    genRequest["max_tokens"] = 10;
    QJsonDocument genDoc(genRequest);
    testEndpoint("POST", "/api/generate", genDoc.toJson(QJsonDocument::Compact));
    
    // Test 6: Chat completions (OpenAI format, non-streaming)
    QJsonObject chatRequest;
    chatRequest["model"] = "gpt-4";
    chatRequest["stream"] = false;
    QJsonArray messages;
    QJsonObject msg1;
    msg1["role"] = "user";
    msg1["content"] = "Hello!";
    messages.append(msg1);
    chatRequest["messages"] = messages;
    QJsonDocument chatDoc(chatRequest);
    testEndpoint("POST", "/v1/chat/completions", chatDoc.toJson(QJsonDocument::Compact));
    
    // Test 7: Invalid endpoint (404 test)
    testEndpoint("GET", "/invalid/endpoint");
    
    // Test 8: Get stats
    auto stats = server->getStats();
    qInfo() << "\n=== Server Statistics ===";
    qInfo() << "Total Requests:" << stats.totalRequests;
    qInfo() << "Successful:" << stats.successfulRequests;
    qInfo() << "Failed:" << stats.failedRequests;
    qInfo() << "Tokens Generated:" << stats.totalTokensGenerated;
    qInfo() << "Uptime:" << stats.uptimeSeconds << "seconds";
    qInfo() << "Start Time:" << stats.startTime;
    
    qInfo() << "\n=== All Tests Complete ===";
    qInfo() << "Stopping server...";
    server->stop();
    
    qInfo() << "Test suite finished successfully!";
    
    delete server;
    delete engine;
    
    return 0;
}
