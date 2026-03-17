#include "agentic_browser.h"
#include "../monitoring/enterprise_metrics_collector.hpp"
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextStream>
#include <QNetworkRequest>
#include <QNetworkCookieJar>
#include <QScrollBar>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <algorithm>


AgenticBrowser::AgenticBrowser(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    auto* bar = new QHBoxLayout();
    m_addressBar = new QLineEdit(this);
    m_goBtn = new QToolButton(this); m_goBtn->setText("Go");
    m_backBtn = new QToolButton(this); m_backBtn->setText("◀");
    m_fwdBtn = new QToolButton(this); m_fwdBtn->setText("▶");
    m_stopBtn = new QToolButton(this); m_stopBtn->setText("⏹");

    bar->addWidget(m_backBtn);
    bar->addWidget(m_fwdBtn);
    bar->addWidget(m_stopBtn);
    bar->addWidget(m_addressBar, 1);
    bar->addWidget(m_goBtn);

    m_view = new QTextBrowser(this);
    m_view->setOpenExternalLinks(false);
    m_view->setOpenLinks(false);

    root->addLayout(bar);
    root->addWidget(m_view, 1);

    m_nam = new QNetworkAccessManager(this);
    // Disable cookies for sandboxing
    m_nam->setCookieJar(new QNetworkCookieJar(this));
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);

    connect(m_goBtn, &QToolButton::clicked, this, &AgenticBrowser::onGoClicked);
    connect(m_backBtn, &QToolButton::clicked, this, &AgenticBrowser::onBackClicked);
    connect(m_fwdBtn, &QToolButton::clicked, this, &AgenticBrowser::onForwardClicked);
    connect(m_stopBtn, &QToolButton::clicked, this, &AgenticBrowser::onStopClicked);
    connect(m_view, &QTextBrowser::anchorClicked, this, &AgenticBrowser::onLinkClicked);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AgenticBrowser::onTimeout);
}

AgenticBrowser::~AgenticBrowser() {
    if (m_activeReply) { m_activeReply->abort(); }
}

void AgenticBrowser::setMetrics(EnterpriseMetricsCollector* metrics) { m_metrics = metrics; }

void AgenticBrowser::navigate(const QUrl& url, const NavigationOptions& opts) {
    httpGet(url, opts);
}

void AgenticBrowser::goBack() {
    if (m_backStack.empty()) return;
    auto entry = m_backStack.back();
    m_backStack.pop_back();
    if (!m_currentUrl.isEmpty()) {
        m_fwdStack.push_back({m_currentUrl, m_view->documentTitle()});
    }
    navigate(entry.url, m_lastOpts);
}

void AgenticBrowser::goForward() {
    if (m_fwdStack.empty()) return;
    auto entry = m_fwdStack.back();
    m_fwdStack.pop_back();
    if (!m_currentUrl.isEmpty()) {
        m_backStack.push_back({m_currentUrl, m_view->documentTitle()});
    }
    navigate(entry.url, m_lastOpts);
}

void AgenticBrowser::stop() {
    if (m_activeReply) m_activeReply->abort();
}

void AgenticBrowser::httpGet(const QUrl& url, const NavigationOptions& opts) {
    if (!url.isValid()) {
        logStructured("ERROR", "Invalid URL", url);
        emit navigationError(url, "Invalid URL");
        return;
    }
    m_lastOpts = opts;
    m_currentUrl = url;
    emit navigationStarted(url);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, opts.userAgent);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     opts.followRedirects ? QNetworkRequest::NoLessSafeRedirectPolicy
                                           : QNetworkRequest::ManualRedirectPolicy);

    if (m_activeReply) { m_activeReply->abort(); }

    m_timer.restart();
    m_activeReply = m_nam->get(req);
    setLoading(true);

    if (opts.timeoutMs > 0) {
        m_timeoutTimer->start(opts.timeoutMs);
    }

    connect(m_activeReply, &QNetworkReply::finished, this, &AgenticBrowser::onRequestFinished);
    connect(m_activeReply, &QNetworkReply::errorOccurred, this, &AgenticBrowser::onRequestError);
}

void AgenticBrowser::httpPost(const QUrl& url, const QByteArray& body,
                              const QList<QPair<QByteArray, QByteArray>>& headers,
                              const NavigationOptions& opts) {
    if (!url.isValid()) {
        logStructured("ERROR", "Invalid URL (POST)", url);
        emit navigationError(url, "Invalid URL");
        return;
    }
    m_lastOpts = opts;
    m_currentUrl = url;
    emit navigationStarted(url);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, opts.userAgent);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    for (const auto& h : headers) req.setRawHeader(h.first, h.second);

    if (m_activeReply) { m_activeReply->abort(); }

    m_timer.restart();
    m_activeReply = m_nam->post(req, body);
    setLoading(true);

    if (opts.timeoutMs > 0) m_timeoutTimer->start(opts.timeoutMs);

    connect(m_activeReply, &QNetworkReply::finished, this, &AgenticBrowser::onRequestFinished);
    connect(m_activeReply, &QNetworkReply::errorOccurred, this, &AgenticBrowser::onRequestError);
}

QString AgenticBrowser::extractMainText() const {
    // Very basic boilerplate removal; prioritize <main>, <article>, and text density
    const QString html = m_view->toHtml();
    QString plain = QTextDocumentFragment::fromHtml(html).toPlainText();
    // Collapse excessive whitespace
    plain.replace(QRegularExpression("\n{3,}"), "\n\n");
    return plain.trimmed();
}

QList<QUrl> AgenticBrowser::extractLinks(int maxLinks) const {
    QList<QUrl> out;
    const auto anchors = m_view->document()->toHtml().split("href=\"");
    for (int i = 1; i < anchors.size() && out.size() < maxLinks; ++i) {
        const auto end = anchors[i].indexOf('"');
        if (end > 0) {
            const auto href = anchors[i].left(end);
            const QUrl url = m_currentUrl.resolved(QUrl(href));
            if (url.isValid()) out.push_back(url);
        }
    }
    return out;
}

void AgenticBrowser::onGoClicked() {
    const QUrl url = QUrl::fromUserInput(m_addressBar->text());
    navigate(url);
}

void AgenticBrowser::onBackClicked() { goBack(); }
void AgenticBrowser::onForwardClicked() { goForward(); }
void AgenticBrowser::onStopClicked() { stop(); }

void AgenticBrowser::onLinkClicked(const QUrl& url) {
    navigate(m_currentUrl.resolved(url), m_lastOpts);
}

void AgenticBrowser::onRequestFinished() {
    if (!m_activeReply) return;
    m_timeoutTimer->stop();

    const int status = m_activeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = m_activeReply->readAll();

    const double ms = m_timer.isValid() ? m_timer.elapsed() : 0.0;

    if (m_activeReply->error() == QNetworkReply::NoError) {
        // Enforce max display size of 2GB
        static const qint64 kMaxDisplayBytes = 2ll * 1024 * 1024 * 1024;
        if (static_cast<qint64>(data.size()) >= kMaxDisplayBytes) {
            const QString notice = QString("<html><body><p><b>Content suppressed:</b> response size %1 bytes exceeds 2GB display limit.</p></body></html>")
                                     .arg(static_cast<qint64>(data.size()));
            m_view->setHtml(notice);
            m_addressBar->setText(m_currentUrl.toString());
            logStructured("ERROR", "Content suppressed (>2GB)", m_currentUrl, status, data.size(), ms);
            recordMetrics("nav_suppressed", ms, status, data.size());
            emit navigationFinished(m_currentUrl, true, status, data.size());
            setLoading(false);
            m_activeReply->deleteLater();
            m_activeReply = nullptr;
            return;
        }
        const QString title = guessTitle(data);
        const QString html = sanitizeHtml(data);
        m_view->setHtml(html);
        m_addressBar->setText(m_currentUrl.toString());
        pushHistory(m_currentUrl, title);
        logStructured("INFO", "Navigation success", m_currentUrl, status, data.size(), ms);
        recordMetrics("nav_success", ms, status, data.size());
        emit navigationFinished(m_currentUrl, true, status, data.size());
        emit contentUpdated(m_currentUrl, title);
    } else {
        logStructured("ERROR", m_activeReply->errorString(), m_currentUrl, status, data.size(), ms);
        recordMetrics("nav_error", ms, status, data.size());
        emit navigationError(m_currentUrl, m_activeReply->errorString());
        emit navigationFinished(m_currentUrl, false, status, data.size());
    }

    setLoading(false);
    m_activeReply->deleteLater();
    m_activeReply = nullptr;
}

void AgenticBrowser::onRequestError(QNetworkReply::NetworkError) {
    // Handled in finished(); keep for completeness
}

void AgenticBrowser::onTimeout() {
    if (m_activeReply) {
        m_activeReply->abort();
        logStructured("ERROR", "Request timeout", m_currentUrl);
        recordMetrics("nav_timeout", m_timer.elapsed(), 0, 0);
        emit navigationError(m_currentUrl, "Timeout");
        setLoading(false);
    }
}

void AgenticBrowser::setLoading(bool loading) {
    m_goBtn->setEnabled(!loading);
    m_stopBtn->setEnabled(loading);
}

void AgenticBrowser::pushHistory(const QUrl& url, const QString& title) {
    if (!m_backStack.empty() && m_backStack.back().url == url) return;
    if (!m_currentUrl.isEmpty()) {
        m_backStack.push_back({url, title});
        // Bound history to prevent unbounded growth
        while (m_backStack.size() > 200) m_backStack.pop_front();
    }
    m_fwdStack.clear();
}

QString AgenticBrowser::sanitizeHtml(const QByteArray& html) const {
    // Remove <script>, <iframe>, and on* attributes for safety
    QString s = QString::fromUtf8(html);
    s.remove(QRegularExpression("<script[\n\r\s\S]*?</script>", QRegularExpression::CaseInsensitiveOption));
    s.remove(QRegularExpression("<iframe[\n\r\s\S]*?</iframe>", QRegularExpression::CaseInsensitiveOption));
    s.replace(QRegularExpression(" on[a-zA-Z]+=\"[^\"]*\""), "");
    return s;
}

QString AgenticBrowser::guessTitle(const QByteArray& html) const {
    const QString s = QString::fromUtf8(html);
    QRegularExpression re("<title>(.*?)</title>", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto m = re.match(s);
    if (m.hasMatch()) return m.captured(1).trimmed();
    return m_currentUrl.host().isEmpty() ? m_currentUrl.toString() : m_currentUrl.host();
}

void AgenticBrowser::recordMetrics(const QString& event, double latencyMs, int status, qint64 bytes) const {
    if (!m_metrics) return;
    m_metrics->recordCounter(QString("agentic_browser_%1_count").arg(event));
    m_metrics->recordHistogram(QString("agentic_browser_%1_latency_ms").arg(event), latencyMs);
    m_metrics->recordMetric("agentic_browser_bytes", static_cast<double>(bytes), {{"event", event}});
    if (status) m_metrics->recordMetric("agentic_browser_http_status", status);
}

void AgenticBrowser::logStructured(const char* level, const QString& msg,
                                   const QUrl& url,
                                   int status,
                                   qint64 bytes,
                                   double latencyMs) const {
    QJsonObject o{
        {"ts", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"subsystem", "agentic_browser"},
        {"level", level},
        {"message", msg},
        {"url", url.toString()},
        {"status", status},
        {"bytes", static_cast<double>(bytes)},
        {"latency_ms", latencyMs}
    };
    const QByteArray line = QJsonDocument(o).toJson(QJsonDocument::Compact);
    if (QString::fromLatin1(level).toUpper() == "ERROR")
        qWarning().noquote() << line;
    else
        qInfo().noquote() << line;
}
