#pragma once

#include <QWidget>
#include <QString>

class QTextEdit;
class QLineEdit;

class ChatInterface : public QWidget {
    Q_OBJECT
public:
    explicit ChatInterface(QWidget* parent = nullptr);
    
    void addMessage(const QString& sender, const QString& message);
    
public slots:
    void displayResponse(const QString& response);
    void focusInput();
    void sendMessage();
    
signals:
    void messageSent(const QString& message);
    
private:
    QTextEdit* message_history_;
    QLineEdit* message_input_;
};
