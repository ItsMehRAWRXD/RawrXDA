#pragma once

#include <QObject>
#include <QString>

class AgenticEngine : public QObject {
    Q_OBJECT
public:
    explicit AgenticEngine(QObject* parent = nullptr);
    virtual ~AgenticEngine() = default;
    
    void initialize();
    void processMessage(const QString& message);
    
signals:
    void responseReady(const QString& response);
};
