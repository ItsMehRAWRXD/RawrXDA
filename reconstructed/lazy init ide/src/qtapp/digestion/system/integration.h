// digestion_system_integration.h
// Integration point for the Digestion Reverse Engineering System
// This header provides convenient initialization and access to the digestion system

#pragma once

#include "digestion_reverse_engineering_enterprise.h"
#include <QObject>
#include <QSharedPointer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

class DigestionSystemIntegration : public QObject {
    Q_OBJECT
public:
    // Singleton accessor
    static DigestionSystemIntegration* instance() {
        static DigestionSystemIntegration inst;
        return &inst;
    }
    
    // Get the digestion system
    DigestionReverseEngineeringSystem* system() const {
        return m_system.get();
    }
    
    // Initialize the system for a specific directory
    void initializeForDirectory(const QString& rootDir) {
        qDebug() << "[DIGESTION] Initializing for directory:" << rootDir;
        m_rootDir = rootDir;
    }
    
    // Run full pipeline on initialization
    void runFullPipeline(int maxFiles = 0, int chunkSize = 50, bool applyExtensions = false) {
        if (m_rootDir.isEmpty()) {
            qWarning() << "[DIGESTION] Root directory not set";
            return;
        }
        
        qDebug() << "[DIGESTION] Running full pipeline...";
        qDebug() << "  - Root Dir:" << m_rootDir;
        qDebug() << "  - Max Files:" << (maxFiles <= 0 ? "unlimited" : QString::number(maxFiles));
        qDebug() << "  - Chunk Size:" << chunkSize;
        qDebug() << "  - Apply Extensions:" << (applyExtensions ? "yes" : "no");
        
        // This would be called from app initialization
        emit pipelineStarted();
    }
    
    // Get last analysis report
    QJsonObject getLastReport() const {
        return m_lastReport;
    }
    
    // Save report to file
    void saveReportToFile(const QString& filePath) {
        if (m_lastReport.isEmpty()) {
            qWarning() << "[DIGESTION] No report to save";
            return;
        }
        
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonDocument doc(m_lastReport);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
            qDebug() << "[DIGESTION] Report saved to:" << filePath;
        }
    }
    
signals:
    void pipelineStarted();
    void pipelineProgress(int current, int total);
    void pipelineFinished(const QJsonObject& report);
    void errorOccurred(const QString& error);
    
private:
    DigestionSystemIntegration() : m_system(new DigestionReverseEngineeringSystem()) {}
    ~DigestionSystemIntegration() = default;
    
    QSharedPointer<DigestionReverseEngineeringSystem> m_system;
    QString m_rootDir;
    QJsonObject m_lastReport;
};

// Convenience macros
#define DIGESTION_SYSTEM DigestionSystemIntegration::instance()->system()
#define DIGESTION_INIT(dir) DigestionSystemIntegration::instance()->initializeForDirectory(dir)
#define DIGESTION_PIPELINE() DigestionSystemIntegration::instance()->runFullPipeline()
