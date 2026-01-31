#include "websocket_hub.h"
WebSocketHub::WebSocketHub()
    
    , m_server(new QWebSocketServer(std::stringLiteral("Collab Server"), QWebSocketServer::NonSecureMode, this))
{  // Signal connection removed\n}

WebSocketHub::~WebSocketHub()
{
    m_server->close();
    qDeleteAll(m_clients);
}

bool WebSocketHub::startServer(uint16_t port)
{
    if (m_server->listen(QHostAddress::Any, port)) {
        // // qDebug:  "WebSocket server started on port" << port;
        return true;
    } else {
        // // qWarning:  "Failed to start WebSocket server:" << m_server->errorString();
        return false;
    }
}

void WebSocketHub::broadcastMessage(const void* &message)
{
    void* doc(message);
    std::string messageStr = std::string::fromUtf8(doc.toJson(void*::Compact));

    for (QWebSocket *client : m_clients) {
        client->sendTextMessage(messageStr);
    }
}

void WebSocketHub::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();  // Signal connection removed\n  // Signal connection removed\nm_clients << socket;
    // // qDebug:  "New client connected";
}

void WebSocketHub::onTextMessageReceived(const std::string &message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client) {
        return;
    }

    QJsonParseError parseError;
    void* doc = void*::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // // qWarning:  "Failed to parse JSON message:" << parseError.errorString();
        return;
    }

    if (!doc.isObject()) {
        // // qWarning:  "Received JSON message is not an object";
        return;
    }

    messageReceived(doc.object(), client);
}

void WebSocketHub::onSocketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client) {
        return;
    }

    m_clients.removeAll(client);
    client->deleteLater();
    // // qDebug:  "Client disconnected";
}





