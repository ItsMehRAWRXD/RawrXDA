#ifndef WEBSOCKET_HUB_H
#define WEBSOCKET_HUB_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QJsonObject>

// WebSocket server embedded inside RawrXD-Agent.exe (port 5173)
class WebSocketHub : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketHub(QObject *parent = nullptr);
    ~WebSocketHub();

    // Start the server
    bool startServer(quint16 port = 5173);

    // Send message to all clients
    void broadcastMessage(const QJsonObject &message);

signals:
    // Emitted when a message is received from a client
    void messageReceived(const QJsonObject &message, QWebSocket *client);

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString &message);
    void onSocketDisconnected();

private:
    QWebSocketServer *m_server;
    QList<QWebSocket *> m_clients;
};

#endif // WEBSOCKET_HUB_H