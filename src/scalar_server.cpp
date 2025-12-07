// Scalar Server - Lightweight inference server for scalar operations

#include "scalar_server.h"
#include "inference_engine.h"
#include "transformer_block_scalar.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QDebug>

ScalarServer::ScalarServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_transformerBlock(new TransformerBlockScalar(this))
    , m_inferenceEngine(new InferenceEngine(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &ScalarServer::handleNewConnection);
}

ScalarServer::~ScalarServer()
{
    stopServer();
}

bool ScalarServer::startServer(quint16 port)
{
    if (m_server->isListening()) {
        qWarning() << "Server already running on port" << m_server->serverPort();
        return true;
    }
    
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "Failed to start server on port" << port << ":" << m_server->errorString();
        return false;
    }
    
    qInfo() << "Scalar server started on port" << port;
    return true;
}

void ScalarServer::stopServer()
{
    if (m_server->isListening()) {
        m_server->close();
        qInfo() << "Scalar server stopped";
    }
}

void ScalarServer::handleNewConnection()
{
    QTcpSocket *clientSocket = m_server->nextPendingConnection();
    
    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        handleClientData(clientSocket);
    });
    
    connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
    
    qDebug() << "New client connected:" << clientSocket->peerAddress().toString();
}

void ScalarServer::handleClientData(QTcpSocket *clientSocket)
{
    QByteArray data = clientSocket->readAll();
    
    // Parse JSON request
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        sendErrorResponse(clientSocket, "Invalid JSON");
        return;
    }
    
    QJsonObject request = doc.object();
    QString method = request.value("method").toString();
    
    if (method == "inference") {
        handleInferenceRequest(clientSocket, request);
    } else if (method == "chat") {
        handleChatRequest(clientSocket, request);
    } else if (method == "analyze") {
        handleAnalyzeRequest(clientSocket, request);
    } else {
        sendErrorResponse(clientSocket, "Unknown method: " + method);
    }
}

void ScalarServer::handleInferenceRequest(QTcpSocket *clientSocket, const QJsonObject &request)
{
    QJsonArray inputArray = request.value("input").toArray();
    uint32_t layerIdx = request.value("layer").toInt();
    uint32_t seqLen = request.value("seq_len").toInt();
    
    // Convert input to float array
    std::vector<float> input(inputArray.size());
    for (int i = 0; i < inputArray.size(); ++i) {
        input[i] = inputArray[i].toDouble();
    }
    
    // Perform inference
    std::vector<float> output(input.size());
    bool success = m_transformerBlock->forwardPass(input.data(), output.data(), layerIdx, seqLen);
    
    // Prepare response
    QJsonObject response;
    response["success"] = success;
    
    if (success) {
        QJsonArray outputArray;
        for (float val : output) {
            outputArray.append(val);
        }
        response["output"] = outputArray;
    } else {
        response["error"] = "Inference failed";
    }
    
    sendJsonResponse(clientSocket, response);
}

void ScalarServer::handleChatRequest(QTcpSocket *clientSocket, const QJsonObject &request)
{
    QString message = request.value("message").toString();
    
    // Process chat message through inference engine
    QString response = m_inferenceEngine->processChat(message);
    
    QJsonObject jsonResponse;
    jsonResponse["success"] = true;
    jsonResponse["response"] = response;
    
    sendJsonResponse(clientSocket, jsonResponse);
}

void ScalarServer::handleAnalyzeRequest(QTcpSocket *clientSocket, const QJsonObject &request)
{
    QString code = request.value("code").toString();
    
    // Analyze code through inference engine
    QString analysis = m_inferenceEngine->analyzeCode(code);
    
    QJsonObject jsonResponse;
    jsonResponse["success"] = true;
    jsonResponse["analysis"] = analysis;
    
    sendJsonResponse(clientSocket, jsonResponse);
}

void ScalarServer::sendJsonResponse(QTcpSocket *clientSocket, const QJsonObject &response)
{
    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    clientSocket->write(data);
    clientSocket->flush();
}

void ScalarServer::sendErrorResponse(QTcpSocket *clientSocket, const QString &error)
{
    QJsonObject response;
    response["success"] = false;
    response["error"] = error;
    
    sendJsonResponse(clientSocket, response);
}

bool ScalarServer::loadModel(const QString &modelPath)
{
    // Load model weights into transformer block
    // This would typically involve GGUF loader integration
    qInfo() << "Loading model from:" << modelPath;
    
    // For now, initialize with default parameters
    return m_transformerBlock->initialize(32, 32, 128, 4096);
}

quint16 ScalarServer::getPort() const
{
    return m_server->serverPort();
}

bool ScalarServer::isRunning() const
{
    return m_server->isListening();
}