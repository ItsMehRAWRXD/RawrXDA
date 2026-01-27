// ============================================================================
// File: src/agent/gguf_proxy_server.cpp
// 
// Purpose: TCP proxy server implementation
// Intercepts and corrects GGUF model outputs in real-time
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "gguf_proxy_server.hpp"
#include "agent_hot_patcher.hpp"

#include <QTcpSocket>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <QMutex>

GGUFProxyServer::GGUFProxyServer(QObject* parent)
    : QTcpServer(parent)
    , m_hotPatcher(nullptr)
{
}

GGUFProxyServer::~GGUFProxyServer() noexcept
{
    try {
        stopServer();
    } catch (...) {
        qWarning() << "[GGUFProxyServer] Exception during destruction";
    }
}

void GGUFProxyServer::initialize(int listenPort, AgentHotPatcher* hotPatcher, 
                                 const QString& ggufEndpoint)
{
    // Defensive validation
    if (listenPort <= 0 || listenPort > 65535) {
        qCritical() << "[GGUFProxyServer] Invalid listen port:" << listenPort;
        return;
    }

    if (!ggufEndpoint.contains(':')) {
        qCritical() << "[GGUFProxyServer] GGUF endpoint must be host:port – got"
                    << ggufEndpoint;
        return;
    }

    m_listenPort = listenPort;
    m_hotPatcher = hotPatcher;
    m_ggufEndpoint = ggufEndpoint;
    
    qDebug() << "[GGUFProxyServer] Initialized:"
             << "Port:" << listenPort
             << "GGUF Endpoint:" << ggufEndpoint
             << "Hot Patcher:" << (hotPatcher ? "connected" : "null");
}

bool GGUFProxyServer::startServer()
{
    if (isListening()) {
        qDebug() << "[GGUFProxyServer] Already listening on port" << m_listenPort;
        return true;
    }
    
    if (listen(QHostAddress::LocalHost, m_listenPort)) {
        qDebug() << "[GGUFProxyServer] ✓ Started listening on port" << m_listenPort;
        emit serverStarted(m_listenPort);               // <<--- NEW: emit signal
        return true;
    } else {
        qWarning() << "[GGUFProxyServer] ✗ Failed to listen on port" << m_listenPort
                   << "Error:" << errorString();
        return false;
    }
}

void GGUFProxyServer::stopServer()
{
    // Close all client connections
    for (auto& conn : m_connections) {
        if (conn->clientSocket) {
            conn->clientSocket->close();
            conn->clientSocket->deleteLater();
        }
        if (conn->ggufSocket) {
            conn->ggufSocket->close();
            conn->ggufSocket->deleteLater();
        }
    }
    m_connections.clear();
    
    // Stop listening
    close();
    qDebug() << "[GGUFProxyServer] Server stopped";
    emit serverStopped();                               // <<--- NEW: emit signal
}

QJsonObject GGUFProxyServer::getServerStatistics() const
{
    QMutexLocker locker(&m_statsMutex);
    
    QJsonObject stats;
    stats["requestsProcessed"] = static_cast<qint64>(m_requestsProcessed);
    stats["hallucinationsCorrected"] = static_cast<qint64>(m_hallucinationsCorrected);
    stats["navigationErrorsFixed"] = static_cast<qint64>(m_navigationErrorsFixed);
    stats["activeConnections"] = m_activeConnections;
    stats["serverListening"] = isListening();
    stats["listenPort"] = m_listenPort;
    stats["ggufEndpoint"] = m_ggufEndpoint;
    return stats;
}

void GGUFProxyServer::setConnectionPoolSize(int size)
{
    m_connectionPoolSize = size;
    qDebug() << "[GGUFProxyServer] Connection pool size set to" << size;
}

void GGUFProxyServer::setConnectionTimeout(int ms)
{
    m_connectionTimeout = ms;
    qDebug() << "[GGUFProxyServer] Connection timeout set to" << ms << "ms";
}

bool GGUFProxyServer::isListening() const
{
    return QTcpServer::isListening();
}

void GGUFProxyServer::incomingConnection(qintptr socketDescriptor)
{
    qDebug() << "[GGUFProxyServer] New client connection:" << socketDescriptor;
    
    // Create client socket
    auto clientSocket = new QTcpSocket(this);
    if (!clientSocket->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "[GGUFProxyServer] Failed to set socket descriptor";
        delete clientSocket;
        return;
    }
    
    // Create connection entry
    auto connection = std::make_unique<ClientConnection>();
    connection->clientSocket = clientSocket;
    m_connections[socketDescriptor] = std::move(connection);
    m_activeConnections++;
    
    // Connect signals with explicit error overload
    connect(clientSocket, &QTcpSocket::readyRead, this, &GGUFProxyServer::onClientDataReceived);
    connect(clientSocket, &QTcpSocket::disconnected, this, &GGUFProxyServer::onClientDisconnected);
    connect(clientSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this,
            &GGUFProxyServer::onGGUFError);
    
    qDebug() << "[GGUFProxyServer] Client connected. Active connections:" << m_activeConnections;
}

void GGUFProxyServer::onClientDataReceived()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;
    
    // Find connection by socket
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
        [clientSocket](const std::pair<const qintptr, std::unique_ptr<ClientConnection>>& p) {
            return p.second->clientSocket == clientSocket;
        });
    
    if (it == m_connections.end()) return;
    
    qintptr socketDescriptor = it->first;
    auto& connection = it->second;
    
    // Read data from client
    QByteArray data = clientSocket->readAll();
    connection->requestBuffer.append(data);
    
    qDebug() << "[GGUFProxyServer] Received" << data.size() << "bytes from client";
    
    // Forward to GGUF
    forwardToGGUF(socketDescriptor);
}

void GGUFProxyServer::onClientDisconnected()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;
    
    // Find and remove connection
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
        [clientSocket](const std::pair<const qintptr, std::unique_ptr<ClientConnection>>& p) {
            return p.second->clientSocket == clientSocket;
        });
    
    if (it != m_connections.end()) {
        qintptr socketDescriptor = it->first;
        
        if (it->second->ggufSocket) {
            it->second->ggufSocket->close();
            it->second->ggufSocket->deleteLater();
        }
        
        m_connections.erase(it);
        m_activeConnections--;
        
        qDebug() << "[GGUFProxyServer] Client disconnected. Active connections:" << m_activeConnections;
    }
    
    clientSocket->deleteLater();
}

void GGUFProxyServer::onGGUFDataReceived()
{
    QTcpSocket* ggufSocket = qobject_cast<QTcpSocket*>(sender());
    if (!ggufSocket || !m_hotPatcher) return;
    
    // Find connection by GGUF socket
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
        [ggufSocket](const std::pair<const qintptr, std::unique_ptr<ClientConnection>>& p) {
            return p.second->ggufSocket == ggufSocket;
        });
    
    if (it == m_connections.end()) return;
    
    qintptr socketDescriptor = it->first;
    auto& connection = it->second;
    
    // Read response from GGUF
    QByteArray response = ggufSocket->readAll();
    connection->responseBuffer.append(response);
    
    qDebug() << "[GGUFProxyServer] Received" << response.size() << "bytes from GGUF";
    
    // Apply hot patching
    processGGUFResponse(socketDescriptor);
}

void GGUFProxyServer::onGGUFError()
{
    QTcpSocket* ggufSocket = qobject_cast<QTcpSocket*>(sender());
    if (!ggufSocket) return;
    
    qWarning() << "[GGUFProxyServer] GGUF socket error:" << ggufSocket->errorString();

    // Find associated client (if any) and report the problem.
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
        [ggufSocket](const auto& pair){ return pair.second->ggufSocket == ggufSocket; });

    if (it != m_connections.end()) {
        QTcpSocket* client = it->second->clientSocket;
        if (client && client->isOpen()) {
            QJsonObject err;
            err["error"] = QStringLiteral("backend_unreachable");
            err["detail"] = ggufSocket->errorString();
            QByteArray payload = QJsonDocument(err).toJson(QJsonDocument::Compact);
            client->write(payload);
        }
    }
}

void GGUFProxyServer::onGGUFDisconnected()
{
    QTcpSocket* ggufSocket = qobject_cast<QTcpSocket*>(sender());
    if (!ggufSocket) return;

    // Find its connection entry
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
        [ggufSocket](const auto& pair) {
            return pair.second->ggufSocket == ggufSocket;
        });

    if (it == m_connections.end())
        return;

    qWarning() << "[GGUFProxyServer] GGUF backend disconnected for client"
               << it->first;

    // Close the gguf side, keep client alive (client may retry)
    ggufSocket->close();
    ggufSocket->deleteLater();
    it->second->ggufSocket = nullptr;
}

void GGUFProxyServer::forwardToGGUF(qintptr socketDescriptor)
{
    auto it = m_connections.find(socketDescriptor);
    if (it == m_connections.end()) return;
    
    auto& connection = it->second;
    
    // Create GGUF socket if needed
    if (!connection->ggufSocket) {
        connection->ggufSocket = new QTcpSocket(this);
        
        // Parse GGUF endpoint
        QStringList parts = m_ggufEndpoint.split(":");
        QString host = parts.size() > 0 ? parts[0] : "localhost";
        int port = parts.size() > 1 ? parts[1].toInt() : 11434;
        
        // Connect to GGUF
        connection->ggufSocket->connectToHost(host, port);
        
        if (!connection->ggufSocket->waitForConnected(m_connectionTimeout)) {
            qWarning() << "[GGUFProxyServer] Failed to connect to GGUF at" << m_ggufEndpoint;
            return;
        }
        
        qDebug() << "[GGUFProxyServer] Connected to GGUF at" << m_ggufEndpoint;
        
        // Connect GGUF socket signals with explicit overload for error
        connect(connection->ggufSocket, &QTcpSocket::readyRead, this, &GGUFProxyServer::onGGUFDataReceived);
        connect(connection->ggufSocket, &QTcpSocket::disconnected, this, &GGUFProxyServer::onGGUFDisconnected);
        connect(connection->ggufSocket,
                QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
                this,
                &GGUFProxyServer::onGGUFError);
    }
    
    // Forward request to GGUF
    if (connection->ggufSocket && connection->ggufSocket->isOpen()) {
        connection->ggufSocket->write(connection->requestBuffer);
        connection->requestBuffer.clear();              // <<--- NEW: clear after forwarding
        
        {
            QMutexLocker locker(&m_statsMutex);
            ++m_requestsProcessed;
        }
    }
}

void GGUFProxyServer::processGGUFResponse(qintptr socketDescriptor)
{
    auto it = m_connections.find(socketDescriptor);
    if (it == m_connections.end() || !m_hotPatcher) return;
    
    auto& connection = it->second;
    
    // Apply hot patching
    QString response = QString::fromUtf8(connection->responseBuffer);
    QString corrected = m_hotPatcher->interceptModelOutput(response, QJsonObject());
    
    // Send corrected response to client
    if (connection->clientSocket && connection->clientSocket->isOpen()) {
        connection->clientSocket->write(corrected.toUtf8());
        connection->responseBuffer.clear();
    }
}

void GGUFProxyServer::sendResponseToClient(qintptr socketDescriptor, const QString& response)
{
    auto it = m_connections.find(socketDescriptor);
    if (it == m_connections.end()) return;
    
    auto& connection = it->second;
    if (connection->clientSocket && connection->clientSocket->isOpen()) {
        connection->clientSocket->write(response.toUtf8());
    }
}

QTcpSocket* GGUFProxyServer::getGGUFConnection()
{
    auto socket = new QTcpSocket(this);
    return socket;
}

void GGUFProxyServer::returnGGUFConnection(QTcpSocket* socket)
{
    if (socket) {
        socket->close();
        socket->deleteLater();
    }
}

QString GGUFProxyServer::parseIncomingRequest(const QByteArray& data)
{
    return QString::fromUtf8(data);
}
