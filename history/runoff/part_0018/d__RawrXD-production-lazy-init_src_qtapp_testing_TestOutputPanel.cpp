#include "TestOutputPanel.h"
#include <QVBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QTextDocument>
#include <QTextCursor>
#include <QRegularExpression>

namespace RawrXD {

TestOutputPanel::TestOutputPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    m_toolbar = new QToolBar(this);
    m_filterEdit = new QLineEdit(m_toolbar);
    m_filterEdit->setPlaceholderText("Filter output...");
    m_copyBtn = new QPushButton("Copy All", m_toolbar);

    m_toolbar->addWidget(m_filterEdit);
    m_toolbar->addWidget(m_copyBtn);

    m_view = new QTextBrowser(this);
    m_view->setOpenLinks(false);
    connect(m_view, &QTextBrowser::anchorClicked, this, &TestOutputPanel::linkActivated);

    layout->addWidget(m_toolbar);
    layout->addWidget(m_view, 1);

    connect(m_filterEdit, &QLineEdit::textChanged, [this](const QString& t){
        // Simple filter: rebuild to show only matching lines
        const QString html = m_view->toHtml();
        const QStringList lines = html.split('<' + QString("br/") + '>');
        QString filtered;
        for (const QString& ln : lines) {
            if (t.isEmpty() || ln.contains(t, Qt::CaseInsensitive)) filtered += ln + "<br/>";
        }
        m_view->setHtml(filtered);
    });
    connect(m_copyBtn, &QPushButton::clicked, [this]{
        QApplication::clipboard()->setText(m_view->toPlainText());
    });
}

void TestOutputPanel::clear() {
    m_view->clear();
}

void TestOutputPanel::appendLiveOutput(const QString& /*testId*/, const QString& text) {
    QString h = escapeHtml(text);
    h = linkifyStackTraces(h);
    m_view->append(h);
}

void TestOutputPanel::showResult(const TestExecutionResult& result) {
    m_view->append("<hr/>");
    m_view->append(formatResultHtml(result));
}

QString TestOutputPanel::escapeHtml(const QString& s) {
    QString r = s;
    r.replace('&', "&amp;");
    r.replace('<', "&lt;");
    r.replace('>', "&gt;");
    return r;
}

QString TestOutputPanel::linkifyStackTraces(const QString& text) {
    QString out = text;
    // Patterns: 
    //  - /path/file.cpp:123
    //  - C:\path\file.cpp(123)
    QRegularExpression rx1("([A-Za-z]:\\\\[^\n\r\t:()]+\\\\[^\n\r\t:()]+)\\((\\d+)\\)");
    out.replace(rx1, "<a href=\"file://\\1:\\2\">\\1(\\2)</a>");

    QRegularExpression rx2("([/\\][^\n\r\t:()]+\.[a-zA-Z]+):(\\d+)");
    out.replace(rx2, "<a href=\"file://\\1:\\2\">\\1:\\2</a>");
    return out;
}

QString TestOutputPanel::formatResultHtml(const TestExecutionResult& result) {
    const QString status = [s=result.testCase.status]{
        switch (s) {
            case TestStatus::Passed: return QString("<span style='color:#0a0'>PASSED</span>");
            case TestStatus::Failed: return QString("<span style='color:#a00'>FAILED</span>");
            case TestStatus::Skipped: return QString("<span style='color:#c80'>SKIPPED</span>");
            case TestStatus::Timeout: return QString("<span style='color:#c50'>TIMEOUT</span>");
            case TestStatus::Error: return QString("<span style='color:#900'>ERROR</span>");
            case TestStatus::Running: return QString("<span style='color:#08c'>RUNNING</span>");
            default: return QString("<span>NOT RUN</span>");
        }
    }();

    QString html;
    html += QString("<b>%1</b> (%2) — %3 ms").arg(escapeHtml(result.testCase.name)).arg((int)result.testCase.framework).arg((int)result.durationMs);
    html += QString("<div>Status: %1</div>").arg(status);
    if (!result.output.isEmpty()) {
        html += "<pre style='white-space:pre-wrap;'>" + linkifyStackTraces(escapeHtml(result.output)) + "</pre>";
    }
    if (!result.failureMessage.isEmpty()) {
        html += "<div style='color:#a00;font-weight:bold'>Failure: " + escapeHtml(result.failureMessage) + "</div>";
    }
    return html;
}

} // namespace RawrXD
