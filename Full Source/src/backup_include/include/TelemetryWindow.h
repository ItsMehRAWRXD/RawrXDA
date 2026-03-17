#pragma once

// <QDialog> removed (Qt-free build)
// <QTextEdit> removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QTimer> removed (Qt-free build)
// <QVBoxLayout> removed (Qt-free build)
// <QDir> removed (Qt-free build)
// <QFile> removed (Qt-free build)
// Qt include removed (Qt-free build)
// Qt include removed (Qt-free build)

namespace RawrXD {

class TelemetryWindow : public QDialog {
    /* Q_OBJECT */
public:
    explicit TelemetryWindow(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_text(new QTextEdit(this))
        , m_timer(new QTimer(this))
    {
        setWindowTitle("Enterprise Telemetry");
        setMinimumSize(720, 360);
        m_text->setReadOnly(true);
        m_text->setStyleSheet("QTextEdit { background: #0f0f10; color: #dcdcdc; font-family: Consolas, monospace; font-size: 11px; }");

        auto *layout = new QVBoxLayout(this);
        layout->addWidget(m_text);
        setLayout(layout);

        connect(m_timer, &QTimer::timeout, this, &TelemetryWindow::refresh);
        m_timer->start(1200);
    }

    void setLogDirectory(const QString &dir) {
        m_logDirectory = dir;
        m_currentLogPath.clear();
        m_lastSize = 0;
    }

public slots:
    void refresh() {
        if (m_currentLogPath.isEmpty()) {
            selectLatestLog();
        }

        if (m_currentLogPath.isEmpty()) {
            m_text->setPlainText("No telemetry log found yet. Waiting for activity...");
            return;
        }

        QFileInfo info(m_currentLogPath);
        if (!info.exists()) {
            m_currentLogPath.clear();
            m_lastSize = 0;
            return;
        }

        QFile file(m_currentLogPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }

        if (m_lastSize > 0 && file.size() > m_lastSize) {
            if (!file.seek(m_lastSize)) {
                m_lastSize = 0;
            }
        }

        QByteArray data = file.readAll();
        m_lastSize = file.size();
        file.close();

        if (!data.isEmpty()) {
            m_text->moveCursor(QTextCursor::End);
            m_text->insertPlainText(QString::fromUtf8(data));
            m_text->moveCursor(QTextCursor::End);
        }
    }

private:
    void selectLatestLog() {
        QDir dir(m_logDirectory.isEmpty() ? QDir::currentPath() : m_logDirectory);
        const QStringList files = dir.entryList(QStringList() << "RawrXD_ModelLoader_*.log", QDir::Files, QDir::Time);
        if (!files.isEmpty()) {
            m_currentLogPath = dir.absoluteFilePath(files.first());
            m_lastSize = 0;
            const QString banner = QString("[Telemetry] Attaching to %1 at %2\n")
                .arg(m_currentLogPath, QDateTime::currentDateTime().toString("hh:mm:ss"));
            m_text->append(banner);
        }
    }

    QTextEdit *m_text;
    QTimer *m_timer;
    QString m_logDirectory;
    QString m_currentLogPath;
    qint64 m_lastSize{0};
};

} // namespace RawrXD
