#pragma once
#include <QWidget>
#include <QUrl>
#include <QElapsedTimer>
#include <QTextBrowser>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QRegularExpression>
#include <optional>
#include <deque>

class EnterpriseMetricsCollector;

// A lightweight, sandboxed agentic browser panel using QtNetwork + QTextBrowser.
// - No JS execution. No plugins. HTML is sanitized and rendered as rich text.
// - Provides agent-facing API: navigate, back/forward, form POST, link extraction.
// - Emits detailed structured logs and latency metrics per navigation.
class AgenticBrowser : public QWidget {
    Q_OBJECT
public:
    struct NavigationOptions {
        int timeoutMs = 15000;
        QByteArray userAgent = QByteArray("RawrXD-AgenticBrowser/1.0");
        bool followRedirects = true;
        bool allowCookies = false; // currently ignored; cookies disabled
    };

    explicit AgenticBrowser(QWidget* parent = nullptr);
    ~AgenticBrowser() override;

    // Agent API
    void navigate(const QUrl& url, const NavigationOptions& opts = {});
    void goBack();
    void goForward();
    void stop();

    // Programmatic interactions
    void httpGet(const QUrl& url, const NavigationOptions& opts = {});
    void httpPost(const QUrl& url, const QByteArray& body,
                  const QList<QPair<QByteArray, QByteArray>>& headers = {},
                  const NavigationOptions& opts = {});

    // Extraction helpers
    QString extractMainText() const;
    QList<QUrl> extractLinks(int maxLinks = 200) const;

    // Observability
    void setMetrics(EnterpriseMetricsCollector* metrics);

signals:
    void navigationStarted(const QUrl& url);
    void navigationFinished(const QUrl& url, bool success, int httpStatus, qint64 bytes);
    void navigationError(const QUrl& url, const QString& errorString);
    void contentUpdated(const QUrl& url, const QString& title);

private slots:
    void onGoClicked();
    void onBackClicked();
    void onForwardClicked();
    void onStopClicked();
    void onLinkClicked(const QUrl& url);
    void onRequestFinished();
    void onRequestError(QNetworkReply::NetworkError code);
    void onTimeout();

private:
    struct HistoryEntry { QUrl url; QString title; };

    // UI
    QLineEdit* m_addressBar = nullptr;
    QToolButton* m_goBtn = nullptr;
    QToolButton* m_backBtn = nullptr;
    QToolButton* m_fwdBtn = nullptr;
    QToolButton* m_stopBtn = nullptr;
    QTextBrowser* m_view = nullptr;

    // Net
    QNetworkAccessManager* m_nam = nullptr;
    QPointer<QNetworkReply> m_activeReply;
    QTimer* m_timeoutTimer = nullptr;

    // State
    NavigationOptions m_lastOpts;
    QElapsedTimer m_timer;
    std::deque<HistoryEntry> m_backStack;
    std::deque<HistoryEntry> m_fwdStack;
    QUrl m_currentUrl;
    EnterpriseMetricsCollector* m_metrics = nullptr;

    // Helpers
    void setLoading(bool loading);
    void pushHistory(const QUrl& url, const QString& title);
    QString sanitizeHtml(const QByteArray& html) const;
    QString guessTitle(const QByteArray& html) const;
    void recordMetrics(const QString& event,
                       double latencyMs,
                       int status,
                       qint64 bytes) const;
    void logStructured(const char* level, const QString& msg,
                       const QUrl& url = {},
                       int status = 0,
                       qint64 bytes = 0,
                       double latencyMs = 0.0) const;
};
