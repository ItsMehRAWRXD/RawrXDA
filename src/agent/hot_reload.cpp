#include "hot_reload.hpp"
#include <QProcess>
#include <QDir>
#include <QTimer>
#include <QDebug>

HotReload::HotReload(QObject* parent) : QObject(parent) {}

bool HotReload::reloadQuant(const QString& quantType) {
    qDebug() << "Hot-reloading quantization:" << quantType;
    
    // Step 1: Rebuild only the quant library
    QProcess buildProc;
    buildProc.start("cmake", {
        "--build", "build", 
        "--config", "Release", 
        "--target", "quant_ladder_avx2"
    });
    
    if (!buildProc.waitForFinished(30000)) {
        emit reloadFailed("Build timeout for quant_ladder_avx2");
        return false;
    }
    
    if (buildProc.exitCode() != 0) {
        QString error = QString::fromUtf8(buildProc.readAllStandardError());
        qWarning() << "Quant rebuild failed:" << error;
        emit reloadFailed(QString("Build failed: %1").arg(error));
        return false;
    }
    
    qDebug() << "Quant library rebuilt successfully";
    
    // Step 2: Signal upper layer to re-map tensors
    // The actual reload happens in InferenceEngine when it receives this signal
    emit quantReloaded(quantType);
    
    return true;
}

bool HotReload::reloadModule(const QString& moduleName) {
    qDebug() << "Hot-reloading module:" << moduleName;
    
    // Build specific target
    QProcess buildProc;
    buildProc.start("cmake", {
        "--build", "build", 
        "--config", "Release", 
        "--target", moduleName
    });
    
    if (!buildProc.waitForFinished(60000)) {
        emit reloadFailed(QString("Build timeout for %1").arg(moduleName));
        return false;
    }
    
    if (buildProc.exitCode() != 0) {
        QString error = QString::fromUtf8(buildProc.readAllStandardError());
        qWarning() << "Module rebuild failed:" << error;
        emit reloadFailed(QString("Build failed: %1").arg(error));
        return false;
    }
    
    qDebug() << "Module rebuilt successfully:" << moduleName;
    emit moduleReloaded(moduleName);
    
    return true;
}
