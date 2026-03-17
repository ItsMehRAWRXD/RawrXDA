#pragma once

#include <QWidget>
#include <QString>

class QTextEdit;

class ChatInterface : public QWidget {
    Q_OBJECT
public:
    explicit ChatInterface(QWidget* parent = nullptr);
    
    void addMessage(const QString& sender, const QString& message);
    
private:
    QTextEdit* message_history_;
};
