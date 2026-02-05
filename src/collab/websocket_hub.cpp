#include "websocket_hub.h"
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

WebSocketHub::WebSocketHub(QObject *parent)
    : QObject(parent)
    , m_server(new QWebSocketServer(QStringLiteral("Collab Server"), QWebSocketServer::NonSecureMode, this))
{
    connect(m_server, &QWebSocketServer::newConnection, this, &WebSocketHub::onNewConnection);
}

WebSocketHub::~WebSocketHub()
{
    m_server->close();
    qDeleteAll(m_clients);
}

bool WebSocketHub::startServer(quint16 port)
{
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "WebSocket server started on port" << port;
        return true;
    } else {
        qWarning() << "Failed to start WebSocket server:" << m_server->errorString();
        return false;
    }
}

void WebSocketHub::broadcastMessage(const QJsonObject &message)
{
    QJsonDocument doc(message);
    QString messageStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    for (QWebSocket *client : m_clients) {
        client->sendTextMessage(messageStr);
    }
}

void WebSocketHub::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QWebSocket::textMessageReceived, this, &WebSocketHub::onTextMessageReceived);
    connect(socket, &QWebSocket::disconnected, this, &WebSocketHub::onSocketDisconnected);
    m_clients << socket;
    qDebug() << "New client connected";
}

void WebSocketHub::onTextMessageReceived(const QString &message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse JSON message:" << parseError.errorString();
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "Received JSON message is not an object";
        return;
    }

    emit messageReceived(doc.object(), client);
}

void WebSocketHub::onSocketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client) {
        return;
    }

    m_clients.removeAll(client);
    client->deleteLater();
    qDebug() << "Client disconnected";
}