#pragma once

/********************************************************************
 *  gguf_proxy_server.hpp
 *
 *  A thin TCP‑proxy that sits between the IDE‑agent (ModelInvoker)
 *  and a GGUF model server.  It forwards the client request, lets
 *  AgentHotPatcher inspect/patch the model output and then sends the
 *  (possibly corrected) reply back to the client.
 *
 *  NOTE:  The implementation lives in gguf_proxy_server.cpp.
 ********************************************************************/

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QByteArray>
#include <QJsonObject>
#include <QMutex>
#include <memory>

class AgentHotPatcher;          // forward declaration – defined elsewhere

/**
 * @brief Small helper that groups everything we need for a single client.
 *
 * The struct is deliberately POD‑like – it only stores raw pointers owned by
 * the server (the server is the parent of both sockets).  All buffers are
 * cleared automatically when the socket is destroyed.
 */
struct ClientConnection
{
    QTcpSocket* clientSocket = nullptr;          ///< socket that talks to the IDE‑agent
    QTcpSocket* ggufSocket   = nullptr;          ///< socket that talks to the real GGUF backend
    QByteArray   requestBuffer;                  ///< data received from the client (not yet sent)
    QByteArray   responseBuffer;                 ///< data received from GGUF (awaiting patch)
};

/**
 * @class GGUFProxyServer
 *
 * Inherits QTcpServer; each incoming client connection spawns a
 * `ClientConnection` entry.  The server forwards traffic to the real GGUF
 * endpoint, passes the raw model output through `AgentHotPatcher`,
 * and finally writes the corrected response back.
 *
 * The class is fully Qt‑signal‑slot aware – all public slots are queued so
 * they are safe even if the hot‑patcher lives in a different thread.
 */
class GGUFProxyServer : public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(GGUFProxyServer)

public:
    explicit GGUFProxyServer(QObject* parent = nullptr);
    ~GGUFProxyServer() override;

    /** Configuration --------------------------------------------------- */
    void initialize(int listenPort,
                    AgentHotPatcher* hotPatcher,
                    const QString& ggufEndpoint);   ///< must be called before startServer()

    /** Lifecycle -------------------------------------------------------- */
    bool startServer();          ///< bind to m_listenPort and listen
    void stopServer();           ///< close all client/gguf sockets and stop listening
    bool isListening() const;          ///< same as QTcpServer::isListening()

    /** Statistics ------------------------------------------------------- */
    QJsonObject getServerStatistics() const;   ///< snapshot of counters

    /** Tuning ----------------------------------------------------------- */
    void setConnectionPoolSize(int size);   ///< future‑proof – currently unused
    void setConnectionTimeout(int ms);      ///< socket connect timeout (ms)

    /** Helpers (mostly for tests) -------------------------------------- */
    QTcpSocket* getGGUFConnection();        ///< allocate a fresh socket (caller must return it)
    void        returnGGUFConnection(QTcpSocket* socket);
    QString     parseIncomingRequest(const QByteArray& data);   ///< hook for custom HTTP parsing

signals:
    /** Emitted only for diagnostics – not used inside the current code. */
    void serverStarted(int port);
    void serverStopped();

private slots:
    /** Socket‑side event handlers */
    void onClientDataReceived();
    void onClientDisconnected();
    void onGGUFDataReceived();
    void onGGUFError();
    void onGGUFDisconnected();               // <-- new slot (see bug‑fix below)

private:
    /** Overridden from QTcpServer – creates a ClientConnection */
    void incomingConnection(qintptr socketDescriptor) override;

    /** Core forwarding / patching helpers */
    void forwardToGGUF(qintptr socketDescriptor);
    void processGGUFResponse(qintptr socketDescriptor);
    void sendResponseToClient(qintptr socketDescriptor,
                              const QString& response);

    /* -----------------------------------------------------------------
       Member data
       ----------------------------------------------------------------- */
    int                 m_listenPort = 0;
    QString             m_ggufEndpoint;               ///< e.g. "localhost:11434"
    AgentHotPatcher*    m_hotPatcher  = nullptr;       ///< non‑owning – created elsewhere

    QMap<qintptr, std::unique_ptr<ClientConnection>> m_connections;
    int                 m_connectionPoolSize = 10;    ///< for future thread‑pool upgrades
    int                 m_connectionTimeout  = 5000;  ///< ms for socket::connectToHost

    /* statistics – atomic updates protected by m_statsMutex */
    mutable QMutex      m_statsMutex;
    qint64              m_requestsProcessed          = 0;
    qint64              m_hallucinationsCorrected   = 0;
    qint64              m_navigationErrorsFixed      = 0;
    int                 m_activeConnections         = 0;
};

#endif // GGUF_PROXY_SERVER_HPP
