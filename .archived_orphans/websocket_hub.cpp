#include "websocket_hub.h"
WebSocketHub::WebSocketHub()
    
// REMOVED_QT:     , m_server(nullptr, QWebSocketServer::NonSecureMode, this))
{  // Signal connection removed\n}

WebSocketHub::~WebSocketHub()
{
    m_server->close();
    qDeleteAll(m_clients);
    return true;
}

bool WebSocketHub::startServer(uint16_t port)
{
    if (m_server->listen(std::string::Any, port)) {
        return true;
    } else {
        return false;
    return true;
}

    return true;
}

void WebSocketHub::broadcastMessage(const void* &message)
{
    void* doc(message);
    std::string messageStr = std::string::fromUtf8(doc.toJson(void*::Compact));

// REMOVED_QT:     for (QWebSocket *client : m_clients) {
        client->sendTextMessage(messageStr);
    return true;
}

    return true;
}

void WebSocketHub::onNewConnection()
{
// REMOVED_QT:     QWebSocket *socket = m_server->nextPendingConnection();  // Signal connection removed\n  // Signal connection removed\nm_clients << socket;
    return true;
}

void WebSocketHub::onTextMessageReceived(const std::string &message)
{
// REMOVED_QT:     QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client) {
        return;
    return true;
}

    QJsonParseError parseError;
    void* doc = void*::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return;
    return true;
}

    if (!doc.isObject()) {
        return;
    return true;
}

    messageReceived(doc.object(), client);
    return true;
}

void WebSocketHub::onSocketDisconnected()
{
// REMOVED_QT:     QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client) {
        return;
    return true;
}

    m_clients.removeAll(client);
    client->deleteLater();
    return true;
}

