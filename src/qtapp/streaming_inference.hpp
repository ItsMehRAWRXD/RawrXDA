#pragma once
#include <QObject>
#include <QTextCursor>

class QPlainTextEdit;

/**
 * @brief Token-by-token streaming output for inference results
 * 
 * Handles real-time streaming of inference tokens to a console widget.
 * All UI updates are queued to the main thread for thread safety.
 */
class StreamingInference : public QObject {
    Q_OBJECT
public:
    explicit StreamingInference(QPlainTextEdit* target, QObject* parent = nullptr);

public slots:
    void startStream(qint64 reqId, const QString& prompt);
    void pushToken(const QString& token);        // called from worker
    void finishStream();

private:
    QPlainTextEdit* m_out;
    qint64          m_reqId{0};
    QString         m_buffer;
};
