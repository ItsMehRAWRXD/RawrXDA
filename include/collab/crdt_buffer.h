#ifndef CRDT_BUFFER_H
#define CRDT_BUFFER_H

#include <QObject>
#include <QString>

// CRDT text buffer → conflict-free multi-user edits
class CRDTBuffer : public QObject
{
    Q_OBJECT

public:
    explicit CRDTBuffer(QObject *parent = nullptr);

    // Apply remote operation
    void applyRemoteOperation(const QString &operation);

    // Get current text
    QString getText() const;

    // Insert text at position
    void insertText(int position, const QString &text);

    // Delete text from position
    void deleteText(int position, int length);

signals:
    // Emitted when text changes
    void textChanged(const QString &newText);

    // Emitted when operation needs to be sent to other clients
    void operationGenerated(const QString &operation);

private:
    QString m_text;
};

#endif // CRDT_BUFFER_H