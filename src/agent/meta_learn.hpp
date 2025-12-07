#pragma once
#include <QString>
#include <QObject>
#include <QHash>
#include <QList>
#include <QJsonArray>

struct PerfRecord {
    QString quant;
    QString kernel;
    QString gpu;
    QString hardware;
    double tps;      // tokens per second
    double ppl;      // perplexity
    qint64 timestamp;
};

class MetaLearn : public QObject {
    Q_OBJECT
public:
    explicit MetaLearn(QObject* parent = nullptr);
    
    // Record performance metrics to database
    bool record(const QString& quant,
                const QString& kernel,
                const QString& gpu,
                double tps,
                double ppl);
    
    // Auto-apply the best quantization/kernel for this machine
    bool autoTuneQuant();
    bool autoTuneKernel();
    
    // Suggestions without side effects
    QString suggestQuant() const;
    QString suggestKernel() const;
    
    // Get performance history
    QList<PerfRecord> getHistory(const QString& quant = QString()) const;
    
    // Load database from disk
    bool loadDatabase();
    
    // Save database to disk
    bool saveDatabase() const;

    // Hardware fingerprint helper
    QString gpuHash() const;

    // Lightweight static helper for callers needing raw records
    static QJsonArray loadDB(bool* ok = nullptr);
    
signals:
    void recordAdded(const PerfRecord& record);
    void suggestionReady(const QString& quant);
    void kernelSuggestionReady(const QString& kernel);
    
private:
    QString resolveGpuLabel(const QString& explicitGpu) const;
    bool computeQuantSuggestion(QString* bestQuant,
                                double* avgTps,
                                double* avgPpl) const;
    bool computeKernelSuggestion(QString* bestKernel,
                                 double* avgTps) const;
    QString hardwareKey() const;
    QList<PerfRecord> m_records;
    QString m_dbPath;
    QString m_lastQuantSuggestion;
    QString m_lastKernelSuggestion;
};
