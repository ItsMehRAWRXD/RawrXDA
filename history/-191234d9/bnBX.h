#pragma once

#include "TestExecutor.h"
#include <QWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QLineEdit>
#include <QToolBar>

namespace RawrXD {

class TestOutputPanel : public QWidget {
    Q_OBJECT
public:
    explicit TestOutputPanel(QWidget* parent = nullptr);

    void clear();
    void showResult(const TestExecutionResult& result);
    void appendLiveOutput(const QString& testId, const QString& text);

signals:
    void linkActivated(const QUrl& url);

private:
    QString formatResultHtml(const TestExecutionResult& result);
    static QString escapeHtml(const QString& s);
    static QString linkifyStackTraces(const QString& text);

    QTextBrowser* m_view{};
    QToolBar* m_toolbar{};
    QLineEdit* m_filterEdit{};
    QPushButton* m_copyBtn{};
};

} // namespace RawrXD
