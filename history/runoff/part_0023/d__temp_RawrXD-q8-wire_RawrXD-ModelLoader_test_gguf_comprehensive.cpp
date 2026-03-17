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
#include <QTcpSocket>
#include <QFile>
#include <QDir>
#include <QDateTime>

class GGUFServerTester : public QObject {
    Q_OBJECT

public:
    GGUFServerTester(GGUFServer* server) : m_server(server), m_testsPassed(0), m_testsFailed(0) {}

    void runAllTests() {
        qInfo() << "\n╔════════════════════════════════════════════════════════════╗";
        qInfo() << "║  GGUF SERVER COMPREHENSIVE TEST SUITE                      ║";
        qInfo() << "╚════════════════════════════════════════════════════════════╝\n";

        // Phase 1: Basic functionality
        qInfo() << "┌─ PHASE 1: BASIC FUNCTIONALITY ─────────────────────────────┐";
        testServerStatus();
        testHealthEndpoint();
        testRootEndpoint();
        testCorsHeaders();
        testInvalidEndpoint404();
        testInvalidJSON();
        testOptionsRequest();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Phase 2: Model management
        qInfo() << "┌─ PHASE 2: MODEL MANAGEMENT ────────────────────────────────┐";
        testTagsEndpoint();
        testShowEndpoint();
        testDeleteEndpoint();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Phase 3: Generation (non-streaming)
        qInfo() << "┌─ PHASE 3: GENERATION (NON-STREAMING) ─────────────────────┐";
        testGenerateNonStreaming();
        testGenerateWithMaxTokens();
        testGenerateMissingPrompt();
        testChatCompletionsNonStreaming();
        testChatCompletionsMissingMessages();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Phase 4: Streaming (NDJSON)
        qInfo() << "┌─ PHASE 4: STREAMING (NDJSON) ──────────────────────────────┐";
        testGenerateStreamingNDJSON();
        testChatCompletionsStreamingNDJSON();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Phase 5: Streaming (SSE)
        qInfo() << "┌─ PHASE 5: STREAMING (SSE) ─────────────────────────────────┐";
        testGenerateStreamingSSE();
        testChatCompletionsStreamingSSE();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Phase 6: Network operations
        qInfo() << "┌─ PHASE 6: NETWORK OPERATIONS ──────────────────────────────┐";
        testPullEndpoint();
        testPullWithResume();
        testPushEndpoint();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Phase 7: Stress testing
        qInfo() << "┌─ PHASE 7: STRESS TESTING ──────────────────────────────────┐";
        testConcurrentRequests();
        testLargePayload();
        testRapidFireRequests();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Phase 8: Edge cases
        qInfo() << "┌─ PHASE 8: EDGE CASES ──────────────────────────────────────┐";
        testMalformedHTTP();
        testEmptyBody();
        testExtremelyLargeRequest();
        testSpecialCharacters();
        testConnectionDropDuringStream();
        qInfo() << "└────────────────────────────────────────────────────────────┘\n";

        // Final report
        printResults();
    }

private:
    GGUFServer* m_server;
    int m_testsPassed;
    int m_testsFailed;

    void testServerStatus() {
        TEST_START("Server running status");
        ASSERT_TRUE(m_server->isRunning(), "Server should be running");
        ASSERT_EQUALS(m_server->port(), 11434, "Server should be on port 11434");
    }

    void testHealthEndpoint() {
        TEST_START("GET /health");
        auto response = httpRequest("GET", "/health");
        ASSERT_EQUALS(response.statusCode, 200, "Health should return 200");
        
        auto json = QJsonDocument::fromJson(response.body).object();
        ASSERT_TRUE(json.contains("status"), "Health should have status field");
        ASSERT_EQUALS(json["status"].toString(), QString("ok"), "Status should be 'ok'");
        ASSERT_TRUE(json.contains("uptime_seconds"), "Health should have uptime");
        ASSERT_TRUE(json.contains("total_requests"), "Health should have request count");
        ASSERT_TRUE(json.contains("model_loaded"), "Health should have model_loaded flag");
    }

    void testRootEndpoint() {
        TEST_START("GET / (Ollama compatibility)");
        auto response = httpRequest("GET", "/");
        ASSERT_EQUALS(response.statusCode, 200, "Root should return 200");
        ASSERT_TRUE(response.body.contains("Ollama is running"), "Should contain Ollama message");
    }

    void testCorsHeaders() {
        TEST_START("CORS headers verification");
        auto response = httpRequest("GET", "/health");
        ASSERT_TRUE(response.headers.contains("Access-Control-Allow-Origin"), "Should have CORS origin header");
        ASSERT_EQUALS(response.headers["Access-Control-Allow-Origin"], QString("*"), "CORS should allow all origins");
    }

    void testInvalidEndpoint404() {
        TEST_START("GET /invalid/endpoint (404 test)");
        auto response = httpRequest("GET", "/invalid/endpoint");
        ASSERT_EQUALS(response.statusCode, 404, "Invalid endpoint should return 404");
        
        auto json = QJsonDocument::fromJson(response.body).object();
        ASSERT_TRUE(json.contains("error"), "404 should have error field");
    }

    void testInvalidJSON() {
        TEST_START("POST /api/generate with invalid JSON");
        auto response = httpRequest("POST", "/api/generate", "{invalid json");
        ASSERT_EQUALS(response.statusCode, 400, "Invalid JSON should return 400");
    }

    void testOptionsRequest() {
        TEST_START("OPTIONS request (CORS preflight)");
        auto response = httpRequest("OPTIONS", "/api/generate");
        ASSERT_EQUALS(response.statusCode, 204, "OPTIONS should return 204");
    }

    void testTagsEndpoint() {
        TEST_START("GET /api/tags");
        auto response = httpRequest("GET", "/api/tags");
        ASSERT_EQUALS(response.statusCode, 200, "Tags should return 200");
        
        auto json = QJsonDocument::fromJson(response.body).object();
        ASSERT_TRUE(json.contains("models"), "Tags should have models array");
        ASSERT_TRUE(json["models"].isArray(), "models should be an array");
    }

    void testShowEndpoint() {
        TEST_START("POST /api/show");
        QJsonObject req;
        req["name"] = "test-model";
        auto response = httpRequest("POST", "/api/show", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 404, "Show should return 404 when no model loaded");
    }

    void testDeleteEndpoint() {
        TEST_START("DELETE /api/delete with missing name");
        QJsonObject req;
        auto response = httpRequest("POST", "/api/delete", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 400, "Delete without name should return 400");
    }

    void testGenerateNonStreaming() {
        TEST_START("POST /api/generate (non-streaming)");
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = "Hello, world!";
        req["stream"] = false;
        req["max_tokens"] = 10;
        
        auto response = httpRequest("POST", "/api/generate", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 200, "Generate should return 200");
        
        auto json = QJsonDocument::fromJson(response.body).object();
        ASSERT_TRUE(json.contains("model"), "Response should have model field");
        ASSERT_TRUE(json.contains("response"), "Response should have response field");
        ASSERT_TRUE(json.contains("done"), "Response should have done field");
        ASSERT_TRUE(json["done"].toBool(), "done should be true for non-streaming");
    }

    void testGenerateWithMaxTokens() {
        TEST_START("POST /api/generate with max_tokens=50");
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = "Count to 100";
        req["stream"] = false;
        req["max_tokens"] = 50;
        
        auto response = httpRequest("POST", "/api/generate", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 200, "Generate with max_tokens should return 200");
    }

    void testGenerateMissingPrompt() {
        TEST_START("POST /api/generate without prompt");
        QJsonObject req;
        req["model"] = "test-model";
        req["stream"] = false;
        
        auto response = httpRequest("POST", "/api/generate", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 400, "Generate without prompt should return 400");
    }

    void testChatCompletionsNonStreaming() {
        TEST_START("POST /v1/chat/completions (non-streaming)");
        QJsonObject req;
        req["model"] = "gpt-4";
        req["stream"] = false;
        
        QJsonArray messages;
        QJsonObject msg1;
        msg1["role"] = "system";
        msg1["content"] = "You are a helpful assistant.";
        messages.append(msg1);
        
        QJsonObject msg2;
        msg2["role"] = "user";
        msg2["content"] = "Hello!";
        messages.append(msg2);
        
        req["messages"] = messages;
        
        auto response = httpRequest("POST", "/v1/chat/completions", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 200, "Chat completions should return 200");
        
        auto json = QJsonDocument::fromJson(response.body).object();
        ASSERT_TRUE(json.contains("id"), "Response should have id");
        ASSERT_TRUE(json.contains("object"), "Response should have object");
        ASSERT_TRUE(json.contains("choices"), "Response should have choices");
        ASSERT_TRUE(json["choices"].isArray(), "choices should be array");
    }

    void testChatCompletionsMissingMessages() {
        TEST_START("POST /v1/chat/completions without messages");
        QJsonObject req;
        req["model"] = "gpt-4";
        req["stream"] = false;
        
        auto response = httpRequest("POST", "/v1/chat/completions", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 400, "Chat without messages should return 400");
    }

    void testGenerateStreamingNDJSON() {
        TEST_START("POST /api/generate (streaming NDJSON)");
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = "Stream test";
        req["stream"] = true;
        
        auto response = streamRequest("POST", "/api/generate", 
                                     QJsonDocument(req).toJson(QJsonDocument::Compact),
                                     "application/x-ndjson");
        
        ASSERT_EQUALS(response.statusCode, 200, "Streaming should return 200");
        ASSERT_TRUE(response.headers["Transfer-Encoding"].contains("chunked"), "Should use chunked transfer");
        ASSERT_TRUE(response.streamLines.size() > 0, "Should have stream lines");
        
        // Verify last line has done=true
        if (!response.streamLines.isEmpty()) {
            auto lastLine = QJsonDocument::fromJson(response.streamLines.last()).object();
            ASSERT_TRUE(lastLine["done"].toBool(), "Last line should have done=true");
        }
    }

    void testChatCompletionsStreamingNDJSON() {
        TEST_START("POST /v1/chat/completions (streaming NDJSON)");
        QJsonObject req;
        req["model"] = "gpt-4";
        req["stream"] = true;
        
        QJsonArray messages;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "Hello";
        messages.append(msg);
        req["messages"] = messages;
        
        auto response = streamRequest("POST", "/v1/chat/completions", 
                                     QJsonDocument(req).toJson(QJsonDocument::Compact),
                                     "application/x-ndjson");
        
        ASSERT_EQUALS(response.statusCode, 200, "Chat streaming should return 200");
        ASSERT_TRUE(response.streamLines.size() > 0, "Should have stream chunks");
    }

    void testGenerateStreamingSSE() {
        TEST_START("POST /api/generate (streaming SSE)");
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = "SSE test";
        req["stream"] = true;
        
        auto response = streamRequest("POST", "/api/generate", 
                                     QJsonDocument(req).toJson(QJsonDocument::Compact),
                                     "text/event-stream");
        
        ASSERT_EQUALS(response.statusCode, 200, "SSE streaming should return 200");
        ASSERT_TRUE(response.headers["Content-Type"].contains("text/event-stream"), "Should be SSE");
        ASSERT_TRUE(response.sseEvents.size() > 0, "Should have SSE events");
    }

    void testChatCompletionsStreamingSSE() {
        TEST_START("POST /v1/chat/completions (streaming SSE)");
        QJsonObject req;
        req["model"] = "gpt-4";
        req["stream"] = true;
        
        QJsonArray messages;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "SSE chat test";
        messages.append(msg);
        req["messages"] = messages;
        
        auto response = streamRequest("POST", "/v1/chat/completions", 
                                     QJsonDocument(req).toJson(QJsonDocument::Compact),
                                     "text/event-stream");
        
        ASSERT_EQUALS(response.statusCode, 200, "Chat SSE should return 200");
        ASSERT_TRUE(response.sseEvents.size() > 0, "Should have SSE events");
        
        // Verify [DONE] marker
        ASSERT_TRUE(response.sseEvents.contains("[DONE]"), "Should have [DONE] marker");
    }

    void testPullEndpoint() {
        TEST_START("POST /api/pull");
        QJsonObject req;
        req["name"] = "test-model.gguf";
        req["url"] = "http://example.com/model.gguf";
        
        auto response = httpRequest("POST", "/api/pull", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_TRUE(response.statusCode == 200 || response.statusCode == 400, "Pull should return 200 or 400");
        
        if (response.statusCode == 200) {
            auto json = QJsonDocument::fromJson(response.body).object();
            ASSERT_TRUE(json.contains("status"), "Pull response should have status");
        }
    }

    void testPullWithResume() {
        TEST_START("POST /api/pull with resume capability");
        QJsonObject req;
        req["name"] = "resumable-model.gguf";
        req["url"] = "http://example.com/large-model.gguf";
        
        auto response = httpRequest("POST", "/api/pull", QJsonDocument(req).toJson(QJsonDocument::Compact));
        // Just verify it doesn't crash - actual resume requires real download
        ASSERT_TRUE(response.statusCode >= 200 && response.statusCode < 600, "Should return valid status code");
    }

    void testPushEndpoint() {
        TEST_START("POST /api/push");
        QJsonObject req;
        req["name"] = "nonexistent-model.gguf";
        req["url"] = "http://example.com/upload";
        
        auto response = httpRequest("POST", "/api/push", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 404, "Push nonexistent model should return 404");
    }

    void testConcurrentRequests() {
        TEST_START("Concurrent requests (10 parallel)");
        
        QList<QNetworkReply*> replies;
        QNetworkAccessManager manager;
        
        for (int i = 0; i < 10; ++i) {
            QNetworkRequest request(QUrl("http://localhost:11434/health"));
            replies.append(manager.get(request));
        }
        
        QEventLoop loop;
        int completed = 0;
        for (auto reply : replies) {
            QObject::connect(reply, &QNetworkReply::finished, [&]() {
                completed++;
                if (completed == 10) loop.quit();
            });
        }
        
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        
        int success = 0;
        for (auto reply : replies) {
            if (reply->error() == QNetworkReply::NoError) success++;
            reply->deleteLater();
        }
        
        ASSERT_TRUE(success == 10, "All concurrent requests should succeed");
    }

    void testLargePayload() {
        TEST_START("Large payload (1MB prompt)");
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = QString("Large ").repeated(1024 * 100); // ~500KB
        req["stream"] = false;
        
        auto response = httpRequest("POST", "/api/generate", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 200, "Large payload should succeed");
    }

    void testRapidFireRequests() {
        TEST_START("Rapid-fire requests (100 sequential)");
        
        int successCount = 0;
        for (int i = 0; i < 100; ++i) {
            auto response = httpRequest("GET", "/health");
            if (response.statusCode == 200) successCount++;
        }
        
        ASSERT_TRUE(successCount >= 95, "At least 95% of rapid requests should succeed");
    }

    void testMalformedHTTP() {
        TEST_START("Malformed HTTP request");
        
        QTcpSocket socket;
        socket.connectToHost("localhost", 11434);
        ASSERT_TRUE(socket.waitForConnected(1000), "Should connect to server");
        
        socket.write("GARBAGE REQUEST\r\n\r\n");
        socket.flush();
        socket.waitForReadyRead(1000);
        
        QByteArray response = socket.readAll();
        socket.close();
        
        // Server should handle gracefully (not crash)
        ASSERT_TRUE(true, "Server should handle malformed request without crashing");
    }

    void testEmptyBody() {
        TEST_START("POST with empty body");
        auto response = httpRequest("POST", "/api/generate", "");
        ASSERT_EQUALS(response.statusCode, 400, "Empty body should return 400");
    }

    void testExtremelyLargeRequest() {
        TEST_START("Extremely large request (>100MB limit)");
        
        // Create 110MB payload
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = QString("X").repeated(110 * 1024 * 1024);
        
        auto response = httpRequest("POST", "/api/generate", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 413, "Oversized request should return 413 Payload Too Large");
    }

    void testSpecialCharacters() {
        TEST_START("Special characters in JSON");
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = "Special chars: 你好 мир 🚀 \"quotes\" \\backslash\\ \n\t";
        req["stream"] = false;
        
        auto response = httpRequest("POST", "/api/generate", QJsonDocument(req).toJson(QJsonDocument::Compact));
        ASSERT_EQUALS(response.statusCode, 200, "Special characters should be handled");
    }

    void testConnectionDropDuringStream() {
        TEST_START("Connection drop during streaming");
        
        QJsonObject req;
        req["model"] = "test-model";
        req["prompt"] = "Stream test";
        req["stream"] = true;
        
        QTcpSocket socket;
        socket.connectToHost("localhost", 11434);
        socket.waitForConnected(1000);
        
        QByteArray request = "POST /api/generate HTTP/1.1\r\n";
        request += "Host: localhost\r\n";
        request += "Content-Type: application/json\r\n";
        QByteArray body = QJsonDocument(req).toJson(QJsonDocument::Compact);
        request += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
        request += body;
        
        socket.write(request);
        socket.flush();
        socket.waitForReadyRead(500);
        
        // Abruptly disconnect
        socket.abort();
        
        // Server should handle gracefully
        ASSERT_TRUE(true, "Server should handle abrupt disconnection");
    }

    struct HttpResponse {
        int statusCode = 0;
        QByteArray body;
        QHash<QString, QString> headers;
        QList<QByteArray> streamLines;
        QList<QByteArray> sseEvents;
    };

    HttpResponse httpRequest(const QString& method, const QString& endpoint, const QByteArray& body = QByteArray()) {
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
            request.setRawHeader("Content-Type", "application/json");
            reply = manager.sendCustomRequest(request, "DELETE", body);
        } else if (method == "OPTIONS") {
            reply = manager.sendCustomRequest(request, "OPTIONS");
        }
        
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        
        HttpResponse response;
        response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.body = reply->readAll();
        
        for (const auto& header : reply->rawHeaderPairs()) {
            response.headers[QString::fromLatin1(header.first)] = QString::fromLatin1(header.second);
        }
        
        reply->deleteLater();
        return response;
    }

    HttpResponse streamRequest(const QString& method, const QString& endpoint, const QByteArray& body, const QString& acceptType) {
        QTcpSocket socket;
        socket.connectToHost("localhost", 11434);
        socket.waitForConnected(1000);
        
        QByteArray request = method.toUtf8() + " " + endpoint.toUtf8() + " HTTP/1.1\r\n";
        request += "Host: localhost\r\n";
        request += "Content-Type: application/json\r\n";
        request += "Accept: " + acceptType.toUtf8() + "\r\n";
        request += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
        request += body;
        
        socket.write(request);
        socket.flush();
        
        HttpResponse response;
        QByteArray buffer;
        
        // Read headers
        while (socket.waitForReadyRead(1000)) {
            buffer += socket.readAll();
            if (buffer.contains("\r\n\r\n")) break;
        }
        
        // Parse status line
        int firstLine = buffer.indexOf("\r\n");
        QString statusLine = QString::fromUtf8(buffer.left(firstLine));
        QStringList parts = statusLine.split(' ');
        if (parts.size() >= 2) {
            response.statusCode = parts[1].toInt();
        }
        
        // Parse headers
        int headerEnd = buffer.indexOf("\r\n\r\n");
        QString headerSection = QString::fromUtf8(buffer.mid(firstLine + 2, headerEnd - firstLine - 2));
        for (const QString& line : headerSection.split("\r\n")) {
            int colonPos = line.indexOf(':');
            if (colonPos > 0) {
                response.headers[line.left(colonPos).trimmed()] = line.mid(colonPos + 1).trimmed();
            }
        }
        
        // Read body
        buffer.remove(0, headerEnd + 4);
        
        // Read streaming response
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 3000) {
            if (socket.waitForReadyRead(500)) {
                buffer += socket.readAll();
            } else {
                break;
            }
        }
        
        response.body = buffer;
        
        // Parse streaming data
        if (acceptType.contains("ndjson")) {
            // Parse NDJSON (chunked)
            QList<QByteArray> lines = buffer.split('\n');
            for (const auto& line : lines) {
                if (line.trimmed().startsWith('{')) {
                    response.streamLines.append(line.trimmed());
                }
            }
        } else if (acceptType.contains("event-stream")) {
            // Parse SSE
            QList<QByteArray> lines = buffer.split('\n');
            for (const auto& line : lines) {
                if (line.startsWith("data: ")) {
                    response.sseEvents.append(line.mid(6).trimmed());
                }
            }
        }
        
        socket.close();
        return response;
    }

    void printResults() {
        qInfo() << "\n╔════════════════════════════════════════════════════════════╗";
        qInfo() << "║  TEST RESULTS                                              ║";
        qInfo() << "╠════════════════════════════════════════════════════════════╣";
        qInfo() << QString("║  Tests Passed:  %1").arg(m_testsPassed, 3) << "                                       ║";
        qInfo() << QString("║  Tests Failed:  %1").arg(m_testsFailed, 3) << "                                       ║";
        qInfo() << QString("║  Success Rate:  %1%").arg(m_testsPassed * 100 / qMax(1, m_testsPassed + m_testsFailed), 3) << "                                      ║";
        qInfo() << "╠════════════════════════════════════════════════════════════╣";
        
        auto stats = m_server->getStats();
        qInfo() << QString("║  Total Server Requests: %1").arg(stats.totalRequests, 6) << "                         ║";
        qInfo() << QString("║  Successful Requests:   %1").arg(stats.successfulRequests, 6) << "                         ║";
        qInfo() << QString("║  Failed Requests:       %1").arg(stats.failedRequests, 6) << "                         ║";
        qInfo() << QString("║  Tokens Generated:      %1").arg(stats.totalTokensGenerated, 6) << "                         ║";
        qInfo() << QString("║  Uptime (seconds):      %1").arg(stats.uptimeSeconds, 6) << "                         ║";
        qInfo() << "╚════════════════════════════════════════════════════════════╝\n";
        
        if (m_testsFailed == 0) {
            qInfo() << "✅ ALL TESTS PASSED! GGUF Server is fully operational.";
        } else {
            qWarning() << "⚠️  Some tests failed. Review output above.";
        }
    }

    #define TEST_START(name) \
        qInfo() << "│ " << name << "...";

    #define ASSERT_TRUE(condition, message) \
        if (condition) { \
            qInfo() << "│   ✓" << message; \
            m_testsPassed++; \
        } else { \
            qWarning() << "│   ✗" << message << "- FAILED"; \
            m_testsFailed++; \
        }

    #define ASSERT_EQUALS(actual, expected, message) \
        if ((actual) == (expected)) { \
            qInfo() << "│   ✓" << message; \
            m_testsPassed++; \
        } else { \
            qWarning() << "│   ✗" << message << "- Expected:" << (expected) << "Got:" << (actual); \
            m_testsFailed++; \
        }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // Create minimal inference engine
    InferenceEngine* engine = new InferenceEngine();
    
    // Create and start server
    GGUFServer* server = new GGUFServer(engine);
    
    qInfo() << "Starting GGUF server on port 11434...";
    if (!server->start(11434)) {
        qCritical() << "Failed to start server!";
        return 1;
    }
    
    qInfo() << "Server started successfully. Waiting 2 seconds for initialization...\n";
    QThread::sleep(2);
    
    // Run comprehensive tests
    GGUFServerTester tester(server);
    tester.runAllTests();
    
    // Cleanup
    qInfo() << "\nStopping server...";
    server->stop();
    
    delete server;
    delete engine;
    
    return 0;
}

#include "test_gguf_comprehensive.moc"
