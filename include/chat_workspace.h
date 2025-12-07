#pragma once

#include <QWidget>
#include <QString>

class ChatWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit ChatWorkspace(QWidget* parent = nullptr);
    void initialize();
    
signals:
    void commandIssued(const QString& command);
};
