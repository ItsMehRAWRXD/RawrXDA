// Lightweight HTTP server compatibility layer to avoid Qt HttpServer dependency
#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>
#include <QPair>
#include <functional>

// Minimal request wrapper
class QHttpServerRequest {
public:
    class Header {
    public:
        Header() = default;
        Header(QByteArray n, QByteArray v) : name_(std::move(n)), value_(std::move(v)) {}
        QByteArray name() const { return name_; }
        QByteArray value() const { return value_; }
    private:
        QByteArray name_;
        QByteArray value_;
    };

    using HeaderList = QVector<Header>;

    class MethodWrapper {
    public:
        MethodWrapper() = default;
        explicit MethodWrapper(const QString& v) : value(v) {}
        QString toString() const { return value; }
    private:
        QString value;
    };

    QHttpServerRequest() = default;
    QHttpServerRequest(const MethodWrapper& m, const QUrl& u, const QByteArray& b,
                       const HeaderList& h, const QHostAddress& addr)
        : method_(m), url_(u), body_(b), headers_(h), remote_(addr) {}

    MethodWrapper method() const { return method_; }
    QUrl url() const { return url_; }
    QByteArray body() const { return body_; }
    HeaderList headers() const { return headers_; }
    QHostAddress remoteAddress() const { return remote_; }

private:
    MethodWrapper method_;
    QUrl url_;
    QByteArray body_;
    HeaderList headers_;
    QHostAddress remote_;
};

// Minimal responder
class QHttpServerResponder {
public:
    enum class StatusCode {
        Ok = 200,
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        TooManyRequests = 429,
        InternalServerError = 500
    };

    explicit QHttpServerResponder(QTcpSocket* socket = nullptr) : socket_(socket) {}

    void writeJson(const QJsonObject& obj, StatusCode code = StatusCode::Ok) {
        if (!socket_) return;
        const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        QByteArray response;
        response += "HTTP/1.1 " + QByteArray::number(static_cast<int>(code)) + "\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += payload;
        socket_->write(response);
        socket_->disconnectFromHost();
    }

private:
    QTcpSocket* socket_ = nullptr;
};

// Lightweight response wrapper for compatibility with REST API helpers
class QHttpServerResponse {
public:
    using StatusCode = QHttpServerResponder::StatusCode;

    QHttpServerResponse() = default;
    QHttpServerResponse(const QJsonObject& obj, StatusCode code = StatusCode::Ok)
        : payload(QJsonDocument(obj).toJson(QJsonDocument::Compact)), status(code) {}

    QByteArray payload;
    StatusCode status = StatusCode::Ok;
};

// Minimal HTTP server with basic routing
class QHttpServer : public QObject {
    Q_OBJECT
public:
    using Handler = std::function<void(const QHttpServerRequest&, QHttpServerResponder&)>;

    explicit QHttpServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server_, &QTcpServer::newConnection, this, &QHttpServer::onNewConnection);
    }

    bool listen(const QHostAddress& address, quint16 port) {
        return server_.listen(address, port);
    }

    void close() { server_.close(); }

    template <typename Functor>
    void route(const QString& path, Functor&& handler) {
        routes_.push_back({path, Handler(handler)});
    }

private slots:
    void onNewConnection() {
        while (auto* socket = server_.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                handleConnection(socket);
            });
        }
    }

private:
    void handleConnection(QTcpSocket* socket) {
        const QByteArray data = socket->readAll();
        QList<QByteArray> lines = data.split('\n');
        if (lines.isEmpty()) { socket->disconnectFromHost(); return; }

        // Parse request line
        QByteArray requestLine = lines.takeFirst().trimmed();
        QList<QByteArray> parts = requestLine.split(' ');
        if (parts.size() < 2) { socket->disconnectFromHost(); return; }
        QString methodStr = QString::fromUtf8(parts[0]);
        QString pathStr = QString::fromUtf8(parts[1]);

        // Parse headers
        QHttpServerRequest::HeaderList headers;
        QByteArray body;
        bool inHeaders = true;
        for (const QByteArray& line : lines) {
            QByteArray trimmed = line.trimmed();
            if (inHeaders) {
                if (trimmed.isEmpty()) { inHeaders = false; continue; }
                int colon = trimmed.indexOf(':');
                if (colon > 0) {
                    headers.push_back(QHttpServerRequest::Header(trimmed.left(colon), trimmed.mid(colon+1).trimmed()));
                }
            } else {
                body += line;
            }
        }

        QUrl url(pathStr);
        QHttpServerRequest request(QHttpServerRequest::MethodWrapper(methodStr), url, body, headers, socket->peerAddress());
        QHttpServerResponder responder(socket);

        // Simple routing: exact match else wildcard "<path>"
        for (const auto& r : routes_) {
            if (r.first == pathStr || r.first == "/<path>") {
                r.second(request, responder);
                return;
            }
        }
        if (!routes_.empty()) {
            routes_.back().second(request, responder);
        }
    }

    QTcpServer server_;
    QVector<QPair<QString, Handler>> routes_;
};
