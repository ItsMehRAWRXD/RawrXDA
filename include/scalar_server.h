#pragma once

#include <QObject>
#include <QString>

// Forward declarations
class QTcpServer;
class QTcpSocket;
class TransformerBlockScalar;
class InferenceEngine;

class ScalarServer : public QObject
{
    Q_OBJECT

public:
    explicit ScalarServer(QObject *parent = nullptr);
    ~ScalarServer();
    
    bool startServer(quint16 port = 8080);
    void stopServer();
    bool loadModel(const QString &modelPath);
    
    quint16 getPort() const;
    bool isRunning() const;

private slots:
    void handleNewConnection();
    void handleClientData(QTcpSocket *clientSocket);

private:
    void handleInferenceRequest(QTcpSocket *clientSocket, const QJsonObject &request);
    void handleChatRequest(QTcpSocket *clientSocket, const QJsonObject &request);
    void handleAnalyzeRequest(QTcpSocket *clientSocket, const QJsonObject &request);
    
    void sendJsonResponse(QTcpSocket *clientSocket, const QJsonObject &response);
    void sendErrorResponse(QTcpSocket *clientSocket, const QString &error);
    
    QTcpServer *m_server;
    TransformerBlockScalar *m_transformerBlock;
    InferenceEngine *m_inferenceEngine;
};