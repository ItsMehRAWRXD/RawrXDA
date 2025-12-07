#pragma once
#include <QString>
#include <QObject>

class HotReload : public QObject {
    Q_OBJECT
public:
    explicit HotReload(QObject* parent = nullptr);
    
    // Reload quantization library on-the-fly
    bool reloadQuant(const QString& quantType);
    
    // Reload specific module
    bool reloadModule(const QString& moduleName);
    
signals:
    void quantReloaded(const QString& quantType);
    void moduleReloaded(const QString& moduleName);
    void reloadFailed(const QString& error);
};
