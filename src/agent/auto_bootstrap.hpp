#pragma once
#include <QString>
#include <QObject>
#include <QJsonArray>

class AutoBootstrap : public QObject {
    Q_OBJECT
public:
    static AutoBootstrap* instance();
    static void installZeroTouch();
    static void startWithWish(const QString& wish);
    
    // Start autonomy loop with zero-touch input
    void start();
    
signals:
    void wishReceived(const QString& wish);
    void planGenerated(const QString& planSummary);
    void executionStarted();
    void executionCompleted(bool success);
    
private:
    explicit AutoBootstrap(QObject* parent = nullptr);
    
    // Grab wish from env-var > clipboard > dialog
    QString grabWish();
    
    // Safety gate to prevent dangerous commands
    bool safetyGate(const QString& wish);
    
    void startWithWishInternal(const QString& wish);
    void executePlan(const QString& wish, const QJsonArray& plan);
    
    static AutoBootstrap* s_instance;
};
